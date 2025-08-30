# 08. Profiling & Debugging Tools

## 학습 목표
- 게임 엔진 성능 분석을 위한 프로파일링 시스템 구축
- CPU/GPU 성능 측정 도구 개발
- 메모리 사용량 추적과 누수 탐지
- 실시간 디버깅 인터페이스 구현

## 강의 내용

### 8.1 프로파일링의 중요성
- 성능 병목점 식별
- 메모리 사용 패턴 분석
- 프레임 드랍과 스터터링 원인 추적
- 최적화 효과 측정

### 8.2 CPU 프로파일링 시스템
```cpp
// 고해상도 타이머 클래스
class HighResolutionTimer {
private:
    std::chrono::high_resolution_clock::time_point startTime;
    
public:
    void start() {
        startTime = std::chrono::high_resolution_clock::now();
    }
    
    double getElapsedMicroseconds() const {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime);
        return static_cast<double>(duration.count());
    }
    
    double getElapsedMilliseconds() const {
        return getElapsedMicroseconds() / 1000.0;
    }
    
    double getElapsedSeconds() const {
        return getElapsedMicroseconds() / 1000000.0;
    }
};

// 프로파일링 데이터 저장
struct ProfileData {
    std::string name;
    double timeMs;
    uint64_t callCount;
    double minTimeMs;
    double maxTimeMs;
    double totalTimeMs;
    
    void update(double newTimeMs) {
        timeMs = newTimeMs;
        callCount++;
        totalTimeMs += newTimeMs;
        minTimeMs = std::min(minTimeMs, newTimeMs);
        maxTimeMs = std::max(maxTimeMs, newTimeMs);
    }
    
    double getAverageMs() const {
        return callCount > 0 ? totalTimeMs / callCount : 0.0;
    }
};

// 프로파일러 클래스
class Profiler {
private:
    std::unordered_map<std::string, ProfileData> profileData;
    std::unordered_map<std::string, HighResolutionTimer> activeTimers;
    std::mutex dataMutex;
    
    // 계층적 프로파일링
    struct ProfileNode {
        std::string name;
        double timeMs;
        std::vector<std::unique_ptr<ProfileNode>> children;
        ProfileNode* parent;
    };
    
    std::unique_ptr<ProfileNode> rootNode;
    ProfileNode* currentNode;
    
public:
    static Profiler& getInstance() {
        static Profiler instance;
        return instance;
    }
    
    void beginProfile(const std::string& name) {
        std::lock_guard<std::mutex> lock(dataMutex);
        activeTimers[name].start();
        
        // 계층적 구조 업데이트
        auto newNode = std::make_unique<ProfileNode>();
        newNode->name = name;
        newNode->parent = currentNode;
        
        if (currentNode) {
            currentNode->children.push_back(std::move(newNode));
            currentNode = currentNode->children.back().get();
        } else {
            rootNode = std::move(newNode);
            currentNode = rootNode.get();
        }
    }
    
    void endProfile(const std::string& name) {
        std::lock_guard<std::mutex> lock(dataMutex);
        
        if (auto it = activeTimers.find(name); it != activeTimers.end()) {
            double elapsedMs = it->second.getElapsedMilliseconds();
            
            // 데이터 업데이트
            if (auto dataIt = profileData.find(name); dataIt != profileData.end()) {
                dataIt->second.update(elapsedMs);
            } else {
                ProfileData data{};
                data.name = name;
                data.minTimeMs = elapsedMs;
                data.maxTimeMs = elapsedMs;
                data.update(elapsedMs);
                profileData[name] = data;
            }
            
            // 노드 시간 설정
            if (currentNode) {
                currentNode->timeMs = elapsedMs;
                currentNode = currentNode->parent;
            }
            
            activeTimers.erase(it);
        }
    }
    
    const std::unordered_map<std::string, ProfileData>& getProfileData() const {
        return profileData;
    }
    
    void clearData() {
        std::lock_guard<std::mutex> lock(dataMutex);
        profileData.clear();
        activeTimers.clear();
        rootNode.reset();
        currentNode = nullptr;
    }
};

// RAII 스타일 프로파일링
class ScopedProfiler {
private:
    std::string name;
    
public:
    explicit ScopedProfiler(std::string name) : name(std::move(name)) {
        Profiler::getInstance().beginProfile(this->name);
    }
    
    ~ScopedProfiler() {
        Profiler::getInstance().endProfile(name);
    }
};

// 편의 매크로
#define PROFILE_SCOPE(name) ScopedProfiler prof(name)
#define PROFILE_FUNCTION() ScopedProfiler prof(__FUNCTION__)
```

### 8.3 GPU 프로파일링 시스템
```cpp
// GPU 타이머 (Vulkan 예시)
class VulkanGPUProfiler {
private:
    struct GPUTimer {
        VkQueryPool queryPool;
        uint32_t queryIndex;
        std::string name;
        bool active;
    };
    
    VkDevice device;
    VkQueue queue;
    std::vector<GPUTimer> timers;
    uint32_t nextQueryIndex = 0;
    
public:
    VulkanGPUProfiler(VkDevice dev, VkQueue q) : device(dev), queue(q) {
        // Query Pool 생성
        VkQueryPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        poolInfo.queryCount = 1000; // 최대 500개 타이머
        
        VkQueryPool queryPool;
        vkCreateQueryPool(device, &poolInfo, nullptr, &queryPool);
    }
    
    void beginGPUTimer(VkCommandBuffer cmd, const std::string& name) {
        GPUTimer timer{};
        timer.name = name;
        timer.queryIndex = nextQueryIndex;
        timer.active = true;
        
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
                           queryPool, nextQueryIndex++);
        
        timers.push_back(timer);
    }
    
    void endGPUTimer(VkCommandBuffer cmd, const std::string& name) {
        // 해당 타이머 찾기
        for (auto& timer : timers) {
            if (timer.name == name && timer.active) {
                vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                   queryPool, nextQueryIndex++);
                timer.active = false;
                break;
            }
        }
    }
    
    std::unordered_map<std::string, double> getResults() {
        std::unordered_map<std::string, double> results;
        
        // Query 결과 읽기
        std::vector<uint64_t> timestamps(nextQueryIndex);
        vkGetQueryPoolResults(device, queryPool, 0, nextQueryIndex,
                             timestamps.size() * sizeof(uint64_t),
                             timestamps.data(), sizeof(uint64_t),
                             VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        
        // 타이머별 시간 계산
        for (const auto& timer : timers) {
            if (!timer.active && timer.queryIndex + 1 < nextQueryIndex) {
                uint64_t startTime = timestamps[timer.queryIndex];
                uint64_t endTime = timestamps[timer.queryIndex + 1];
                
                // 나노초를 밀리초로 변환
                double timeMs = (endTime - startTime) / 1000000.0;
                results[timer.name] = timeMs;
            }
        }
        
        return results;
    }
};
```

### 8.4 메모리 프로파일링
```cpp
// 메모리 추적 시스템
class MemoryProfiler {
private:
    struct AllocationInfo {
        size_t size;
        std::string file;
        int line;
        std::string function;
        std::chrono::time_point<std::chrono::system_clock> timestamp;
    };
    
    std::unordered_map<void*, AllocationInfo> allocations;
    std::mutex allocationsMutex;
    
    std::atomic<size_t> totalAllocated{0};
    std::atomic<size_t> totalDeallocated{0};
    std::atomic<size_t> peakUsage{0};
    std::atomic<size_t> currentUsage{0};
    
public:
    static MemoryProfiler& getInstance() {
        static MemoryProfiler instance;
        return instance;
    }
    
    void recordAllocation(void* ptr, size_t size, const char* file, int line, const char* function) {
        if (!ptr) return;
        
        std::lock_guard<std::mutex> lock(allocationsMutex);
        
        AllocationInfo info{};
        info.size = size;
        info.file = file;
        info.line = line;
        info.function = function;
        info.timestamp = std::chrono::system_clock::now();
        
        allocations[ptr] = info;
        
        totalAllocated.fetch_add(size);
        size_t current = currentUsage.fetch_add(size) + size;
        
        // Peak usage 업데이트
        size_t peak = peakUsage.load();
        while (current > peak && !peakUsage.compare_exchange_weak(peak, current)) {
            peak = peakUsage.load();
        }
    }
    
    void recordDeallocation(void* ptr) {
        if (!ptr) return;
        
        std::lock_guard<std::mutex> lock(allocationsMutex);
        
        if (auto it = allocations.find(ptr); it != allocations.end()) {
            size_t size = it->second.size;
            totalDeallocated.fetch_add(size);
            currentUsage.fetch_sub(size);
            allocations.erase(it);
        }
    }
    
    struct MemoryStats {
        size_t totalAllocated;
        size_t totalDeallocated;
        size_t currentUsage;
        size_t peakUsage;
        size_t leakCount;
        size_t leakSize;
    };
    
    MemoryStats getStats() const {
        MemoryStats stats{};
        stats.totalAllocated = totalAllocated.load();
        stats.totalDeallocated = totalDeallocated.load();
        stats.currentUsage = currentUsage.load();
        stats.peakUsage = peakUsage.load();
        
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(allocationsMutex));
        stats.leakCount = allocations.size();
        
        for (const auto& [ptr, info] : allocations) {
            stats.leakSize += info.size;
        }
        
        return stats;
    }
    
    std::vector<AllocationInfo> getLeaks() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(allocationsMutex));
        
        std::vector<AllocationInfo> leaks;
        for (const auto& [ptr, info] : allocations) {
            leaks.push_back(info);
        }
        
        return leaks;
    }
};

// 커스텀 allocator로 메모리 추적
void* operator new(size_t size, const char* file, int line, const char* function) {
    void* ptr = std::malloc(size);
    MemoryProfiler::getInstance().recordAllocation(ptr, size, file, line, function);
    return ptr;
}

void operator delete(void* ptr) noexcept {
    MemoryProfiler::getInstance().recordDeallocation(ptr);
    std::free(ptr);
}

// 편의 매크로
#define PROFILE_NEW new(__FILE__, __LINE__, __FUNCTION__)
#define PROFILE_DELETE delete
```

### 8.5 실시간 디버그 인터페이스
```cpp
// ImGui를 활용한 디버그 UI
class DebugUI {
private:
    bool showProfiler = false;
    bool showMemoryProfiler = false;
    bool showGPUProfiler = false;
    
    std::vector<float> frameTimes;
    static constexpr size_t MAX_FRAME_SAMPLES = 120;
    
public:
    void initialize() {
        // ImGui 초기화
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        
        ImGui::StyleColorsDark();
    }
    
    void render() {
        // 메인 메뉴
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Debug")) {
                ImGui::MenuItem("CPU Profiler", nullptr, &showProfiler);
                ImGui::MenuItem("Memory Profiler", nullptr, &showMemoryProfiler);
                ImGui::MenuItem("GPU Profiler", nullptr, &showGPUProfiler);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        
        // 프로파일러 창
        if (showProfiler) {
            renderCPUProfiler();
        }
        
        if (showMemoryProfiler) {
            renderMemoryProfiler();
        }
        
        if (showGPUProfiler) {
            renderGPUProfiler();
        }
        
        // 프레임 타임 그래프
        renderFrameTimeGraph();
    }
    
private:
    void renderCPUProfiler() {
        if (!ImGui::Begin("CPU Profiler", &showProfiler)) {
            ImGui::End();
            return;
        }
        
        const auto& profileData = Profiler::getInstance().getProfileData();
        
        if (ImGui::BeginTable("ProfilerTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_Sortable)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Time (ms)");
            ImGui::TableSetupColumn("Calls");
            ImGui::TableSetupColumn("Avg (ms)");
            ImGui::TableSetupColumn("Max (ms)");
            ImGui::TableHeadersRow();
            
            for (const auto& [name, data] : profileData) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%s", name.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%.3f", data.timeMs);
                ImGui::TableNextColumn(); ImGui::Text("%llu", data.callCount);
                ImGui::TableNextColumn(); ImGui::Text("%.3f", data.getAverageMs());
                ImGui::TableNextColumn(); ImGui::Text("%.3f", data.maxTimeMs);
            }
            
            ImGui::EndTable();
        }
        
        if (ImGui::Button("Clear Data")) {
            Profiler::getInstance().clearData();
        }
        
        ImGui::End();
    }
    
    void renderMemoryProfiler() {
        if (!ImGui::Begin("Memory Profiler", &showMemoryProfiler)) {
            ImGui::End();
            return;
        }
        
        auto stats = MemoryProfiler::getInstance().getStats();
        
        ImGui::Text("Total Allocated: %.2f MB", stats.totalAllocated / (1024.0 * 1024.0));
        ImGui::Text("Total Deallocated: %.2f MB", stats.totalDeallocated / (1024.0 * 1024.0));
        ImGui::Text("Current Usage: %.2f MB", stats.currentUsage / (1024.0 * 1024.0));
        ImGui::Text("Peak Usage: %.2f MB", stats.peakUsage / (1024.0 * 1024.0));
        
        ImGui::Separator();
        
        ImGui::Text("Memory Leaks: %zu allocations (%.2f MB)", 
                   stats.leakCount, stats.leakSize / (1024.0 * 1024.0));
        
        if (ImGui::Button("Show Leaks") && stats.leakCount > 0) {
            auto leaks = MemoryProfiler::getInstance().getLeaks();
            // 리크 정보 표시 로직
        }
        
        ImGui::End();
    }
    
    void renderFrameTimeGraph() {
        // 프레임 시간 수집
        static auto lastTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto frameTime = std::chrono::duration<float, std::milli>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        frameTimes.push_back(frameTime);
        if (frameTimes.size() > MAX_FRAME_SAMPLES) {
            frameTimes.erase(frameTimes.begin());
        }
        
        // 그래프 렌더링
        ImGui::Begin("Frame Times");
        
        float average = 0.0f;
        for (float time : frameTimes) {
            average += time;
        }
        average /= frameTimes.size();
        
        ImGui::Text("Frame Time: %.1f ms (%.1f FPS)", frameTime, 1000.0f / frameTime);
        ImGui::Text("Average: %.1f ms (%.1f FPS)", average, 1000.0f / average);
        
        ImGui::PlotLines("Frame Times", frameTimes.data(), frameTimes.size(), 
                        0, nullptr, 0.0f, 50.0f, ImVec2(0, 80));
        
        ImGui::End();
    }
};
```

## 실습 과제

### Phase 1: 기본 프로파일링
1. **CPU 타이머 구현**
2. **RAII 스타일 프로파일러**
3. **기본 통계 수집**

### Phase 2: 고급 기능
1. **계층적 프로파일링**
2. **GPU 타이머 통합**
3. **메모리 추적 시스템**

### Phase 3: 시각화
1. **ImGui 기반 디버그 UI**
2. **실시간 그래프**
3. **성능 리포트 생성**

## 최적화 고려사항
- 프로파일링 오버헤드 최소화
- 릴리즈 빌드에서 프로파일링 코드 제거
- 스레드 안전성 보장
- 대량 데이터 처리 최적화

## 참고 자료
- "Game Engine Architecture" Profiling 챕터
- Intel VTune Profiler 문서
- NVIDIA Nsight Graphics 가이드
- Dear ImGui 사용법