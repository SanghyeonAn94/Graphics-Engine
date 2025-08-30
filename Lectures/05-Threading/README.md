# 05. Multi-Threading & Job System

## 학습 목표
- 게임 엔진에서의 병렬 프로그래밍 전략 수립
- Lock-free 자료구조 설계 및 구현
- Work-Stealing 기반 Job System 구축
- Thread-safe한 ECS 통합

## 강의 내용

### 5.1 게임 엔진의 병렬성
- Frame 기반 파이프라인
- CPU/GPU 병렬 처리
- 시스템 간 의존성 관리
- 동기화 포인트 최소화

### 5.2 Lock-Free Programming
```cpp
// Lock-free Ring Buffer
template<typename T, size_t Capacity>
class LockFreeRingBuffer {
private:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
    alignas(64) std::atomic<size_t> head{0};
    alignas(64) std::atomic<size_t> tail{0};
    alignas(64) std::array<T, Capacity> buffer;
    
public:
    bool push(T&& item) {
        const size_t current_tail = tail.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & (Capacity - 1);
        
        if (next_tail == head.load(std::memory_order_acquire)) {
            return false; // Full
        }
        
        buffer[current_tail] = std::move(item);
        tail.store(next_tail, std::memory_order_release);
        return true;
    }
    
    bool pop(T& item) {
        const size_t current_head = head.load(std::memory_order_relaxed);
        
        if (current_head == tail.load(std::memory_order_acquire)) {
            return false; // Empty
        }
        
        item = std::move(buffer[current_head]);
        head.store((current_head + 1) & (Capacity - 1), std::memory_order_release);
        return true;
    }
};
```

### 5.3 Job System Architecture
```cpp
// Job 인터페이스
class Job {
public:
    virtual ~Job() = default;
    virtual void execute() = 0;
    virtual bool canRunInParallel() const { return true; }
};

// Lambda Job Wrapper
template<typename Callable>
class LambdaJob : public Job {
private:
    Callable callable;
    
public:
    explicit LambdaJob(Callable&& c) : callable(std::move(c)) {}
    
    void execute() override {
        callable();
    }
};
```

### 5.4 Work-Stealing Queue
```cpp
class WorkStealingQueue {
private:
    alignas(64) std::atomic<size_t> top{0};
    alignas(64) std::atomic<size_t> bottom{0};
    std::vector<std::unique_ptr<Job>> jobs;
    std::mutex resize_mutex;
    
public:
    void push(std::unique_ptr<Job> job) {
        const size_t b = bottom.load(std::memory_order_relaxed);
        jobs[b & (jobs.size() - 1)] = std::move(job);
        bottom.store(b + 1, std::memory_order_release);
    }
    
    std::unique_ptr<Job> pop() {
        const size_t b = bottom.load(std::memory_order_relaxed) - 1;
        bottom.store(b, std::memory_order_relaxed);
        
        const size_t t = top.load(std::memory_order_acquire);
        
        if (t <= b) {
            auto job = std::move(jobs[b & (jobs.size() - 1)]);
            
            if (t == b) {
                if (!top.compare_exchange_strong(t, t + 1)) {
                    job = nullptr;
                }
                bottom.store(b + 1, std::memory_order_relaxed);
            }
            return job;
        } else {
            bottom.store(b + 1, std::memory_order_relaxed);
            return nullptr;
        }
    }
    
    std::unique_ptr<Job> steal() {
        const size_t t = top.load(std::memory_order_acquire);
        const size_t b = bottom.load(std::memory_order_acquire);
        
        if (t < b) {
            auto job = jobs[t & (jobs.size() - 1)];
            if (top.compare_exchange_strong(t, t + 1)) {
                return job;
            }
        }
        return nullptr;
    }
};
```

### 5.5 Thread Pool과 Scheduler
```cpp
class JobSystem {
private:
    std::vector<std::thread> workers;
    std::vector<std::unique_ptr<WorkStealingQueue>> queues;
    std::atomic<bool> shutdown{false};
    
    thread_local size_t worker_id = SIZE_MAX;
    
public:
    JobSystem(size_t thread_count = std::thread::hardware_concurrency()) {
        queues.resize(thread_count);
        workers.reserve(thread_count);
        
        for (size_t i = 0; i < thread_count; ++i) {
            queues[i] = std::make_unique<WorkStealingQueue>();
            workers.emplace_back([this, i]() { workerLoop(i); });
        }
    }
    
    template<typename Callable>
    auto submit(Callable&& callable) -> std::future<decltype(callable())> {
        using ReturnType = decltype(callable());
        
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::forward<Callable>(callable)
        );
        
        auto future = task->get_future();
        
        auto job = std::make_unique<LambdaJob>([task]() { (*task)(); });
        
        if (worker_id != SIZE_MAX) {
            queues[worker_id]->push(std::move(job));
        } else {
            // Submit to random queue
            size_t target = std::random_device{}() % queues.size();
            queues[target]->push(std::move(job));
        }
        
        return future;
    }
    
private:
    void workerLoop(size_t id) {
        worker_id = id;
        
        while (!shutdown.load()) {
            std::unique_ptr<Job> job = nullptr;
            
            // Try own queue first
            job = queues[id]->pop();
            
            // Try stealing from other queues
            if (!job) {
                for (size_t i = 1; i < queues.size(); ++i) {
                    size_t target = (id + i) % queues.size();
                    job = queues[target]->steal();
                    if (job) break;
                }
            }
            
            if (job) {
                job->execute();
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }
};
```

### 5.6 ECS와 Threading 통합
```cpp
// Thread-safe System 실행
class ThreadedSystemManager {
private:
    JobSystem& jobSystem;
    std::vector<std::unique_ptr<System>> systems;
    
public:
    void updateSystems(ECSWorld& world, float deltaTime) {
        std::vector<std::future<void>> futures;
        
        // Phase 1: Independent systems
        for (auto& system : getIndependentSystems()) {
            futures.push_back(jobSystem.submit([&system, &world, deltaTime]() {
                system->update(world, deltaTime);
            }));
        }
        
        // Wait for Phase 1
        for (auto& future : futures) {
            future.wait();
        }
        
        // Phase 2: Dependent systems
        futures.clear();
        for (auto& system : getDependentSystems()) {
            futures.push_back(jobSystem.submit([&system, &world, deltaTime]() {
                system->update(world, deltaTime);
            }));
        }
        
        // Wait for Phase 2
        for (auto& future : futures) {
            future.wait();
        }
    }
};
```

## 실습 과제

### Phase 1: 기본 Job System
1. **Lock-free Queue 구현**
2. **Simple Thread Pool 제작**
3. **성능 벤치마킹**

### Phase 2: Work-Stealing 최적화
1. **Work-Stealing Queue 구현**
2. **Load Balancing 개선**
3. **Cache-line 정렬 최적화**

### Phase 3: ECS 통합
1. **System 의존성 그래프**
2. **Parallel Component Processing**
3. **Thread-safe Query 시스템**

## 성능 측정
- Thread contention 분석
- Cache miss 비율 측정
- Load balancing 효율성
- 스케일링 특성 평가

## 디버깅과 프로파일링
- Race condition 탐지
- Deadlock 방지 전략
- Thread sanitizer 활용
- Performance counter 구현

## 참고 자료
- "The Art of Multiprocessor Programming" (Herlihy & Shavit)
- "C++ Concurrency in Action" (Anthony Williams)
- "Parallel Programming with Intel Threading Building Blocks"
- GDC Talks on Job Systems