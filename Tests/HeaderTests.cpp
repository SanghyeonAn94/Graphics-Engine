// HeaderTests.cpp - 헤더 버전 테스트
#include "Engine/Core/Core.hpp"
#include "Engine/Core/TypeRegistry.hpp"

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

template<RenderableObject T>
void testRenderable(const T& object) {
    if (object.isVisible()) {
        std::cout << "Object is visible, rendering...\n";
        object.render();
    } else {
        std::cout << "Object is not visible, skipping render.\n";
    }
}

int main() {
    std::cout << "=== Engine Core Header Tests ===\n";
    
    try {
        // Component 테스트
        std::cout << "\n--- Component Tests ---\n";
        Transform transform{};
        Velocity velocity{};
        Health health{};
        TestComponents::Renderable renderable{};
        
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
        constexpr auto movableSignature = Signature<Transform, Velocity, Health, TestComponents::Renderable>::create<Transform, Velocity>();
        constexpr auto renderableSignature = Signature<Transform, Velocity, Health, TestComponents::Renderable>::create<Transform, TestComponents::Renderable>();
        
        std::cout << "Movable signature has Transform: " << movableSignature.has<Transform>() << "\n";
        std::cout << "Movable signature has Velocity: " << movableSignature.has<Velocity>() << "\n";
        std::cout << "Movable signature has Health: " << movableSignature.has<Health>() << "\n";
        std::cout << "Renderable signature matches movable: " << renderableSignature.matches(movableSignature) << "\n";
        
        // 컴파일 타임 해싱 테스트
        std::cout << "\n--- Compile-time Hashing Tests ---\n";
        constexpr uint32 hash1 = Utils::hashString("Transform");
        constexpr uint32 hash2 = Utils::hashString("Velocity");
        constexpr uint32 hash3 = Utils::hashString("Health");
        
        std::cout << "Hash('Transform'): " << hash1 << "\n";
        std::cout << "Hash('Velocity'): " << hash2 << "\n";
        std::cout << "Hash('Health'): " << hash3 << "\n";
        
        // RAII 테스트
        std::cout << "\n--- RAII Tests ---\n";
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
        
        std::cout << "\n=== All Header Tests Completed Successfully! ===\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}