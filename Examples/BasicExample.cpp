// BasicExample.cpp - 기본 사용 예제
import Engine.Core;
import Engine.TypeRegistry;

#include <iostream>
#include <vector>
#include <memory>

using namespace Engine;
using namespace Engine::TestComponents;

// 게임 객체 클래스
class GameObject {
private:
    EntityID id;
    Signature<Transform, Velocity, Health, Renderable> signature;
    
    // 컴포넌트 데이터 (실제로는 별도 저장소에 보관)
    std::unique_ptr<Transform> transform;
    std::unique_ptr<Velocity> velocity;
    std::unique_ptr<Health> health;
    std::unique_ptr<Renderable> renderable;
    
public:
    explicit GameObject(EntityID entityId) : id(entityId) {}
    
    EntityID getId() const { return id; }
    
    template<Component T>
    void addComponent(T&& component) {
        if constexpr (std::is_same_v<T, Transform>) {
            transform = std::make_unique<Transform>(std::forward<T>(component));
            signature.add<Transform>();
        } else if constexpr (std::is_same_v<T, Velocity>) {
            velocity = std::make_unique<Velocity>(std::forward<T>(component));
            signature.add<Velocity>();
        } else if constexpr (std::is_same_v<T, Health>) {
            health = std::make_unique<Health>(std::forward<T>(component));
            signature.add<Health>();
        } else if constexpr (std::is_same_v<T, Renderable>) {
            renderable = std::make_unique<Renderable>(std::forward<T>(component));
            signature.add<Renderable>();
        }
        
        std::cout << "Added component to entity " << id << "\n";
    }
    
    template<Component T>
    bool hasComponent() const {
        return signature.has<T>();
    }
    
    template<Component T>
    T* getComponent() {
        if constexpr (std::is_same_v<T, Transform>) {
            return transform.get();
        } else if constexpr (std::is_same_v<T, Velocity>) {
            return velocity.get();
        } else if constexpr (std::is_same_v<T, Health>) {
            return health.get();
        } else if constexpr (std::is_same_v<T, Renderable>) {
            return renderable.get();
        }
        return nullptr;
    }
    
    const auto& getSignature() const { return signature; }
};

// 간단한 시스템들
class MovementSystem {
public:
    void update(std::vector<GameObject>& objects, float deltaTime) {
        std::cout << "\n--- Movement System Update ---\n";
        
        // Transform과 Velocity를 가진 객체들만 처리
        constexpr auto requiredSignature = Signature<Transform, Velocity, Health, Renderable>::create<Transform, Velocity>();
        
        for (auto& obj : objects) {
            if (obj.getSignature().matches(requiredSignature)) {
                Transform* transform = obj.getComponent<Transform>();
                Velocity* velocity = obj.getComponent<Velocity>();
                
                if (transform && velocity) {
                    // 위치 업데이트
                    transform->x += velocity->vx * deltaTime;
                    transform->y += velocity->vy * deltaTime;
                    transform->z += velocity->vz * deltaTime;
                    
                    std::cout << "Entity " << obj.getId() 
                              << " moved to (" << transform->x 
                              << ", " << transform->y 
                              << ", " << transform->z << ")\n";
                }
            }
        }
    }
};

class RenderSystem {
public:
    void render(const std::vector<GameObject>& objects) {
        std::cout << "\n--- Render System Update ---\n";
        
        // Transform과 Renderable을 가진 객체들만 처리
        constexpr auto requiredSignature = Signature<Transform, Velocity, Health, Renderable>::create<Transform, Renderable>();
        
        for (const auto& obj : objects) {
            if (obj.getSignature().matches(requiredSignature)) {
                const Transform* transform = obj.getComponent<Transform>();
                const Renderable* renderable = obj.getComponent<Renderable>();
                
                if (transform && renderable && renderable->isVisible()) {
                    std::cout << "Rendering entity " << obj.getId() 
                              << " at (" << transform->x 
                              << ", " << transform->y 
                              << ", " << transform->z << ")\n";
                    
                    // 실제 렌더링은 여기서...
                    renderable->render();
                }
            }
        }
    }
};

class HealthSystem {
public:
    void update(std::vector<GameObject>& objects) {
        std::cout << "\n--- Health System Update ---\n";
        
        // Health 컴포넌트를 가진 객체들만 처리
        constexpr auto requiredSignature = Signature<Transform, Velocity, Health, Renderable>::create<Health>();
        
        for (auto& obj : objects) {
            if (obj.getSignature().matches(requiredSignature)) {
                Health* health = obj.getComponent<Health>();
                
                if (health) {
                    // 체력이 0 이하인 객체 처리
                    if (health->current <= 0) {
                        std::cout << "Entity " << obj.getId() << " is dead!\n";
                    } else {
                        std::cout << "Entity " << obj.getId() 
                                  << " health: " << health->current 
                                  << "/" << health->maximum << "\n";
                    }
                }
            }
        }
    }
};

// 게임 월드 클래스
class GameWorld {
private:
    std::vector<GameObject> objects;
    EntityID nextEntityId = 1;
    
    // 시스템들
    MovementSystem movementSystem;
    RenderSystem renderSystem;
    HealthSystem healthSystem;
    
public:
    EntityID createEntity() {
        EntityID id = nextEntityId++;
        objects.emplace_back(id);
        std::cout << "Created entity with ID: " << id << "\n";
        return id;
    }
    
    GameObject* getEntity(EntityID id) {
        auto it = std::find_if(objects.begin(), objects.end(),
                              [id](const GameObject& obj) { return obj.getId() == id; });
        return it != objects.end() ? &(*it) : nullptr;
    }
    
    void update(float deltaTime) {
        std::cout << "\n======= World Update (dt=" << deltaTime << ") =======\n";
        
        // 시스템들을 순서대로 실행
        movementSystem.update(objects, deltaTime);
        healthSystem.update(objects);
        renderSystem.render(objects);
    }
    
    size_t getEntityCount() const { return objects.size(); }
};

// Concepts 사용 예제
template<Component T>
void printComponentInfo() {
    constexpr ComponentTypeID typeId = Utils::getComponentTypeID<T>();
    const TypeInfo* info = GameTypeRegistry::getTypeInfo<T>();
    
    std::cout << "Component " << typeid(T).name() << ":\n";
    std::cout << "  Type ID: " << typeId << "\n";
    std::cout << "  Size: " << sizeof(T) << " bytes\n";
    std::cout << "  Alignment: " << alignof(T) << " bytes\n";
    
    if (info) {
        std::cout << "  Registry Size: " << info->size << " bytes\n";
        std::cout << "  Registry Alignment: " << info->alignment << " bytes\n";
    }
    std::cout << "\n";
}

int main() {
    std::cout << "=== Basic Engine Example ===\n";
    
    // Type Registry 정보 출력
    std::cout << "\n--- Component Information ---\n";
    printComponentInfo<Transform>();
    printComponentInfo<Velocity>();
    printComponentInfo<Health>();
    printComponentInfo<Renderable>();
    
    // 게임 월드 생성
    GameWorld world;
    
    // 플레이어 엔티티 생성
    std::cout << "--- Creating Player Entity ---\n";
    EntityID playerId = world.createEntity();
    GameObject* player = world.getEntity(playerId);
    
    if (player) {
        // 컴포넌트 추가
        player->addComponent(Transform{10.0f, 20.0f, 5.0f});
        player->addComponent(Velocity{1.0f, 0.0f, 0.5f});
        player->addComponent(Health{100, 100});
        player->addComponent(Renderable{1, 1, true});
    }
    
    // 적 엔티티 생성
    std::cout << "\n--- Creating Enemy Entity ---\n";
    EntityID enemyId = world.createEntity();
    GameObject* enemy = world.getEntity(enemyId);
    
    if (enemy) {
        enemy->addComponent(Transform{-5.0f, 10.0f, 0.0f});
        enemy->addComponent(Velocity{-0.5f, 0.0f, 0.0f});
        enemy->addComponent(Health{50, 50});
        enemy->addComponent(Renderable{2, 2, true});
    }
    
    // 정적 오브젝트 생성 (움직이지 않는 오브젝트)
    std::cout << "\n--- Creating Static Object ---\n";
    EntityID staticId = world.createEntity();
    GameObject* staticObj = world.getEntity(staticId);
    
    if (staticObj) {
        staticObj->addComponent(Transform{0.0f, 0.0f, 0.0f});
        staticObj->addComponent(Renderable{3, 3, true});
        // Velocity나 Health는 추가하지 않음
    }
    
    // 게임 루프 시뮬레이션
    std::cout << "\n--- Game Loop Simulation ---\n";
    const float deltaTime = 0.016f; // 60 FPS
    
    for (int frame = 0; frame < 3; ++frame) {
        std::cout << "\nFrame " << frame + 1 << ":\n";
        world.update(deltaTime);
        
        // 적의 체력을 점차 감소시켜 테스트
        if (enemy) {
            Health* enemyHealth = enemy->getComponent<Health>();
            if (enemyHealth && enemyHealth->current > 0) {
                enemyHealth->current -= 25;
            }
        }
    }
    
    // RAII와 스코프 가드 예제
    std::cout << "\n--- RAII Example ---\n";
    {
        bool resourceCleaned = false;
        
        {
            auto guard = makeScopeGuard([&resourceCleaned]() {
                resourceCleaned = true;
                std::cout << "Resource automatically cleaned up!\n";
            });
            
            std::cout << "Working with resource...\n";
            // 스코프를 벗어날 때 자동으로 정리됨
        }
        
        assert(resourceCleaned);
        std::cout << "RAII working correctly!\n";
    }
    
    // 컴파일 타임 계산 예제
    std::cout << "\n--- Compile-time Computations ---\n";
    constexpr uint32 playerSignatureHash = Utils::hashString("PlayerSignature");
    constexpr ComponentTypeID transformId = Utils::getComponentTypeID<Transform>();
    
    std::cout << "Player signature hash: " << playerSignatureHash << "\n";
    std::cout << "Transform component ID: " << transformId << "\n";
    
    std::cout << "\n=== Example Completed Successfully! ===\n";
    return 0;
}