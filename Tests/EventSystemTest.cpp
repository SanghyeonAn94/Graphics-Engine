// EventSystemTest.cpp - Event System 테스트 및 예제
#include "../Engine/Core/Events/Event.hpp"
#include "../Engine/Core/Events/EventDispatcher.hpp"
#include "../Engine/Core/Threading/ThreadSafeQueue.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace Engine;
using namespace Engine::Events;
using namespace Engine::Threading;

// 커스텀 이벤트 예제
class PlayerDamagedEvent : public EventBase<PlayerDamagedEvent> {
public:
    EntityID playerId;
    int damage;
    String attackerName;
    
    PlayerDamagedEvent(EntityID id, int dmg, String attacker)
        : playerId(id), damage(dmg), attackerName(std::move(attacker)) {}
        
    String toString() const override {
        return "PlayerDamagedEvent: Player " + std::to_string(playerId) + 
               " damaged by " + attackerName + " for " + std::to_string(damage);
    }
};

class GameStateChangedEvent : public EventBase<GameStateChangedEvent> {
public:
    enum class State { Menu, Playing, Paused, GameOver };
    
    State oldState;
    State newState;
    
    GameStateChangedEvent(State old, State newState) 
        : oldState(old), newState(newState) {}
        
    String toString() const override {
        auto stateToString = [](State s) {
            switch(s) {
                case State::Menu: return "Menu";
                case State::Playing: return "Playing";
                case State::Paused: return "Paused";
                case State::GameOver: return "GameOver";
                default: return "Unknown";
            }
        };
        return "GameStateChangedEvent: " + String(stateToString(oldState)) + 
               " -> " + String(stateToString(newState));
    }
};

// 이벤트 리스너 예제들
class HealthSystem {
public:
    void onPlayerDamaged(const PlayerDamagedEvent& event) {
        std::cout << "[HealthSystem] Processing: " << event.toString() << std::endl;
        std::cout << "  - Reducing player " << event.playerId << " health by " << event.damage << std::endl;
    }
};

class UISystem {
public:
    void onPlayerDamaged(const PlayerDamagedEvent& event) {
        std::cout << "[UISystem] Updating health bar for player " << event.playerId << std::endl;
    }
    
    void onGameStateChanged(const GameStateChangedEvent& event) {
        std::cout << "[UISystem] " << event.toString() << std::endl;
        std::cout << "  - Updating UI for new game state" << std::endl;
    }
};

class LoggingSystem {
public:
    void onAnyEvent(const Event& event) {
        std::cout << "[Logger] " << event.toString() 
                  << " (age: " << event.getAgeMs() << "ms)" << std::endl;
    }
};

// 스레드 안전 큐 테스트
void testThreadSafeQueues() {
    std::cout << "\n=== Thread Safe Queue Tests ===\n";
    
    // 기본 스레드 안전 큐 테스트
    ThreadSafeQueue<int> queue;
    
    // Producer 스레드
    std::thread producer([&queue]() {
        for (int i = 0; i < 10; ++i) {
            queue.push(i);
            std::cout << "Produced: " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        queue.shutdown();
    });
    
    // Consumer 스레드
    std::thread consumer([&queue]() {
        int value;
        while (queue.pop(value)) {
            std::cout << "Consumed: " << value << std::endl;
        }
        std::cout << "Queue shut down, consumer finished" << std::endl;
    });
    
    producer.join();
    consumer.join();
    
    // Lock-free SPSC 큐 테스트
    std::cout << "\n--- Lock-free SPSC Queue Test ---\n";
    SPSCQueue<int, 16> spscQueue;
    
    std::thread spscProducer([&spscQueue]() {
        for (int i = 0; i < 8; ++i) {
            while (!spscQueue.push(i)) {
                std::this_thread::yield();
            }
            std::cout << "SPSC Produced: " << i << std::endl;
        }
    });
    
    std::thread spscConsumer([&spscQueue]() {
        int value;
        int consumed = 0;
        while (consumed < 8) {
            if (spscQueue.pop(value)) {
                std::cout << "SPSC Consumed: " << value << std::endl;
                ++consumed;
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    spscProducer.join();
    spscConsumer.join();
}

// 이벤트 시스템 통합 테스트
void testEventSystem() {
    std::cout << "\n=== Event System Integration Test ===\n";
    
    EventDispatcher dispatcher;
    dispatcher.initialize();
    
    // 시스템 인스턴스들
    HealthSystem healthSystem;
    UISystem uiSystem;
    LoggingSystem logger;
    
    // 이벤트 리스너 등록
    auto healthListener = dispatcher.subscribe<PlayerDamagedEvent>([&healthSystem](const PlayerDamagedEvent& event) {
        healthSystem.onPlayerDamaged(event);
    });
    
    auto uiDamageListener = dispatcher.subscribe<PlayerDamagedEvent>([&uiSystem](const PlayerDamagedEvent& event) {
        uiSystem.onPlayerDamaged(event);
    });
    
    auto uiStateListener = dispatcher.subscribe<GameStateChangedEvent>([&uiSystem](const GameStateChangedEvent& event) {
        uiSystem.onGameStateChanged(event);
    });
    
    std::cout << "\nDispatching events...\n";
    
    // 이벤트 발생 시뮬레이션
    dispatcher.dispatchImmediate<PlayerDamagedEvent>(101, 25, "Orc Warrior");
    dispatcher.dispatchImmediate<GameStateChangedEvent>(GameStateChangedEvent::State::Menu, 
                                                        GameStateChangedEvent::State::Playing);
    
    // 큐된 이벤트 처리
    std::cout << "\n--- Processing Queued Events ---\n";
    dispatcher.dispatchQueued<WindowResizeEvent>(EventPriority::Normal, 1920, 1080);
    dispatcher.dispatchQueued<KeyEvent>(EventPriority::High, 65, KeyEvent::Action::Press); // 'A' key
    
    // 다음 프레임 이벤트
    std::cout << "\n--- Next Frame Events ---\n";
    dispatcher.dispatchNextFrame<PlayerDamagedEvent>(EventPriority::Normal, 103, 15, "Goblin");
    
    // 프레임 이벤트 시뮬레이션
    std::cout << "\n--- Frame Event Simulation ---\n";
    for (uint64 frame = 1; frame <= 3; ++frame) {
        double deltaTime = 16.67; // ~60 FPS
        double totalTime = frame * deltaTime;
        
        dispatcher.dispatchImmediate<FrameStartEvent>(frame, deltaTime, totalTime);
        
        // 프레임 이벤트 처리
        dispatcher.processFrameEvents();
        
        // 게임 로직 시뮬레이션
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        
        dispatcher.dispatchImmediate<FrameEndEvent>(frame, 5.0);
    }
    
    // 통계 출력
    std::cout << "\n--- Event Statistics ---\n";
    auto stats = dispatcher.getStatistics();
    std::cout << "Total events dispatched: " << stats.totalEventsDispatched << std::endl;
    std::cout << "Events this frame: " << stats.eventsThisFrame << std::endl;
    std::cout << "Queued events: " << stats.queuedEventCount << std::endl;
    std::cout << "Next frame events: " << stats.nextFrameEventCount << std::endl;
    std::cout << "Registered listeners: " << stats.listenersRegistered << std::endl;
    std::cout << "Average dispatch time: " << stats.averageDispatchTimeMs << "ms" << std::endl;
    
    dispatcher.shutdown();
}

// 성능 테스트
void performanceTest() {
    std::cout << "\n=== Performance Test ===\n";
    
    EventDispatcher dispatcher;
    dispatcher.initialize();
    std::atomic<int> processedEvents{0};
    
    // 고성능 리스너 등록
    auto listener = dispatcher.subscribe<KeyEvent>([&processedEvents](const KeyEvent&) {
        processedEvents.fetch_add(1, std::memory_order_relaxed);
    });
    
    const int eventCount = 10000; // 더 적은 수로 테스트
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 대량 이벤트 발생
    for (int i = 0; i < eventCount; ++i) {
        dispatcher.dispatchImmediate<KeyEvent>(i % 256, KeyEvent::Action::Press);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    std::cout << "Processed " << eventCount << " events in " 
              << duration.count() << " microseconds" << std::endl;
    std::cout << "Events per second: " 
              << (eventCount * 1000000.0 / duration.count()) << std::endl;
    std::cout << "Actually processed: " << processedEvents.load() << std::endl;
    
    dispatcher.shutdown();
}

int main() {
    std::cout << "=== Graphics Engine Event System Test ===\n";
    
    try {
        testThreadSafeQueues();
        testEventSystem();
        performanceTest();
        
        std::cout << "\n=== All Tests Completed Successfully ===\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}