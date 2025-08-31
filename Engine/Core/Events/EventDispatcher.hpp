// EventDispatcher.hpp - 이벤트 디스패처와 메시지 버스
#pragma once

#include "Event.hpp"
#include "../Threading/ThreadSafeQueue.hpp"
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <iostream>
#include <queue>
#include <memory>

namespace Engine::Events {
    
    // 이벤트 리스너 인터페이스
    class IEventListener {
    public:
        virtual ~IEventListener() = default;
        virtual void onEvent(const Event& event) = 0;
        virtual bool wantsEvent(EventTypeID eventType) const = 0;
    };
    
    // 타입 안전한 이벤트 리스너
    template<typename EventType>
    class EventListener : public IEventListener {
    private:
        std::function<void(const EventType&)> callback;
        
    public:
        explicit EventListener(std::function<void(const EventType&)> cb)
            : callback(std::move(cb)) {}
            
        void onEvent(const Event& event) override {
            if (event.getEventType() == EventType::getStaticType()) {
                callback(static_cast<const EventType&>(event));
            }
        }
        
        bool wantsEvent(EventTypeID eventType) const override {
            return eventType == EventType::getStaticType();
        }
    };
    
    // 이벤트 디스패치 모드
    enum class DispatchMode {
        Immediate,    // 즉시 처리 (시뮬레이션에서 동기 이벤트용)
        Queued,       // 큐에 저장 후 나중에 처리 (일반적인 게임 이벤트)
        NextFrame     // 다음 프레임에 처리 (렌더링 관련 이벤트)
    };
    
    // 이벤트 우선순위 (시뮬레이션에서 중요)
    enum class EventPriority : uint8 {
        Critical = 0,  // 시스템 종료, 크래시 등
        High = 1,      // 입력, 물리 충돌 등
        Normal = 2,    // 일반 게임 로직
        Low = 3,       // UI 업데이트, 사운드 등
        Background = 4 // 통계, 로깅 등
    };
    
    // 우선순위가 있는 이벤트 래퍼
    struct PrioritizedEvent {
        UniquePtr<Event> event;
        EventPriority priority;
        DispatchMode mode;
        
        PrioritizedEvent(UniquePtr<Event> evt, EventPriority prio, DispatchMode dispatchMode)
            : event(std::move(evt)), priority(prio), mode(dispatchMode) {}
            
        // Move-only 타입으로 만들기
        PrioritizedEvent(const PrioritizedEvent&) = delete;
        PrioritizedEvent& operator=(const PrioritizedEvent&) = delete;
        PrioritizedEvent(PrioritizedEvent&&) = default;
        PrioritizedEvent& operator=(PrioritizedEvent&&) = default;
            
        // 우선순위 정렬을 위한 연산자
        bool operator<(const PrioritizedEvent& other) const {
            // 우선순위가 낮을수록 먼저 처리 (Critical = 0이 최우선)
            return priority > other.priority;
        }
    };
    
    // 메인 이벤트 디스패처 클래스
    class EventDispatcher {
    private:
        // 리스너 관리
        struct ListenerInfo {
            std::weak_ptr<IEventListener> listener;
            bool persistent; // 자동 제거 안함
        };
        
        std::unordered_map<EventTypeID, Vector<ListenerInfo>> listeners;
        mutable std::shared_mutex listenerMutex;
        
        // 이벤트 큐들 (우선순위별)
        std::priority_queue<PrioritizedEvent> immediateQueue;
        std::priority_queue<PrioritizedEvent> queuedEvents;
        std::priority_queue<PrioritizedEvent> nextFrameEvents;
        
        mutable std::mutex queueMutex;
        
        // 통계 및 디버깅
        struct Statistics {
            std::atomic<uint64> totalEventsDispatched{0};
            std::atomic<uint64> eventsThisFrame{0};
            std::atomic<uint64> listenersRegistered{0};
            std::atomic<double> averageDispatchTimeMs{0.0};
        } stats;
        
        // 이벤트 필터 (조건부 처리)
        std::function<bool(const Event&)> eventFilter;
        
        // Worker thread for background processing
        std::thread workerThread;
        std::atomic<bool> running{false};
        std::condition_variable cv;
        
    public:
        EventDispatcher() = default;
        ~EventDispatcher() { shutdown(); }
        
        // 시작/종료
        void initialize() {
            running = true;
            workerThread = std::thread([this]() { workerLoop(); });
        }
        
        void shutdown() {
            running = false;
            cv.notify_all();
            if (workerThread.joinable()) {
                workerThread.join();
            }
        }
        
        // 리스너 등록
        template<typename EventType>
        void subscribe(std::shared_ptr<IEventListener> listener, bool persistent = true) {
            static_assert(std::is_base_of_v<Event, EventType>, "EventType must inherit from Event");
            
            std::unique_lock lock(listenerMutex);
            
            EventTypeID eventType = EventType::getStaticType();
            listeners[eventType].emplace_back(ListenerInfo{listener, persistent});
            
            stats.listenersRegistered.fetch_add(1);
        }
        
        // 편의 함수: 람다로 리스너 등록
        template<typename EventType>
        auto subscribe(std::function<void(const EventType&)> callback, 
                      EventPriority priority = EventPriority::Normal,
                      bool persistent = true) 
            -> std::shared_ptr<EventListener<EventType>> {
            
            auto listener = std::make_shared<EventListener<EventType>>(std::move(callback));
            subscribe<EventType>(listener, persistent);
            return listener;
        }
        
        // 리스너 제거
        void unsubscribe(std::shared_ptr<IEventListener> listener) {
            std::unique_lock lock(listenerMutex);
            
            for (auto& [eventType, listenerList] : listeners) {
                listenerList.erase(
                    std::remove_if(listenerList.begin(), listenerList.end(),
                        [&listener](const ListenerInfo& info) {
                            return info.listener.expired() || info.listener.lock() == listener;
                        }),
                    listenerList.end()
                );
            }
            
            stats.listenersRegistered.fetch_sub(1);
        }
        
        // 이벤트 발생 (즉시 처리)
        template<typename EventType, typename... Args>
        void dispatchImmediate(Args&&... args) {
            auto event = std::make_unique<EventType>(std::forward<Args>(args)...);
            dispatchImmediate(std::move(event), EventPriority::Normal);
        }
        
        void dispatchImmediate(UniquePtr<Event> event, EventPriority priority = EventPriority::Normal) {
            auto startTime = std::chrono::high_resolution_clock::now();
            
            // 필터 체크
            if (eventFilter && !eventFilter(*event)) {
                return;
            }
            
            // 즉시 처리
            processEvent(*event);
            
            // 통계 업데이트
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            updateStatistics(duration.count() / 1000.0);
        }
        
        // 이벤트 큐잉 (지연 처리)
        template<typename EventType, typename... Args>
        void dispatchQueued(EventPriority priority = EventPriority::Normal, Args&&... args) {
            auto event = std::make_unique<EventType>(std::forward<Args>(args)...);
            dispatchQueued(std::move(event), priority);
        }
        
        void dispatchQueued(UniquePtr<Event> event, EventPriority priority = EventPriority::Normal) {
            std::lock_guard lock(queueMutex);
            queuedEvents.emplace(std::move(event), priority, DispatchMode::Queued);
            cv.notify_one();
        }
        
        // 다음 프레임에 처리
        template<typename EventType, typename... Args>
        void dispatchNextFrame(EventPriority priority = EventPriority::Normal, Args&&... args) {
            auto event = std::make_unique<EventType>(std::forward<Args>(args)...);
            std::lock_guard lock(queueMutex);
            nextFrameEvents.emplace(std::move(event), priority, DispatchMode::NextFrame);
        }
        
        // 프레임별 이벤트 처리 (게임 루프에서 호출)
        void processFrameEvents() {
            stats.eventsThisFrame.store(0);
            
            // 이번 프레임 이벤트들을 큐에서 현재 큐로 이동
            {
                std::lock_guard lock(queueMutex);
                while (!nextFrameEvents.empty()) {
                    queuedEvents.push(std::move(const_cast<PrioritizedEvent&>(nextFrameEvents.top())));
                    nextFrameEvents.pop();
                }
            }
            
            // 큐된 이벤트들 처리
            processQueuedEvents();
        }
        
        // 이벤트 필터 설정
        void setEventFilter(std::function<bool(const Event&)> filter) {
            eventFilter = std::move(filter);
        }
        
        void clearEventFilter() {
            eventFilter = nullptr;
        }
        
        // 통계 조회
        struct EventStatistics {
            uint64 totalEventsDispatched;
            uint64 eventsThisFrame; 
            uint64 listenersRegistered;
            double averageDispatchTimeMs;
            size_t queuedEventCount;
            size_t nextFrameEventCount;
            size_t scheduledEvents;
            size_t activeListeners;
        };
        
        EventStatistics getStatistics() const {
            std::lock_guard lock(queueMutex);
            std::shared_lock listenerLock(listenerMutex);
            
            size_t activeListenerCount = 0;
            for (const auto& [eventType, listenerList] : listeners) {
                for (const auto& info : listenerList) {
                    if (!info.listener.expired()) {
                        ++activeListenerCount;
                    }
                }
            }
            
            return EventStatistics{
                stats.totalEventsDispatched.load(),
                stats.eventsThisFrame.load(),
                stats.listenersRegistered.load(),
                stats.averageDispatchTimeMs.load(),
                queuedEvents.size(),
                nextFrameEvents.size(),
                0, // scheduledEvents는 별도 스케줄러에서 관리
                activeListenerCount
            };
        }
        
        // 디버깅용 이벤트 덤프
        void dumpQueuedEvents() const {
            std::lock_guard lock(queueMutex);
            
            std::cout << "=== Queued Events ===\n";
            std::cout << "Queued events count: " << queuedEvents.size() << "\n";
            std::cout << "Next frame events count: " << nextFrameEvents.size() << "\n";
        }
        
    private:
        // 단일 이벤트 처리
        void processEvent(const Event& event) {
            std::shared_lock lock(listenerMutex);
            
            auto it = listeners.find(event.getEventType());
            if (it != listeners.end()) {
                // 만료된 리스너들 제거 (읽기 락에서는 제거 안함)
                for (const auto& listenerInfo : it->second) {
                    if (auto listener = listenerInfo.listener.lock()) {
                        if (listener->wantsEvent(event.getEventType())) {
                            listener->onEvent(event);
                            
                            // 이벤트가 처리되면 중단할지 결정
                            if (event.isHandled()) {
                                break;
                            }
                        }
                    }
                }
            }
            
            stats.eventsThisFrame.fetch_add(1);
            stats.totalEventsDispatched.fetch_add(1);
        }
        
        // 큐된 이벤트들 처리
        void processQueuedEvents() {
            const size_t maxEventsPerFrame = 100; // 프레임당 최대 이벤트 수
            size_t processedCount = 0;
            
            std::unique_lock lock(queueMutex);
            
            while (!queuedEvents.empty() && processedCount < maxEventsPerFrame) {
                auto prioritizedEvent = std::move(const_cast<PrioritizedEvent&>(queuedEvents.top()));
                queuedEvents.pop();
                
                lock.unlock(); // 처리 중에는 락 해제
                processEvent(*prioritizedEvent.event);
                lock.lock();
                
                ++processedCount;
            }
        }
        
        // 백그라운드 워커 루프
        void workerLoop() {
            while (running) {
                std::unique_lock lock(queueMutex);
                
                // 큐가 비어있으면 대기
                cv.wait(lock, [this] { return !queuedEvents.empty() || !running; });
                
                if (!running) break;
                
                // 백그라운드에서 처리할 이벤트들 (낮은 우선순위)
                while (!queuedEvents.empty()) {
                    const auto& prioritizedEvent = queuedEvents.top();
                    
                    // 백그라운드에서 처리할 우선순위인지 확인
                    if (prioritizedEvent.priority <= EventPriority::Normal) {
                        break; // 메인 스레드에서 처리하도록 남겨둠
                    }
                    
                    auto event = std::move(const_cast<PrioritizedEvent&>(prioritizedEvent));
                    queuedEvents.pop();
                    
                    lock.unlock();
                    processEvent(*event.event);
                    lock.lock();
                }
            }
        }
        
        // 통계 업데이트
        void updateStatistics(double dispatchTimeMs) {
            // 이동 평균 계산
            double current = stats.averageDispatchTimeMs.load();
            double newAverage = current * 0.95 + dispatchTimeMs * 0.05;
            stats.averageDispatchTimeMs.store(newAverage);
        }
        
        // 만료된 리스너들 정리 (주기적으로 호출)
        void cleanupExpiredListeners() {
            std::unique_lock lock(listenerMutex);
            
            for (auto& [eventType, listenerList] : listeners) {
                listenerList.erase(
                    std::remove_if(listenerList.begin(), listenerList.end(),
                        [](const ListenerInfo& info) {
                            return info.listener.expired();
                        }),
                    listenerList.end()
                );
            }
        }
    };
    
    // 글로벌 이벤트 시스템 (싱글톤 패턴)
    class EventSystem {
    private:
        static inline UniquePtr<EventDispatcher> dispatcher;
        static inline std::once_flag initFlag;
        
        static void createDispatcher() {
            dispatcher = std::make_unique<EventDispatcher>();
            dispatcher->initialize();
        }
        
    public:
        static EventDispatcher& getInstance() {
            std::call_once(initFlag, createDispatcher);
            return *dispatcher;
        }
        
        static void shutdown() {
            if (dispatcher) {
                dispatcher->shutdown();
                dispatcher.reset();
            }
        }
        
        // 편의 함수들
        template<typename EventType, typename... Args>
        static void emit(Args&&... args) {
            getInstance().dispatchImmediate<EventType>(std::forward<Args>(args)...);
        }
        
        template<typename EventType, typename... Args>
        static void emitQueued(EventPriority priority = EventPriority::Normal, Args&&... args) {
            getInstance().dispatchQueued<EventType>(priority, std::forward<Args>(args)...);
        }
        
        template<typename EventType>
        static auto listen(std::function<void(const EventType&)> callback, bool persistent = true) {
            return getInstance().subscribe<EventType>(std::move(callback), persistent);
        }
        
        static void processFrame() {
            getInstance().processFrameEvents();
        }
        
        static auto getStats() {
            return getInstance().getStatistics();
        }
    };
    
    // 스코프 기반 이벤트 리스너 (RAII)
    template<typename EventType>
    class ScopedEventListener {
    private:
        std::shared_ptr<EventListener<EventType>> listener;
        
    public:
        template<typename Callback>
        explicit ScopedEventListener(Callback&& callback) {
            listener = EventSystem::listen<EventType>(std::forward<Callback>(callback), false);
        }
        
        ~ScopedEventListener() {
            if (listener) {
                EventSystem::getInstance().unsubscribe(listener);
            }
        }
        
        // Non-copyable, move-only
        ScopedEventListener(const ScopedEventListener&) = delete;
        ScopedEventListener& operator=(const ScopedEventListener&) = delete;
        ScopedEventListener(ScopedEventListener&&) = default;
        ScopedEventListener& operator=(ScopedEventListener&&) = default;
    };
    
    // 이벤트 스케줄러 (시뮬레이션에서 중요한 기능)
    class EventScheduler {
    private:
        struct ScheduledEvent {
            UniquePtr<Event> event;
            std::chrono::high_resolution_clock::time_point executeTime;
            bool recurring;
            std::chrono::milliseconds interval;
            EventPriority priority;
            
            bool operator<(const ScheduledEvent& other) const {
                // 실행 시간이 늦을수록 우선순위가 낮음 (priority_queue는 max heap)
                return executeTime > other.executeTime;
            }
        };
        
        std::priority_queue<ScheduledEvent> scheduledEvents;
        mutable std::mutex scheduleMutex;
        
    public:
        // 지연 실행 이벤트 스케줄링
        template<typename EventType, typename... Args>
        void scheduleEvent(std::chrono::milliseconds delay, 
                          EventPriority priority = EventPriority::Normal,
                          Args&&... args) {
            auto event = std::make_unique<EventType>(std::forward<Args>(args)...);
            auto executeTime = std::chrono::high_resolution_clock::now() + delay;
            
            std::lock_guard lock(scheduleMutex);
            scheduledEvents.emplace(ScheduledEvent{
                std::move(event), executeTime, false, {}, priority
            });
        }
        
        // 반복 실행 이벤트 스케줄링
        template<typename EventType, typename... Args>
        void scheduleRecurring(std::chrono::milliseconds interval,
                              EventPriority priority = EventPriority::Normal, 
                              Args&&... args) {
            auto event = std::make_unique<EventType>(std::forward<Args>(args)...);
            auto executeTime = std::chrono::high_resolution_clock::now() + interval;
            
            std::lock_guard lock(scheduleMutex);
            scheduledEvents.emplace(ScheduledEvent{
                std::move(event), executeTime, true, interval, priority
            });
        }
        
        // 스케줄된 이벤트들 처리 (매 프레임 호출)
        void processScheduledEvents() {
            auto now = std::chrono::high_resolution_clock::now();
            Vector<ScheduledEvent> readyEvents;
            
            {
                std::lock_guard lock(scheduleMutex);
                
                while (!scheduledEvents.empty() && scheduledEvents.top().executeTime <= now) {
                    readyEvents.push_back(std::move(const_cast<ScheduledEvent&>(scheduledEvents.top())));
                    scheduledEvents.pop();
                }
            }
            
            // 준비된 이벤트들 실행
            for (auto& scheduledEvent : readyEvents) {
                EventSystem::getInstance().dispatchImmediate(
                    std::move(scheduledEvent.event), scheduledEvent.priority);
                
                // 반복 이벤트면 다시 스케줄링
                if (scheduledEvent.recurring) {
                    std::lock_guard lock(scheduleMutex);
                    scheduledEvent.executeTime = now + scheduledEvent.interval;
                    // 새 이벤트 인스턴스 필요 (원본은 이미 이동됨)
                    // 실제 구현에서는 팩토리 패턴 등으로 해결
                }
            }
        }
        
        // 모든 스케줄된 이벤트 취소
        void clearScheduledEvents() {
            std::lock_guard lock(scheduleMutex);
            while (!scheduledEvents.empty()) {
                scheduledEvents.pop();
            }
        }
        
        size_t getScheduledEventCount() const {
            std::lock_guard lock(scheduleMutex);
            return scheduledEvents.size();
        }
    };
    
    // 글로벌 이벤트 스케줄러
    inline EventScheduler g_eventScheduler;
}

// 편의 매크로들
#define ENGINE_EMIT_EVENT(EventType, ...) \
    Engine::Events::EventSystem::emit<EventType>(__VA_ARGS__)

#define ENGINE_EMIT_QUEUED(EventType, priority, ...) \
    Engine::Events::EventSystem::emitQueued<EventType>(priority, __VA_ARGS__)

#define ENGINE_LISTEN(EventType, callback) \
    Engine::Events::EventSystem::listen<EventType>(callback)

#define ENGINE_SCOPED_LISTEN(EventType, callback) \
    Engine::Events::ScopedEventListener<EventType> ENGINE_UNIQUE_NAME(listener)(callback)