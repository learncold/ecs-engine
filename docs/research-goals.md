# 논문 연구 목표

## 연구 주제

경량 게임 엔진 프로토타입의 3D Boids 시뮬레이션에서 OOP, Sparse Set ECS와
Archetype ECS의 성능 및 메모리 특성을 정량적으로 비교한다.

## 연구 배경

ECS(Entity-Component-System)의 성능은 ECS라는 이름 자체보다 컴포넌트 저장
방식과 반복 접근 방식에 크게 좌우된다. Sparse Set은 유연한 컴포넌트 추가와
삭제에 적합하지만 반복 쿼리에서 sparse/dense index 조회 비용이 발생할 수 있다.
Archetype은 같은 컴포넌트 조합을 가진 개체를 column 단위로 저장해 직접 순회할
수 있지만, 구조 변경 비용과 구현 복잡도가 존재한다. 연속 객체 배열을 사용하는
OOP도 동종 데이터 처리에서는 높은 지역성을 가질 수 있다.

본 연구는 동일한 단일 스레드 3D Boids workload를 세 저장 구조로 구현한다.
알고리즘과 초기 상태를 통제한 뒤 update 시간, phase별 시간, 초기화 시간과
메모리 사용량을 측정해 관측된 차이를 저장 구조와 접근 비용으로 설명한다.

## 연구 목표

1. OOP, Sparse Set ECS와 Archetype ECS로 동일한 3D Boids를 구현한다.
2. 에이전트 수에 따른 세 구현의 simulation update 성능을 비교한다.
3. Sparse Set의 query/index 비용과 Archetype의 직접 column 순회 효과를 분석한다.
4. 세 구현의 초기화 시간과 메모리 사용량을 비교한다.

## 핵심 연구 질문

### RQ1. 세 저장 구조의 update 성능은 개체 수에 따라 어떻게 달라지는가?

동일한 naive all-pairs Boids에서 OOP, Sparse Set ECS와 Archetype ECS의 전체 및
phase별 update 시간을 비교한다.

### RQ2. 저장 구조와 반복 접근 방식은 관측된 성능 차이를 어떻게 설명하는가?

Sparse Set의 sparse/dense 간접 접근과 Archetype의 직접 column 순회를 비교한다.

### RQ3. 세 저장 구조의 초기화 시간과 메모리 비용은 어떻게 다른가?

프로세스 메모리와 컨테이너 기반 저장량을 구분해 측정하고 bytes per entity를
비교한다.

## 구현 범위

### 게임 엔진 런타임 정의

본 연구에서의 게임 엔진은 게임 제작 도구 전체가 아니라, 실시간 실행과 시뮬레이션 검증에 필요한 최소 기능을 제공하는 런타임이다.

- Application 및 game loop
- Window 및 input 처리
- Time step 관리
- Scene 또는 World 관리
- ECS 기반 객체 관리
- Rendering
- Simulation update
- Debug 및 benchmark 측정

Boids 시뮬레이션은 이 엔진 런타임 위에서 실행되는 검증용 콘텐츠로 사용한다. 이를 통해 단순 알고리즘 구현이 아니라, 게임에서 대량 NPC, 군중 행동, 파티클성 객체를 처리하는 상황을 가정한 ECS 구조의 성능을 평가한다.

### 엔진 코어

- Application 및 게임 루프
- Entity 생성 및 삭제
- Component 등록, 추가, 조회
- System 등록 및 실행
- 시간 측정 및 프레임 통계

### 렌더링

- OpenGL 기반 단순 3D 렌더링
- 3D 위치 기반 에이전트 시각화
- 기본 카메라
- 저폴리 삼각형 또는 원뿔 형태의 에이전트 메시
- 대량 에이전트 렌더링을 위한 Instancing 적용

### 시뮬레이션

- Transform Component
- Velocity Component
- Boid Component
- Boid System
- Movement System

### 비교 구현

- 연속 `std::vector<BoidObject>` 기반 OOP
- 반복 순회 경로가 최적화된 Sparse Set ECS
- 동일 컴포넌트 조합을 column으로 저장하는 Archetype ECS
- 세 구현에 동일한 초기 상태, update 순서와 naive all-pairs 이웃 탐색 적용

## 실험 설계

### 실험 1. 에이전트 수에 따른 저장 구조 성능 비교

에이전트 수를 단계적으로 증가시키며 OOP, Sparse Set ECS와 Archetype ECS의
성능을 측정한다.

예상 실험 규모:

```text
100 agents
500 agents
1,000 agents
3,000 agents
5,000 agents (실행 시간이 허용할 때 확장)
```

측정 지표:

- 전체 simulation update time
- 이웃 계산, 이동, 경계 처리와 방향 갱신의 phase별 시간
- updates per second
- 초기화 시간
- memory usage 및 bytes per entity

### 실험 2. Phase별 실행 비용 비교

이웃 계산, 이동, 경계 처리와 방향 갱신 시간을 구분하여 저장 구조에 따른 차이가
어느 처리 단계에서 발생하는지 분석한다.

### 실험 3. 초기화 및 메모리 비교

세 구현의 초기화 시간, 프로세스 메모리와 컨테이너 size/capacity 기반 저장량을
측정한다. 프로세스 메모리에 포함되는 allocator와 런타임 비용을 별도로 명시한다.

## 성능 지표 정의

| 지표 | 설명 |
| --- | --- |
| Simulation update time | 한 simulation step 전체의 경과 시간 |
| Phase time | 이웃 계산, 이동, 경계 처리와 방향 갱신 구간별 시간 |
| Updates per second | 단위 시간당 완료한 simulation step 수 |
| Initialization time | N개 개체와 컴포넌트를 생성하는 시간 |
| Memory usage | 프로세스 및 시뮬레이션 컨테이너의 메모리 사용량 |

## 우선순위

### 필수 목표

1. ECS 엔진 코어 구현
2. Boids 군집 시뮬레이션 구현
3. OOP, Sparse Set ECS와 Archetype ECS의 성능 비교
4. 에이전트 수별 update time과 메모리 측정
5. 논문용 그래프와 표 생성

## 논문 결론에서 보여줄 핵심

본 연구의 결론은 단순히 게임 엔진을 제작했다는 점이 아니라, ECS와 데이터 지향 설계가 다수의 에이전트를 처리하는 시뮬레이션 환경에서 어떤 장단점을 가지는지 실험적으로 제시하는 데 있다.

최종 결과물은 다음을 포함해야 한다.

- 실행 가능한 ECS 기반 3D 시뮬레이션 엔진 프로토타입
- 3D Boids 군집 시뮬레이션 데모
- OOP, Sparse Set ECS와 Archetype ECS의 비교 실험 결과
- 에이전트 수 증가에 따른 성능 그래프
- 전체/phase별 update 시간과 메모리 비교 결과
