# 02. Modern C++ for Game Engines

## 학습 목표
- C++23/26 최신 기능을 게임 엔진에 적용
- Concepts와 Modules를 활용한 타입 안전성 확보
- Template metaprogramming으로 제로비용 추상화 구현
- Ranges와 Coroutines를 통한 현대적 코드 작성

## 강의 내용

### 2.1 C++23/26 핵심 기능
```cpp
// Modules - 컴파일 시간 단축과 캡슐화
export module Engine.Core;
import <memory>;
import <vector>;

export namespace Engine {
    template<typename T>
    concept Component = requires(T t) {
        sizeof(T) > 0;
        std::is_trivially_copyable_v<T>;
    };
}
```

### 2.2 Concepts를 통한 타입 제약
```cpp
// 컴파일 타임 타입 검증
template<typename T>
concept Renderable = requires(T obj, RenderContext& ctx) {
    obj.render(ctx);
    { obj.getBounds() } -> std::convertible_to<BoundingBox>;
    { obj.isVisible() } -> std::convertible_to<bool>;
};

// 사용 예시
template<Renderable T>
void submitToRenderer(const T& object) {
    if (object.isVisible()) {
        renderer.submit(object);
    }
}
```

### 2.3 Ranges를 활용한 함수형 프로그래밍
```cpp
// 전통적 방식
std::vector<Entity> visibleEntities;
for (const auto& entity : allEntities) {
    if (entity.isVisible() && camera.canSee(entity)) {
        visibleEntities.push_back(entity);
    }
}

// Ranges 활용
auto visibleEntities = allEntities 
    | std::views::filter([](const Entity& e) { return e.isVisible(); })
    | std::views::filter([&](const Entity& e) { return camera.canSee(e); })
    | std::ranges::to<std::vector>();
```

### 2.4 Template Metaprogramming
```cpp
// 컴파일 타임 컴포넌트 타입 등록
template<typename... Components>
class ComponentRegistry {
    static constexpr size_t component_count = sizeof...(Components);
    
    template<typename T>
    static constexpr size_t getComponentIndex() {
        return getIndexImpl<T, Components...>();
    }
    
private:
    template<typename T, typename First, typename... Rest>
    static constexpr size_t getIndexImpl() {
        if constexpr (std::is_same_v<T, First>) {
            return 0;
        } else {
            return 1 + getIndexImpl<T, Rest...>();
        }
    }
};
```

### 2.5 RAII와 Resource Management
```cpp
// 자동 리소스 관리
class VulkanBuffer {
private:
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    VmaAllocator allocator;
    
public:
    VulkanBuffer(VmaAllocator alloc, size_t size) : allocator(alloc) {
        // 버퍼 생성
    }
    
    ~VulkanBuffer() {
        if (buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, buffer, allocation);
        }
    }
    
    // Move-only semantics
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;
    VulkanBuffer(VulkanBuffer&&) = default;
    VulkanBuffer& operator=(VulkanBuffer&&) = default;
};
```

### 2.6 Coroutines를 활용한 비동기 처리
```cpp
// 비동기 에셋 로딩
Task<std::unique_ptr<Texture>> loadTextureAsync(const std::string& path) {
    auto fileData = co_await loadFileAsync(path);
    auto imageData = co_await decodeImageAsync(fileData);
    auto texture = co_await createGPUTextureAsync(imageData);
    co_return texture;
}
```

## 실습 과제

### Phase 1: Modules 설정
1. **Engine.Core 모듈 생성**
2. **CMake 설정 업데이트**
3. **기본 타입 정의**

### Phase 2: Concepts 시스템
1. **Component Concept 정의**
2. **System Concept 구현**
3. **컴파일 타임 검증 테스트**

### Phase 3: Template 시스템
1. **Type Registry 구현**
2. **Compile-time 문자열 해싱**
3. **Zero-cost 추상화 벤치마크**

## 성능 고려사항
- 컴파일 시간 vs 런타임 성능 트레이드오프
- Template instantiation 비용
- Concept 검증 오버헤드
- 인라인화와 최적화

## 도구와 환경
- 컴파일러 지원 상황 (GCC 13+, Clang 16+, MSVC 19.3+)
- CMake 3.25+ 설정
- IDE 지원 (VS Code, Visual Studio, CLion)

## 참고 자료
- "C++20 - The Complete Guide" (Nicolai Josuttis)
- "Template Metaprogramming with C++" (Marius Bancila)
- cppreference.com C++23/26 features