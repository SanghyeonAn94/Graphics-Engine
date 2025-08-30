# Next-Generation Graphics Engine Lectures

완전한 게임 엔진 구현을 위한 단계별 강의 시리즈입니다.

## 강의 목차 (Curriculum)

### Part 1: Foundation & Architecture (1-4강)

#### **01. Engine Architecture & Design Patterns**
- Hexagonal Architecture 원리
- Clean Architecture in Game Engines
- 의존성 역전과 포트-어댑터 패턴
- 프로젝트 구조 설계
- **실습**: 기본 프로젝트 구조 생성

#### **02. Modern C++ for Game Engines**  
- C++23/26 핵심 기능
- Concepts, Modules, Ranges
- Template metaprogramming
- RAII와 Smart Pointers
- **실습**: 모던 C++ 엔진 기반 코드 작성

#### **03. Memory Management Systems**
- Custom Allocators 설계
- Stack, Pool, Ring Buffer Allocators  
- Memory Alignment와 Cache Optimization
- RAII와 Resource Management
- **실습**: 고성능 메모리 관리자 구현

#### **04. Entity Component System (ECS)**
- Data-Oriented Design 원리
- Component, Entity, System 아키텍처
- Archetype 기반 메모리 레이아웃
- Query 시스템과 성능 최적화
- **실습**: 완전한 ECS 시스템 구현

---

### Part 2: Core Systems (5-8강)

#### **05. Multi-Threading & Job System**
- 병렬 프로그래밍 기초
- Lock-free 자료구조
- Job System 아키텍처
- Work-Stealing Queue
- **실습**: 고성능 Job 스케줄러 구현

#### **06. Platform Abstraction Layer**
- Cross-platform 윈도우 시스템
- 입력 처리 (키보드, 마우스, 게임패드)
- 파일 시스템 추상화
- Hot-Reloading 시스템
- **실습**: 플랫폼 독립적 기반 시스템

#### **07. Asset Pipeline & Serialization**
- 에셋 로딩 시스템
- Binary Serialization
- Reflection System 구현
- Hot-Reloading for Assets
- **실습**: 완전한 에셋 파이프라인

#### **08. Profiling & Debugging Tools**
- CPU/GPU 프로파일링
- Memory Leak Detection
- Performance Counter 시스템
- Visual Debugger 구현
- **실습**: 통합 프로파일링 툴

---

### Part 3: Rendering Foundation (9-12강)

#### **09. Graphics API Abstraction**
- Vulkan vs DirectX 12 vs Metal
- Command Buffer 패턴
- Resource Management
- Synchronization Primitives
- **실습**: 공통 렌더링 추상화 레이어

#### **10. Vulkan Deep Dive** 
- Vulkan 초기화와 설정
- Pipeline State Objects
- Descriptor Sets & Bindless
- Dynamic Rendering (Vulkan 1.3)
- **실습**: 기본 Vulkan 렌더러

#### **11. Shader System & Compilation**
- HLSL/GLSL to SPIR-V
- Shader Reflection
- Hot-Reloading Shaders
- Shader Variants & Permutations
- **실습**: 완전한 셰이더 컴파일 시스템

#### **12. Basic Rendering Pipeline**
- Forward vs Deferred Rendering
- Depth Testing & Culling
- Basic Lighting Models
- Texture Management
- **실습**: 첫 번째 렌더링된 삼각형

---

### Part 4: Advanced Rendering (13-16강)

#### **13. GPU-Driven Rendering**
- Indirect Drawing Commands  
- GPU Frustum Culling
- Bindless Textures
- Multi-Draw Indirect
- **실습**: GPU 기반 렌더링 파이프라인

#### **14. Modern Lighting & Shading**
- Physically Based Rendering (PBR)
- Image-Based Lighting (IBL)
- Real-time Global Illumination
- Shadow Techniques
- **실습**: PBR 머티리얼 시스템

#### **15. Ray Tracing Integration**
- Hardware Ray Tracing 기초
- Acceleration Structures (BLAS/TLAS)
- RT Reflections & Shadows
- Hybrid Rendering Pipeline
- **실습**: 실시간 레이트레이싱

#### **16. Post-Processing & Effects**
- HDR & Tone Mapping
- Bloom, DOF, Motion Blur
- Anti-Aliasing (TAA, DLSS)
- Screen-Space Effects
- **실습**: 완전한 포스트 프로세싱 파이프라인

---

### Part 5: Optimization & Production (17-20강)

#### **17. Performance Optimization**
- CPU/GPU 병목 분석
- SIMD 최적화
- Cache-Friendly 자료구조
- Draw Call Batching
- **실습**: 엔진 성능 최적화

#### **18. Level-of-Detail (LOD) Systems**
- Geometric LOD
- Texture Streaming
- Audio LOD
- Adaptive Quality Settings
- **실습**: 동적 LOD 시스템

#### **19. Networking & Multiplayer**
- Client-Server Architecture
- State Synchronization
- Lag Compensation
- Distributed Rendering
- **실습**: 기본 멀티플레이어 지원

#### **20. Scripting & Editor Integration**
- Scripting Language 통합
- Visual Node Editor
- Runtime Hot-Reloading
- In-Engine Debugging
- **실습**: 완전한 게임 엔진 에디터

---

## 학습 로드맵

### 초급 (1-8강): **Engine Foundation**
게임 엔진의 핵심 아키텍처와 기본 시스템들을 구축합니다.

### 중급 (9-16강): **Graphics Programming** 
현대적인 렌더링 파이프라인과 고급 그래픽스 기법을 학습합니다.

### 고급 (17-20강): **Production Ready**
상용 엔진 수준의 최적화와 고급 기능들을 완성합니다.

---

## 학습 목표

이 강의를 완료하면 다음을 할 수 있게 됩니다:

- **AAA급 게임 엔진** 아키텍처 설계
- **Vulkan/DirectX 12** 기반 고성능 렌더러 구현  
- **ECS + 멀티스레딩** 현대적 엔진 시스템
- **Ray Tracing + GPU-Driven** 최첨단 그래픽스
- **Hot-Reloading + 에디터** 개발 도구 제작

---

## 강의 자료 구조

```
Lectures/
├── 01-Foundation/
│   ├── slides.md           # 강의 슬라이드
│   ├── code/              # 실습 코드
│   ├── exercises/         # 연습 문제
│   └── resources/         # 참고 자료
├── 02-CoreSystems/
│   └── ...
└── README.md              # 이 파일
```

---

> **"The best way to learn game engine development is by building one."**

**목표**: 세계 최고 수준의 게임 엔진을 처음부터 완성하는 여정!