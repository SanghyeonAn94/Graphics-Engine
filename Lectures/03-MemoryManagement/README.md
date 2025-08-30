# 03. Memory Management Systems

## 학습 목표
- 게임 엔진에서 메모리 관리의 중요성 이해
- 다양한 Custom Allocator 설계 및 구현
- Cache-friendly 메모리 패턴 학습
- RAII와 Smart Pointer 활용

## 강의 내용

### 3.1 게임 엔진의 메모리 문제
- 메모리 파편화 (Fragmentation)
- 가비지 컬렉션 없는 환경에서의 관리
- 실시간 성능과 메모리 할당

### 3.2 Custom Allocators 설계
```cpp
// Stack Allocator - LIFO 순서로 할당/해제
template<size_t Size>
class StackAllocator {
    alignas(std::max_align_t) std::byte memory_[Size];
    size_t offset_ = 0;
public:
    template<typename T>
    T* allocate(size_t count = 1);
    void reset();
};

// Pool Allocator - 동일한 크기 객체들을 위한 할당자
template<typename T, size_t BlockCount>
class PoolAllocator;

// Ring Buffer - 순환 할당 (Frame-based allocation)
template<size_t Size>
class RingBufferAllocator;
```

### 3.3 메모리 정렬과 Cache 최적화
- CPU Cache Line 이해
- Structure of Arrays (SoA) vs Array of Structures (AoS)
- Data-Oriented Design 원칙

### 3.4 RAII와 Resource Management
- RAII 패턴의 활용
- Custom Smart Pointers
- Scope Guards와 Resource Cleanup

## 실습 과제
1. **StackAllocator 구현**
   - 프레임 기반 임시 메모리 할당자
   - 정렬 지원 및 오버플로우 체크

2. **PoolAllocator 구현** 
   - 동일한 크기 객체들의 효율적 할당
   - Free-list 관리

3. **메모리 프로파일러 제작**
   - 할당/해제 추적
   - 메모리 리크 감지

## 성능 측정
- 할당자별 성능 비교
- 캐시 미스 측정
- 메모리 사용량 프로파일링

## 참고 자료
- "Memory Management" - Game Engine Architecture Ch.15
- "Data-Oriented Design" (Richard Fabian)
- CPU Cache 최적화 기법들