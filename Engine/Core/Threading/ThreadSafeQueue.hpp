// ThreadSafeQueue.hpp - 스레드 안전한 큐 구현
#pragma once

#include "../Core.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace Engine::Threading {
    
    // 기본 스레드 안전 큐
    template<typename T>
    class ThreadSafeQueue {
    private:
        mutable std::mutex mutex_;
        std::queue<T> queue_;
        std::condition_variable condition_;
        std::atomic<bool> shutdown_{false};
        
    public:
        ThreadSafeQueue() = default;
        
        // Non-copyable
        ThreadSafeQueue(const ThreadSafeQueue&) = delete;
        ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
        
        // Move-only
        ThreadSafeQueue(ThreadSafeQueue&& other) noexcept {
            std::lock_guard lock(other.mutex_);
            queue_ = std::move(other.queue_);
            shutdown_.store(other.shutdown_.load());
        }
        
        ThreadSafeQueue& operator=(ThreadSafeQueue&& other) noexcept {
            if (this != &other) {
                std::lock(mutex_, other.mutex_);
                std::lock_guard lock1(mutex_, std::adopt_lock);
                std::lock_guard lock2(other.mutex_, std::adopt_lock);
                
                queue_ = std::move(other.queue_);
                shutdown_.store(other.shutdown_.load());
            }
            return *this;
        }
        
        ~ThreadSafeQueue() {
            shutdown();
        }
        
        // 요소 추가
        void push(T item) {
            std::lock_guard lock(mutex_);
            if (!shutdown_.load()) {
                queue_.push(std::move(item));
                condition_.notify_one();
            }
        }
        
        // 요소 제거 (블로킹)
        bool pop(T& item) {
            std::unique_lock lock(mutex_);
            
            while (queue_.empty() && !shutdown_.load()) {
                condition_.wait(lock);
            }
            
            if (shutdown_.load()) {
                return false;
            }
            
            if (!queue_.empty()) {
                item = std::move(queue_.front());
                queue_.pop();
                return true;
            }
            
            return false;
        }
        
        // 요소 제거 (논블로킹)
        bool tryPop(T& item) {
            std::lock_guard lock(mutex_);
            
            if (queue_.empty()) {
                return false;
            }
            
            item = std::move(queue_.front());
            queue_.pop();
            return true;
        }
        
        // 요소 제거 (타임아웃)
        template<typename Rep, typename Period>
        bool popWithTimeout(T& item, const std::chrono::duration<Rep, Period>& timeout) {
            std::unique_lock lock(mutex_);
            
            if (condition_.wait_for(lock, timeout, [this] { return !queue_.empty() || shutdown_.load(); })) {
                if (shutdown_.load()) {
                    return false;
                }
                
                if (!queue_.empty()) {
                    item = std::move(queue_.front());
                    queue_.pop();
                    return true;
                }
            }
            
            return false;
        }
        
        // 큐 상태
        bool empty() const {
            std::lock_guard lock(mutex_);
            return queue_.empty();
        }
        
        size_t size() const {
            std::lock_guard lock(mutex_);
            return queue_.size();
        }
        
        // 큐 종료
        void shutdown() {
            shutdown_.store(true);
            condition_.notify_all();
        }
        
        bool isShutdown() const {
            return shutdown_.load();
        }
        
        // 모든 요소 제거
        void clear() {
            std::lock_guard lock(mutex_);
            while (!queue_.empty()) {
                queue_.pop();
            }
        }
        
        // 큐 내용을 벡터로 복사 (디버깅용)
        Vector<T> snapshot() const {
            std::lock_guard lock(mutex_);
            
            Vector<T> result;
            auto tempQueue = queue_;
            
            while (!tempQueue.empty()) {
                result.push_back(tempQueue.front());
                tempQueue.pop();
            }
            
            return result;
        }
    };
    
    // 우선순위 기반 스레드 안전 큐
    template<typename T>
    class ThreadSafePriorityQueue {
    private:
        mutable std::mutex mutex_;
        std::priority_queue<T> queue_;
        std::condition_variable condition_;
        std::atomic<bool> shutdown_{false};
        
    public:
        ThreadSafePriorityQueue() = default;
        
        // Non-copyable, move-only
        ThreadSafePriorityQueue(const ThreadSafePriorityQueue&) = delete;
        ThreadSafePriorityQueue& operator=(const ThreadSafePriorityQueue&) = delete;
        ThreadSafePriorityQueue(ThreadSafePriorityQueue&&) = default;
        ThreadSafePriorityQueue& operator=(ThreadSafePriorityQueue&&) = default;
        
        ~ThreadSafePriorityQueue() {
            shutdown();
        }
        
        void push(T item) {
            std::lock_guard lock(mutex_);
            if (!shutdown_.load()) {
                queue_.push(std::move(item));
                condition_.notify_one();
            }
        }
        
        bool pop(T& item) {
            std::unique_lock lock(mutex_);
            
            while (queue_.empty() && !shutdown_.load()) {
                condition_.wait(lock);
            }
            
            if (shutdown_.load()) {
                return false;
            }
            
            if (!queue_.empty()) {
                item = std::move(const_cast<T&>(queue_.top()));
                queue_.pop();
                return true;
            }
            
            return false;
        }
        
        bool tryPop(T& item) {
            std::lock_guard lock(mutex_);
            
            if (queue_.empty()) {
                return false;
            }
            
            item = std::move(const_cast<T&>(queue_.top()));
            queue_.pop();
            return true;
        }
        
        bool empty() const {
            std::lock_guard lock(mutex_);
            return queue_.empty();
        }
        
        size_t size() const {
            std::lock_guard lock(mutex_);
            return queue_.size();
        }
        
        void shutdown() {
            shutdown_.store(true);
            condition_.notify_all();
        }
        
        void clear() {
            std::lock_guard lock(mutex_);
            while (!queue_.empty()) {
                queue_.pop();
            }
        }
    };
    
    // Lock-free Single Producer Single Consumer Queue (SPSC)
    template<typename T, size_t Capacity>
    class SPSCQueue {
    private:
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
        
        alignas(64) std::atomic<size_t> head_{0};
        alignas(64) std::atomic<size_t> tail_{0};
        alignas(64) std::array<T, Capacity> buffer_;
        
    public:
        // Producer side
        bool push(T&& item) {
            const size_t current_tail = tail_.load(std::memory_order_relaxed);
            const size_t next_tail = (current_tail + 1) & (Capacity - 1);
            
            if (next_tail == head_.load(std::memory_order_acquire)) {
                return false; // Queue full
            }
            
            buffer_[current_tail] = std::move(item);
            tail_.store(next_tail, std::memory_order_release);
            return true;
        }
        
        bool push(const T& item) {
            const size_t current_tail = tail_.load(std::memory_order_relaxed);
            const size_t next_tail = (current_tail + 1) & (Capacity - 1);
            
            if (next_tail == head_.load(std::memory_order_acquire)) {
                return false; // Queue full
            }
            
            buffer_[current_tail] = item;
            tail_.store(next_tail, std::memory_order_release);
            return true;
        }
        
        // Consumer side
        bool pop(T& item) {
            const size_t current_head = head_.load(std::memory_order_relaxed);
            
            if (current_head == tail_.load(std::memory_order_acquire)) {
                return false; // Queue empty
            }
            
            item = std::move(buffer_[current_head]);
            head_.store((current_head + 1) & (Capacity - 1), std::memory_order_release);
            return true;
        }
        
        // 상태 확인
        bool empty() const {
            return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
        }
        
        bool full() const {
            const size_t current_tail = tail_.load(std::memory_order_acquire);
            const size_t next_tail = (current_tail + 1) & (Capacity - 1);
            return next_tail == head_.load(std::memory_order_acquire);
        }
        
        size_t size() const {
            const size_t current_head = head_.load(std::memory_order_acquire);
            const size_t current_tail = tail_.load(std::memory_order_acquire);
            return (current_tail - current_head) & (Capacity - 1);
        }
        
        static constexpr size_t capacity() {
            return Capacity;
        }
    };
    
    // Multi-Producer Single Consumer Queue (MPSC)
    template<typename T>
    class MPSCQueue {
    private:
        struct Node {
            std::atomic<Node*> next{nullptr};
            T data;
            
            Node() = default;
            explicit Node(T&& item) : data(std::move(item)) {}
        };
        
        alignas(64) std::atomic<Node*> head_;
        alignas(64) std::atomic<Node*> tail_;
        
    public:
        MPSCQueue() {
            Node* dummy = new Node();
            head_.store(dummy);
            tail_.store(dummy);
        }
        
        ~MPSCQueue() {
            while (Node* oldHead = head_.load()) {
                head_.store(oldHead->next.load());
                delete oldHead;
            }
        }
        
        // Producer side (thread-safe for multiple producers)
        void push(T&& item) {
            Node* newNode = new Node(std::move(item));
            Node* prevTail = tail_.exchange(newNode, std::memory_order_acq_rel);
            prevTail->next.store(newNode, std::memory_order_release);
        }
        
        // Consumer side (single consumer only)
        bool pop(T& item) {
            Node* head = head_.load(std::memory_order_relaxed);
            Node* next = head->next.load(std::memory_order_acquire);
            
            if (next == nullptr) {
                return false; // Queue empty
            }
            
            item = std::move(next->data);
            head_.store(next, std::memory_order_release);
            delete head;
            return true;
        }
        
        bool empty() const {
            Node* head = head_.load(std::memory_order_acquire);
            return head->next.load(std::memory_order_acquire) == nullptr;
        }
    };
}

// 사용 편의성을 위한 별칭들
namespace Engine {
    template<typename T>
    using SafeQueue = Threading::ThreadSafeQueue<T>;
    
    template<typename T>
    using SafePriorityQueue = Threading::ThreadSafePriorityQueue<T>;
    
    template<typename T, size_t Capacity>
    using LockFreeQueue = Threading::SPSCQueue<T, Capacity>;
    
    template<typename T>
    using MPSCQueue = Threading::MPSCQueue<T>;
}