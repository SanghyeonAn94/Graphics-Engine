# 01. Event System & Message Bus

## 강의 개요

이벤트 시스템은 현대 게임 엔진의 핵심 아키텍처 패턴 중 하나로, 시스템 간의 느슨한 결합(loose coupling)을 제공하며 확장 가능한 아키텍처를 구현하는 데 필수적인 요소입니다.

## 1. 이벤트 시스템의 정의와 필요성

### 정의
이벤트 시스템은 소프트웨어 구성요소 간에 비동기적으로 메시지를 전달하는 통신 패턴으로, 발신자(Publisher)와 수신자(Subscriber) 간의 직접적인 의존성을 제거합니다.

### 필요성
- **시스템 간 결합도 감소**: 각 시스템이 다른 시스템의 구체적인 구현을 알 필요가 없음
- **확장성**: 새로운 시스템 추가나 기존 시스템 제거가 용이
- **재사용성**: 이벤트 기반 컴포넌트는 다른 프로젝트에서 재사용 가능
- **테스트 용이성**: 각 컴포넌트를 독립적으로 테스트할 수 있음

## 2. 핵심 개념과 용어

### Publisher-Subscriber 패턴 (Pub-Sub)
- **Publisher**: 이벤트를 발생시키는 주체
- **Subscriber**: 이벤트를 구독하고 처리하는 주체
- **Event**: 시스템 간에 전달되는 데이터 객체
- **Event Bus**: 이벤트의 전달을 중개하는 중앙 집중식 메커니즘

### Observer 패턴과의 차이점
- Observer: 직접적인 참조 관계 (Subject가 Observer를 직접 알고 있음)
- Pub-Sub: 중개자(Mediator)를 통한 간접적 통신

### Message Bus vs Event Bus
- **Message Bus**: 일반적인 메시지 전달 시스템
- **Event Bus**: 특정 상태 변화나 사건에 특화된 메시지 시스템

## 3. 이벤트 시스템의 구성요소

### Event (이벤트)
- **Base Event**: 모든 이벤트의 공통 인터페이스
- **Event Type ID**: 이벤트 타입을 구분하는 고유 식별자
- **Timestamp**: 이벤트 발생 시각
- **Event Data**: 이벤트와 함께 전달되는 데이터

### Event Dispatcher (이벤트 디스패처)
- **Listener Management**: 이벤트 리스너의 등록/해제 관리
- **Event Routing**: 이벤트를 적절한 리스너들에게 전달
- **Priority Handling**: 이벤트 처리 우선순위 관리

### Event Listener (이벤트 리스너)
- **Callback Function**: 이벤트 처리를 위한 콜백 함수
- **Event Filter**: 관심 있는 이벤트만 필터링
- **Lifecycle Management**: 리스너의 생명주기 관리
- **RAII Pattern**: 스코프 기반 자동 리스너 해제

## 4. 이벤트 처리 방식

### Synchronous vs Asynchronous
- **동기식 처리**: 이벤트 발생 즉시 모든 리스너가 순차적으로 처리
- **비동기식 처리**: 이벤트를 큐에 저장하고 나중에 처리

### Immediate vs Queued vs Scheduled
- **Immediate**: 이벤트 발생 즉시 처리
- **Queued**: 큐에 저장 후 프레임 단위로 일괄 처리
- **Scheduled**: 지정된 시간에 처리 또는 주기적 실행

### Priority-based Processing
- **Critical**: 시스템 종료, 크래시 등 최우선 처리
- **High**: 입력, 물리 충돌 등 실시간 처리 필요
- **Normal**: 일반적인 게임 로직
- **Low**: UI 업데이트, 사운드 등
- **Background**: 통계, 로깅 등 백그라운드 처리

## 5. 스레드 안전성과 동시성

### Thread-Safe Event Handling
- **Mutex와 Lock**: 동시 접근 제어
- **Lock-free 자료구조**: 성능 향상을 위한 무잠금 구조
- **Reader-Writer Lock**: 읽기 작업 최적화

### Producer-Consumer 패턴
- **Single Producer Single Consumer (SPSC)**: 1:1 통신에 최적화
- **Multi Producer Single Consumer (MPSC)**: 다중 생산자, 단일 소비자
- **Multi Producer Multi Consumer (MPMC)**: 완전한 다중 통신

### Memory Ordering
- **Acquire-Release**: 메모리 순서 보장
- **Sequential Consistency**: 순차 일관성
- **Relaxed Ordering**: 성능 최적화를 위한 완화된 순서

## 6. 게임 엔진에서의 이벤트 타입

### System Events
- **System Startup/Shutdown**: 시스템 생명주기 이벤트
- **Frame Start/End**: 프레임 기반 처리 동기화
- **Scene Change**: 씬 전환 이벤트

### Input Events
- **Keyboard Events**: 키 입력 (Press, Release, Repeat)
- **Mouse Events**: 마우스 입력 (Button, Move, Wheel)
- **Touch Events**: 터치 입력 (모바일)

### Gameplay Events
- **Entity Lifecycle**: 엔티티 생성/소멸
- **Component Events**: 컴포넌트 추가/제거/수정
- **State Changes**: 게임 상태 변화

### Rendering Events
- **Window Events**: 창 크기 변경, 포커스 변화
- **Render Events**: 렌더링 파이프라인 관련
- **Resource Events**: 텍스처/메시 로딩 완료

## 7. 성능 고려사항

### Memory Management
- **Object Pooling**: 이벤트 객체 재사용
- **Memory Arena**: 연속된 메모리 할당
- **Custom Allocators**: 특화된 메모리 할당자
- **RAII (Resource Acquisition Is Initialization)**: 자원 관리 자동화

### Event Filtering
- **Type-based Filtering**: 타입별 이벤트 필터링
- **Spatial Filtering**: 공간적 범위 기반 필터링
- **Conditional Filtering**: 조건부 이벤트 처리

### Batching
- **Event Batching**: 유사한 이벤트들의 일괄 처리
- **Frame-based Processing**: 프레임당 처리 이벤트 수 제한
- **Temporal Batching**: 시간 기반 일괄 처리

## 8. 디버깅과 프로파일링

### Event Tracing
- **Event Logging**: 이벤트 발생 및 처리 로깅
- **Performance Metrics**: 처리 시간, 큐 크기 등 성능 지표
- **Event Flow Visualization**: 이벤트 흐름 시각화

### Common Issues
- **Event Loops**: 이벤트가 서로를 호출하는 무한 루프
- **Memory Leaks**: 리스너 해제 누락 (RAII 패턴으로 해결 가능)
- **Performance Bottlenecks**: 과도한 이벤트 처리
- **Listener Lifetime**: 스코프 벗어난 리스너의 안전한 해제

## 9. 실제 게임 엔진에서의 활용

### Unreal Engine
- **Delegate System**: 함수 포인터 기반 이벤트 시스템
- **Blueprint Events**: 비주얼 스크립팅을 위한 이벤트
- **Multicast Delegates**: 다중 리스너 지원

### Unity
- **UnityEvent**: Inspector에서 설정 가능한 이벤트
- **C# Events**: 언어 레벨의 이벤트 지원
- **Message System**: SendMessage 기반 통신

### Custom Implementation
- **Type Safety**: 컴파일 타임 타입 검증
- **Zero-Copy Semantics**: 불필요한 복사 최소화
- **Template Metaprogramming**: 컴파일 타임 최적화

## 10. 설계 패턴과의 연관성

### Mediator Pattern
- **중재자 역할**: Event Bus가 시스템 간 통신 중재
- **결합도 감소**: 직접적인 참조 제거

### Command Pattern
- **Command as Event**: 명령을 이벤트로 캡슐화
- **Undo/Redo**: 명령 기록을 통한 되돌리기 구현

### State Machine
- **State Change Events**: 상태 전환을 이벤트로 표현
- **Event-driven FSM**: 이벤트 기반 상태 기계

## 11. 다음 강의 연결점

다음 강의에서는 시간 관리 시스템(Time Management System)을 다루며, 이벤트 시스템과 시간 기반 스케줄링의 통합, 고정/가변 타임스텝 처리, 그리고 시뮬레이션의 정확성과 성능 간의 균형을 탐구합니다.

이벤트 시스템은 시간 관리 시스템의 기반이 되며, 프레임 기반 이벤트 처리와 시간 기반 이벤트 스케줄링을 통해 일관된 시뮬레이션 환경을 제공합니다.