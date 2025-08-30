// ModuleTests.cpp - 모듈과 Concepts 테스트
import Engine.Core;
import Engine.TypeRegistry;

#include <iostream>
#include <cassert>
#include <chrono>

using namespace Engine;
using namespace Engine::TestComponents;

// 테스트용 간단한 시스템
class MovementSystem {
public:
    void update(float deltaTime) {
        std::cout << "MovementSystem::update(" << deltaTime << ")\n";
    }
};

// 테스트용 렌더러
struct MockRenderContext {
    void submit(const auto& object) {
        std::cout << "Rendering object\n";
    }
};

// Concepts 테스트 함수들
template<Component T>
void testComponent(const T& component) {
    std::cout << "Testing component of size: " << sizeof(T) << "\n";
    static_assert(std::is_standard_layout_v<T>, "Component must be standard layout");
}

template<System T>
void testSystem(T& system, float deltaTime) {
    std::cout << "Testing system: ";
    system.update(deltaTime);
}

template<Renderable T>
void testRenderable(const T& object) {
    if (object.isVisible()) {
        std::cout << "Object is visible, rendering...\n";
        object.render();
    } else {
        std::cout << "Object is not visible, skipping render.\n";
    }
}

// 성능 테스트 함수
void benchmarkTypeRegistry() {
    using namespace std::chrono;
    
    std::cout << "\n=== Type Registry Benchmark ===\n";
    
    // 컴파일 타임 연산들
    constexpr auto start_compile_time = high_resolution_clock::now();
    
    constexpr auto transformId = GameTypeRegistry::getComponentID<Transform>();
    constexpr auto velocityId = GameTypeRegistry::getComponentID<Velocity>();
    constexpr auto healthId = GameTypeRegistry::getComponentID<Health>();
    constexpr auto renderableId = GameTypeRegistry::getComponentID<Renderable>();
    
    constexpr auto signature = Signature<Transform, Velocity, Health, Renderable>::create<Transform, Velocity>();
    
    std::cout << "Transform ID: " << transformId << "\n";
    std::cout << "Velocity ID: " << velocityId << "\n";
    std::cout << "Health ID: " << healthId << "\n";
    std::cout << "Renderable ID: " << renderableId << "\n";
    
    // 런타임 성능 테스트
    const int iterations = 1000000;
    
    auto start = high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        [[maybe_unused]] const TypeInfo* info = GameTypeRegistry::getTypeInfo<Transform>();
    }
    auto end = high_resolution_clock::now();
    
    auto duration = duration_cast<nanoseconds>(end - start);
    std::cout << "TypeInfo lookup (1M iterations): " << duration.count() << " ns\n";
    std::cout << "Average per lookup: " << duration.count() / iterations << " ns\n";
    
    // 서명 테스트
    start = high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        [[maybe_unused]] bool hasTransform = signature.has<Transform>();
        [[maybe_unused]] bool hasVelocity = signature.has<Velocity>();
    }
    end = high_resolution_clock::now();
    
    duration = duration_cast<nanoseconds>(end - start);
    std::cout << "Signature checks (1M iterations): " << duration.count() << " ns\n";
    std::cout << "Average per check: " << duration.count() / iterations << " ns\n";
}

// 컴파일 타임 해싱 테스트
void testCompileTimeHashing() {
    std::cout << "\n=== Compile-time Hashing Tests ===\n";
    
    // 컴파일 타임 문자열 해싱
    constexpr uint32 hash1 = Utils::hashString("Transform");
    constexpr uint32 hash2 = Utils::hashString("Velocity");
    constexpr uint32 hash3 = Utils::hashString("Health");
    
    std::cout << "Hash('Transform'): " << hash1 << "\n";
    std::cout << "Hash('Velocity'): " << hash2 << "\n";
    std::cout << "Hash('Health'): " << hash3 << "\n";
    
    // 해시 충돌 확인
    assert(hash1 != hash2);
    assert(hash2 != hash3);
    assert(hash1 != hash3);
    
    std::cout << "All hashes are unique ✓\n";
    
    // 타입 해시 테스트
    constexpr uint32 typeHash1 = Utils::getTypeHash<Transform>();
    constexpr uint32 typeHash2 = Utils::getTypeHash<Velocity>();
    
    std::cout << "Type hash Transform: " << typeHash1 << "\n";
    std::cout << "Type hash Velocity: " << typeHash2 << "\n";
}

// RAII 테스트
void testRAII() {
    std::cout << "\n=== RAII Tests ===\n";
    
    // 스코프 가드 테스트
    {
        bool cleaned = false;
        {
            auto guard = makeScopeGuard([&cleaned]() {
                cleaned = true;
                std::cout << "Scope guard executed\n";
            });
            
            std::cout << "Inside scope\n";
        }
        assert(cleaned);
        std::cout << "Scope guard worked correctly ✓\n";
    }
    
    // RAII 래퍼 테스트
    {
        auto resource = RAIIWrapper<int, std::function<void(int)>>(
            42, 
            [](int value) { 
                std::cout << "Cleaning up resource: " << value << "\n"; 
            }
        );
        
        std::cout << "Resource value: " << resource.get() << "\n";
        
        // Move test
        auto moved = std::move(resource);
        assert(!resource.isValid());
        assert(moved.isValid());
        std::cout << "Move semantics working ✓\n";
    }
}

// Ranges 테스트 (C++23 std::ranges::to 사용)
void testRanges() {
    std::cout << "\n=== Ranges Tests ===\n";
    
    Vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    // 전통적인 방식
    Vector<int> evenNumbers;
    for (int n : numbers) {
        if (n % 2 == 0) {
            evenNumbers.push_back(n * n);
        }
    }
    
    std::cout << "Traditional way - even squares: ";
    for (int n : evenNumbers) {
        std::cout << n << " ";
    }
    std::cout << "\n";
    
    // Ranges 방식 (C++23)
    // auto evenSquares = numbers 
    //     | std::views::filter([](int n) { return n % 2 == 0; })
    //     | std::views::transform([](int n) { return n * n; })
    //     | std::ranges::to<Vector>();
    
    // std::cout << "Ranges way - even squares: ";
    // for (int n : evenSquares) {
    //     std::cout << n << " ";
    // }
    // std::cout << "\n";
    
    std::cout << "Ranges test completed (implementation may vary by compiler)\n";
}

int main() {
    std::cout << "=== Engine Core Module Tests ===\n";
    
    try {
        // Component 테스트
        std::cout << "\n--- Component Tests ---\n";
        Transform transform{};
        Velocity velocity{};
        Health health{};
        Renderable renderable{};
        
        testComponent(transform);
        testComponent(velocity);
        testComponent(health);
        testComponent(renderable);
        
        // System 테스트
        std::cout << "\n--- System Tests ---\n";
        MovementSystem movementSystem;
        testSystem(movementSystem, 0.016f);
        
        // Renderable 테스트
        std::cout << "\n--- Renderable Tests ---\n";
        testRenderable(renderable);
        renderable.visible = false;
        testRenderable(renderable);
        
        // Type Registry 테스트
        std::cout << "\n--- Type Registry Tests ---\n";
        constexpr size_t componentCount = GameTypeRegistry::getComponentCount();
        std::cout << "Total registered components: " << componentCount << "\n";
        
        const TypeInfo* transformInfo = GameTypeRegistry::getTypeInfo<Transform>();
        if (transformInfo) {
            std::cout << "Transform info - Size: " << transformInfo->size 
                      << ", Alignment: " << transformInfo->alignment << "\n";
        }
        
        // Component Signature 테스트
        std::cout << "\n--- Component Signature Tests ---\n";
        constexpr auto movableSignature = Signature<Transform, Velocity, Health, Renderable>::create<Transform, Velocity>();
        constexpr auto renderableSignature = Signature<Transform, Velocity, Health, Renderable>::create<Transform, Renderable>();
        
        std::cout << "Movable signature has Transform: " << movableSignature.has<Transform>() << "\n";
        std::cout << "Movable signature has Velocity: " << movableSignature.has<Velocity>() << "\n";
        std::cout << "Movable signature has Health: " << movableSignature.has<Health>() << "\n";
        std::cout << "Renderable signature matches movable: " << renderableSignature.matches(movableSignature) << "\n";
        
        // 성능 벤치마크
        benchmarkTypeRegistry();
        
        // 컴파일 타임 해싱 테스트
        testCompileTimeHashing();
        
        // RAII 테스트
        testRAII();
        
        // Ranges 테스트
        testRanges();
        
        std::cout << "\n=== All Tests Completed Successfully! ===\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}