# 04. Entity Component System (ECS)

## 학습 목표
- Data-Oriented Design 철학 이해
- ECS 아키텍처의 핵심 개념 습득
- 고성능 Component 저장 및 Query 시스템 구현
- Cache-friendly한 메모리 레이아웃 설계

## 강의 내용

### 4.1 전통적 OOP vs Data-Oriented Design
```cpp
// 전통적 OOP 방식 (피해야 할 패턴)
class GameObject {
    Transform transform;
    Renderer renderer;
    Physics physics;
    // 모든 객체가 모든 컴포넌트를 가짐 (메모리 낭비)
};

// ECS 방식 (권장)
struct TransformComponent { glm::mat4 matrix; };
struct RenderComponent { MeshID mesh; MaterialID material; };
// 필요한 컴포넌트만 조합
```

### 4.2 ECS 핵심 개념
- **Entity**: 고유 ID (단순한 정수)
- **Component**: 순수 데이터 구조체 (동작 없음)
- **System**: 순수 로직 프로세서 (데이터 저장 없음)
- **World**: 모든 것을 관리하는 컨테이너

### 4.3 Archetype 기반 메모리 구조
```cpp
// 동일한 컴포넌트 조합을 가진 엔티티들을 함께 저장
class Archetype {
    std::vector<TransformComponent> transforms;
    std::vector<RenderComponent> renderers;
    std::vector<EntityID> entities;
    
    // Cache-friendly iteration
    void forEach(auto&& func) {
        for (size_t i = 0; i < entities.size(); ++i) {
            func(entities[i], transforms[i], renderers[i]);
        }
    }
};
```

### 4.4 Query 시스템 설계
```cpp
// 유연한 컴포넌트 쿼리
auto renderables = world.query<TransformComponent, RenderComponent>()
                       .exclude<HiddenComponent>()
                       .build();

renderables.forEach([](Entity e, Transform& t, Render& r) {
    // 렌더링 로직
});
```

### 4.5 System 실행 순서와 의존성
- System Graph를 통한 자동 스케줄링
- 병렬 실행 가능한 System들 식별
- 데이터 의존성 분석

## 실습 과제

### Phase 1: 기본 ECS 구현
1. **Entity Manager**
   ```cpp
   class EntityManager {
       std::queue<EntityID> freeIDs;
       EntityID nextID = 1;
   public:
       EntityID create();
       void destroy(EntityID id);
   };
   ```

2. **Component Storage**
   ```cpp
   template<typename T>
   class ComponentArray {
       std::unordered_map<EntityID, size_t> entityToIndex;
       std::vector<T> components;
       std::vector<EntityID> indexToEntity;
   };
   ```

### Phase 2: Archetype 시스템
1. **Archetype 자동 생성**
2. **Entity 이동 (컴포넌트 추가/제거 시)**
3. **Query 최적화**

### Phase 3: 성능 최적화
1. **SIMD 친화적 데이터 레이아웃**
2. **Multi-threading 지원** 
3. **Memory Pool 통합**

## 🔬 성능 벤치마크
- 100만 엔티티 생성/삭제
- 복잡한 쿼리 성능 측정
- 메모리 사용량 프로파일링
- Cache miss 분석

## 📊 비교 분석
| 방식 | 메모리 효율성 | 캐시 성능 | 확장성 | 복잡도 |
|------|---------------|-----------|--------|--------|
| OOP | ❌ 낮음 | ❌ 나쁨 | ❌ 제한적 | ✅ 단순 |
| ECS | ✅ 높음 | ✅ 우수 | ✅ 뛰어남 | ⚠️ 복잡 |

## 📖 참고 자료
- "Data-Oriented Design" (Richard Fabian)
- EnTT (Entity-Component-System library)
- Unity DOTS 아키텍처
- "Overwatch Gameplay Architecture" (GDC Talk)