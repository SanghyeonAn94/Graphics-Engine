// 타입 레지스트리 모듈 - 컴파일 타임 타입 관리
export module Engine.TypeRegistry;

import Engine.Core;
import <unordered_map>;
import <typeinfo>;
import <functional>;

export namespace Engine {
    // 타입 정보 구조체
    struct TypeInfo {
        ComponentTypeID id;
        String name;
        size_t size;
        size_t alignment;
        std::function<void*()> constructor;
        std::function<void(void*)> destructor;
        
        template<Component T>
        static TypeInfo create() {
            TypeInfo info{};
            info.id = Utils::getComponentTypeID<T>();
            info.name = typeid(T).name();
            info.size = sizeof(T);
            info.alignment = alignof(T);
            info.constructor = []() -> void* { 
                return new T{}; 
            };
            info.destructor = [](void* ptr) { 
                delete static_cast<T*>(ptr); 
            };
            return info;
        }
    };
    
    // 컴파일 타임 타입 레지스트리
    template<Component... Components>
    class TypeRegistry {
    private:
        static inline std::unordered_map<ComponentTypeID, TypeInfo> typeMap;
        static inline bool initialized = false;
        
        static void initialize() {
            if (initialized) return;
            
            // 모든 컴포넌트 타입을 등록
            (registerComponent<Components>(), ...);
            initialized = true;
        }
        
        template<Component T>
        static void registerComponent() {
            auto typeInfo = TypeInfo::create<T>();
            typeMap[typeInfo.id] = std::move(typeInfo);
        }
        
    public:
        static constexpr size_t getComponentCount() {
            return sizeof...(Components);
        }
        
        template<Component T>
        static constexpr ComponentTypeID getComponentID() {
            return Utils::getComponentTypeID<T>();
        }
        
        template<Component T>
        static constexpr size_t getComponentIndex() {
            return getComponentIndexImpl<T, Components...>();
        }
        
        static const TypeInfo* getTypeInfo(ComponentTypeID id) {
            initialize();
            auto it = typeMap.find(id);
            return it != typeMap.end() ? &it->second : nullptr;
        }
        
        template<Component T>
        static const TypeInfo* getTypeInfo() {
            return getTypeInfo(getComponentID<T>());
        }
        
        static auto getAllTypes() {
            initialize();
            return typeMap;
        }
        
    private:
        template<Component T, Component First, Component... Rest>
        static constexpr size_t getComponentIndexImpl() {
            if constexpr (std::is_same_v<T, First>) {
                return 0;
            } else if constexpr (sizeof...(Rest) > 0) {
                return 1 + getComponentIndexImpl<T, Rest...>();
            } else {
                static_assert(std::is_same_v<T, First>, "Component not found in registry");
                return 0; // Never reached
            }
        }
    };
    
    // 컴파일 타임 문자열 해싱을 위한 리터럴 타입
    template<size_t N>
    struct ConstString {
        constexpr ConstString(const char (&str)[N]) {
            std::copy_n(str, N, data);
        }
        
        char data[N];
        static constexpr size_t size = N - 1; // null terminator 제외
        
        constexpr std::string_view view() const {
            return std::string_view(data, size);
        }
        
        constexpr uint32 hash() const {
            return Utils::hashString(view());
        }
    };
    
    // 문자열 리터럴에서 ConstString 생성
    template<ConstString str>
    constexpr uint32 operator""_hash() {
        return str.hash();
    }
    
    // 컴포넌트 서명(Signature) 클래스
    template<Component... Components>
    class ComponentSignature {
    private:
        static constexpr size_t MaxComponents = 64; // 비트셋 크기
        using BitSet = uint64;
        BitSet bits = 0;
        
        template<Component T>
        static constexpr size_t getBitIndex() {
            constexpr size_t index = TypeRegistry<Components...>::template getComponentIndex<T>();
            static_assert(index < MaxComponents, "Too many components for signature");
            return index;
        }
        
    public:
        constexpr ComponentSignature() = default;
        
        template<Component T>
        constexpr ComponentSignature& add() {
            constexpr size_t index = getBitIndex<T>();
            bits |= (BitSet{1} << index);
            return *this;
        }
        
        template<Component T>
        constexpr ComponentSignature& remove() {
            constexpr size_t index = getBitIndex<T>();
            bits &= ~(BitSet{1} << index);
            return *this;
        }
        
        template<Component T>
        constexpr bool has() const {
            constexpr size_t index = getBitIndex<T>();
            return (bits & (BitSet{1} << index)) != 0;
        }
        
        constexpr bool matches(const ComponentSignature& other) const {
            // other의 모든 비트가 this에 포함되어 있는지 확인
            return (bits & other.bits) == other.bits;
        }
        
        constexpr bool operator==(const ComponentSignature& other) const {
            return bits == other.bits;
        }
        
        constexpr bool empty() const {
            return bits == 0;
        }
        
        constexpr size_t count() const {
            return std::popcount(bits);
        }
        
        constexpr BitSet getBits() const {
            return bits;
        }
        
        // 컴파일 타임에 서명 생성
        template<Component... Cs>
        static constexpr ComponentSignature create() {
            ComponentSignature sig;
            (sig.template add<Cs>(), ...);
            return sig;
        }
    };
    
    // 편의를 위한 별칭 템플릿
    template<Component... Components>
    using Signature = ComponentSignature<Components...>;
}

// 사용 예제를 위한 테스트 컴포넌트들
export namespace Engine::TestComponents {
    struct Transform {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        float rotationX = 0.0f, rotationY = 0.0f, rotationZ = 0.0f;
        float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;
    };
    
    struct Velocity {
        float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    };
    
    struct Health {
        int current = 100;
        int maximum = 100;
    };
    
    struct Renderable {
        uint32 meshId = 0;
        uint32 materialId = 0;
        bool visible = true;
        
        bool isVisible() const { return visible; }
        void render() const { /* 렌더링 로직 */ }
    };
    
    // 타입 레지스트리 인스턴스 생성
    using GameTypeRegistry = TypeRegistry<Transform, Velocity, Health, Renderable>;
}

// 사용 예제 함수들
export namespace Engine::Examples {
    void demonstrateTypeRegistry() {
        using namespace TestComponents;
        
        // 컴파일 타임 타입 ID 가져오기
        constexpr auto transformId = GameTypeRegistry::getComponentID<Transform>();
        constexpr auto velocityId = GameTypeRegistry::getComponentID<Velocity>();
        
        // 런타임에 타입 정보 가져오기
        const TypeInfo* transformInfo = GameTypeRegistry::getTypeInfo<Transform>();
        if (transformInfo) {
            // 타입 정보 활용
        }
        
        // 컴포넌트 서명 생성
        constexpr auto movableSignature = Signature<Transform, Velocity, Health, Renderable>::create<Transform, Velocity>();
        constexpr auto renderableSignature = Signature<Transform, Velocity, Health, Renderable>::create<Transform, Renderable>();
        
        // 서명 매칭 테스트
        constexpr bool canMove = movableSignature.has<Velocity>();
        constexpr bool canRender = renderableSignature.has<Renderable>();
        
        // 컴파일 타임 문자열 해싱
        constexpr uint32 systemNameHash = Utils::hashString("MovementSystem");
    }
}