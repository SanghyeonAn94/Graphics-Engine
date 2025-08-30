// Engine.Core 모듈 - 엔진의 핵심 기능들
export module Engine.Core;

// 표준 라이브러리 모듈 임포트
import <memory>;
import <vector>;
import <string>;
import <type_traits>;
import <concepts>;
import <span>;
import <optional>;
import <functional>;

// 기본 타입 정의
export namespace Engine {
    // 기본 정수 타입들
    using uint8 = std::uint8_t;
    using uint16 = std::uint16_t;
    using uint32 = std::uint32_t;
    using uint64 = std::uint64_t;
    using int8 = std::int8_t;
    using int16 = std::int16_t;
    using int32 = std::int32_t;
    using int64 = std::int64_t;
    
    // 부동소수점 타입들
    using float32 = float;
    using float64 = double;
    
    // 문자열 타입들
    using String = std::string;
    template<typename T>
    using Vector = std::vector<T>;
    template<typename T>
    using Span = std::span<T>;
    template<typename T>
    using Optional = std::optional<T>;
    template<typename T>
    using UniquePtr = std::unique_ptr<T>;
    template<typename T>
    using SharedPtr = std::shared_ptr<T>;
    
    // ID 타입들
    using EntityID = uint32;
    using ComponentTypeID = uint32;
    using SystemID = uint32;
    
    static constexpr EntityID INVALID_ENTITY = 0;
    static constexpr ComponentTypeID INVALID_COMPONENT_TYPE = 0;
    static constexpr SystemID INVALID_SYSTEM = 0;
}

// Concepts 정의
export namespace Engine {
    // Component는 평범한 데이터 구조여야 함
    template<typename T>
    concept Component = requires {
        sizeof(T) > 0;
        std::is_standard_layout_v<T>;
        std::is_trivially_destructible_v<T>;
    };
    
    // System은 update 함수를 가져야 함
    template<typename T>
    concept System = requires(T system, float deltaTime) {
        system.update(deltaTime);
    };
    
    // Renderable 객체는 렌더링 관련 함수들을 가져야 함
    template<typename T>
    concept Renderable = requires(T obj) {
        { obj.isVisible() } -> std::convertible_to<bool>;
        obj.render();
    };
    
    // Serializable 객체는 직렬화 가능해야 함
    template<typename T>
    concept Serializable = requires(T obj) {
        obj.serialize();
        T::deserialize(std::declval<const std::vector<uint8>&>());
    };
    
    // Resource는 이름과 ID를 가져야 함
    template<typename T>
    concept Resource = requires(T resource) {
        { resource.getId() } -> std::convertible_to<uint32>;
        { resource.getName() } -> std::convertible_to<String>;
    };
}

// 유틸리티 함수들
export namespace Engine::Utils {
    // 컴파일 타임 문자열 해싱
    constexpr uint32 hashString(std::string_view str) noexcept {
        uint32 hash = 2166136261u; // FNV offset basis
        for (char c : str) {
            hash ^= static_cast<uint32>(c);
            hash *= 16777619u; // FNV prime
        }
        return hash;
    }
    
    // 타입 이름을 해시로 변환
    template<typename T>
    constexpr uint32 getTypeHash() noexcept {
        return hashString(typeid(T).name());
    }
    
    // 타입 ID 생성기
    template<typename T>
    constexpr ComponentTypeID getComponentTypeID() noexcept {
        static_assert(Component<T>, "T must be a Component");
        return getTypeHash<T>();
    }
}

// RAII 래퍼들
export namespace Engine {
    // 범용 RAII 래퍼
    template<typename Resource, typename Deleter>
    class RAIIWrapper {
    private:
        Resource resource;
        Deleter deleter;
        bool valid = true;
        
    public:
        explicit RAIIWrapper(Resource res, Deleter del = Deleter{}) 
            : resource(std::move(res)), deleter(std::move(del)) {}
        
        ~RAIIWrapper() {
            if (valid) {
                deleter(resource);
            }
        }
        
        // Move-only
        RAIIWrapper(const RAIIWrapper&) = delete;
        RAIIWrapper& operator=(const RAIIWrapper&) = delete;
        
        RAIIWrapper(RAIIWrapper&& other) noexcept 
            : resource(std::move(other.resource))
            , deleter(std::move(other.deleter))
            , valid(other.valid) {
            other.valid = false;
        }
        
        RAIIWrapper& operator=(RAIIWrapper&& other) noexcept {
            if (this != &other) {
                if (valid) {
                    deleter(resource);
                }
                resource = std::move(other.resource);
                deleter = std::move(other.deleter);
                valid = other.valid;
                other.valid = false;
            }
            return *this;
        }
        
        const Resource& get() const noexcept { return resource; }
        Resource& get() noexcept { return resource; }
        
        Resource release() noexcept {
            valid = false;
            return std::move(resource);
        }
        
        bool isValid() const noexcept { return valid; }
    };
    
    // 스코프 가드
    template<typename Callable>
    class ScopeGuard {
    private:
        Callable callable;
        bool active = true;
        
    public:
        explicit ScopeGuard(Callable&& c) : callable(std::forward<Callable>(c)) {}
        
        ~ScopeGuard() {
            if (active) {
                callable();
            }
        }
        
        void dismiss() noexcept { active = false; }
        
        // Non-copyable, non-movable
        ScopeGuard(const ScopeGuard&) = delete;
        ScopeGuard& operator=(const ScopeGuard&) = delete;
        ScopeGuard(ScopeGuard&&) = delete;
        ScopeGuard& operator=(ScopeGuard&&) = delete;
    };
    
    // 스코프 가드 생성 헬퍼
    template<typename Callable>
    auto makeScopeGuard(Callable&& callable) {
        return ScopeGuard<std::decay_t<Callable>>(std::forward<Callable>(callable));
    }
}

// 매크로 정의
export namespace Engine {
    // 디버그 매크로들
    #ifdef ENGINE_DEBUG
        #define ENGINE_ASSERT(condition, message) \
            do { \
                if (!(condition)) { \
                    std::abort(); \
                } \
            } while(0)
    #else
        #define ENGINE_ASSERT(condition, message) ((void)0)
    #endif
    
    // 스코프 가드 매크로
    #define ENGINE_SCOPE_EXIT(code) \
        auto ENGINE_UNIQUE_NAME(scope_guard) = Engine::makeScopeGuard([&]() { code; })
        
    // 고유 이름 생성
    #define ENGINE_CONCAT_IMPL(a, b) a ## b
    #define ENGINE_CONCAT(a, b) ENGINE_CONCAT_IMPL(a, b)
    #define ENGINE_UNIQUE_NAME(prefix) ENGINE_CONCAT(prefix, __LINE__)
}