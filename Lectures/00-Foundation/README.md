# 01. Engine Architecture & Design Patterns

## 학습 목표
- Hexagonal Architecture 원리 이해
- Clean Architecture를 게임 엔진에 적용
- 의존성 역전과 포트-어댑터 패턴 실습
- 확장 가능한 프로젝트 구조 설계

## 강의 내용

### 1.1 왜 아키텍처가 중요한가?
- 게임 엔진의 복잡성
- 유지보수성과 확장성
- 팀 개발과 모듈화

### 1.2 Hexagonal Architecture 핵심
- Domain Layer: 순수 비즈니스 로직
- Ports: 외부와의 인터페이스 정의  
- Adapters: 구체적인 구현체
- 의존성의 방향성

### 1.3 게임 엔진에서의 적용
- Rendering System의 추상화
- Platform 독립적 설계
- Testing 가능한 아키텍처

## 실습 과제
1. 프로젝트 기본 구조 생성
2. 렌더링 포트 인터페이스 설계
3. 간단한 어댑터 패턴 구현

## 참고 자료
- Clean Architecture (Robert C. Martin)
- Game Engine Architecture (Jason Gregory)
- Hexagonal Architecture 블로그 포스트들