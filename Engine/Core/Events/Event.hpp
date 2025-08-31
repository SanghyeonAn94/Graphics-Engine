// Event.hpp - 이벤트 시스템의 기본 클래스들
#pragma once

#include "../Core.hpp"
#include <typeinfo>
#include <chrono>

namespace Engine::Events {
    
    // 이벤트 타입 ID
    using EventTypeID = uint32;
    
    // 이벤트 기본 클래스  
    class Event {
    private:
        bool handled = false;
        std::chrono::high_resolution_clock::time_point timestamp;
        
    public:
        Event() : timestamp(std::chrono::high_resolution_clock::now()) {}
        virtual ~Event() = default;
        
        // 이벤트 타입 정보
        virtual EventTypeID getEventType() const = 0;
        virtual const char* getName() const = 0;
        
        // 이벤트 상태 관리
        bool isHandled() const { return handled; }
        void setHandled(bool h = true) { handled = h; }
        
        // 시간 정보
        auto getTimestamp() const { return timestamp; }
        double getAgeMs() const {
            auto now = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - timestamp);
            return duration.count() / 1000.0;
        }
        
        // 디버깅용
        virtual String toString() const {
            return String("Event: ") + getName();
        }
    };
    
    // 이벤트 타입 ID 생성 헬퍼
    template<typename T>
    EventTypeID getEventTypeID() {
        static_assert(std::is_base_of_v<Event, T>, "T must inherit from Event");
        return static_cast<EventTypeID>(typeid(T).hash_code());
    }
    
    // 구체적인 이벤트 타입들을 위한 CRTP 기본 클래스
    template<typename Derived>
    class EventBase : public Event {
    public:
        EventTypeID getEventType() const override {
            return getEventTypeID<Derived>();
        }
        
        const char* getName() const override {
            return typeid(Derived).name();
        }
        
        static EventTypeID getStaticType() {
            return getEventTypeID<Derived>();
        }
    };
    
    // 일반적인 게임 이벤트들
    
    // 입력 이벤트
    class KeyEvent : public EventBase<KeyEvent> {
    public:
        enum class Action { Press, Release, Repeat };
        
        int keyCode;
        Action action;
        int modifiers;
        
        KeyEvent(int key, Action act, int mods = 0) 
            : keyCode(key), action(act), modifiers(mods) {}
            
        String toString() const override {
            return "KeyEvent: " + std::to_string(keyCode) + 
                   (action == Action::Press ? " pressed" : 
                    action == Action::Release ? " released" : " repeated");
        }
    };
    
    class MouseButtonEvent : public EventBase<MouseButtonEvent> {
    public:
        enum class Action { Press, Release };
        
        int button;
        Action action;
        float x, y; // 마우스 위치
        
        MouseButtonEvent(int btn, Action act, float mouseX, float mouseY)
            : button(btn), action(act), x(mouseX), y(mouseY) {}
            
        String toString() const override {
            return "MouseButtonEvent: button " + std::to_string(button) + 
                   " at (" + std::to_string(x) + ", " + std::to_string(y) + ")";
        }
    };
    
    class MouseMoveEvent : public EventBase<MouseMoveEvent> {
    public:
        float x, y;           // 현재 위치
        float deltaX, deltaY; // 이동량
        
        MouseMoveEvent(float posX, float posY, float dx, float dy)
            : x(posX), y(posY), deltaX(dx), deltaY(dy) {}
            
        String toString() const override {
            return "MouseMoveEvent: (" + std::to_string(x) + ", " + std::to_string(y) + 
                   ") delta (" + std::to_string(deltaX) + ", " + std::to_string(deltaY) + ")";
        }
    };
    
    // 윈도우 이벤트
    class WindowResizeEvent : public EventBase<WindowResizeEvent> {
    public:
        uint32 width, height;
        
        WindowResizeEvent(uint32 w, uint32 h) : width(w), height(h) {}
        
        String toString() const override {
            return "WindowResizeEvent: " + std::to_string(width) + "x" + std::to_string(height);
        }
    };
    
    class WindowCloseEvent : public EventBase<WindowCloseEvent> {
    public:
        String toString() const override {
            return "WindowCloseEvent";
        }
    };
    
    // 게임 로직 이벤트들
    class EntityCreatedEvent : public EventBase<EntityCreatedEvent> {
    public:
        EntityID entityId;
        
        explicit EntityCreatedEvent(EntityID id) : entityId(id) {}
        
        String toString() const override {
            return "EntityCreatedEvent: " + std::to_string(entityId);
        }
    };
    
    class EntityDestroyedEvent : public EventBase<EntityDestroyedEvent> {
    public:
        EntityID entityId;
        
        explicit EntityDestroyedEvent(EntityID id) : entityId(id) {}
        
        String toString() const override {
            return "EntityDestroyedEvent: " + std::to_string(entityId);
        }
    };
    
    // 시스템 이벤트들
    class SystemStartedEvent : public EventBase<SystemStartedEvent> {
    public:
        String systemName;
        
        explicit SystemStartedEvent(String name) : systemName(std::move(name)) {}
        
        String toString() const override {
            return "SystemStartedEvent: " + systemName;
        }
    };
    
    class SystemStoppedEvent : public EventBase<SystemStoppedEvent> {
    public:
        String systemName;
        
        explicit SystemStoppedEvent(String name) : systemName(std::move(name)) {}
        
        String toString() const override {
            return "SystemStoppedEvent: " + systemName;
        }
    };
    
    // 프레임 이벤트 (시뮬레이션에서 중요한 개념)
    class FrameStartEvent : public EventBase<FrameStartEvent> {
    public:
        uint64 frameNumber;
        double deltaTime;
        double totalTime;
        
        FrameStartEvent(uint64 frame, double dt, double total)
            : frameNumber(frame), deltaTime(dt), totalTime(total) {}
            
        String toString() const override {
            return "FrameStartEvent: #" + std::to_string(frameNumber) + 
                   " dt=" + std::to_string(deltaTime);
        }
    };
    
    class FrameEndEvent : public EventBase<FrameEndEvent> {
    public:
        uint64 frameNumber;
        double frameTime; // 이 프레임이 걸린 시간
        
        FrameEndEvent(uint64 frame, double time)
            : frameNumber(frame), frameTime(time) {}
            
        String toString() const override {
            return "FrameEndEvent: #" + std::to_string(frameNumber) + 
                   " time=" + std::to_string(frameTime) + "ms";
        }
    };
}