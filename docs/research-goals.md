# 졸업 논문 통합 계획

갱신일: 2026-11-03

이 문서는 연구·실험·논문 기준을 관리한다. 작업과 진행 상태는 GitHub 이슈에서 관리한다.

## 목차

1. [연구 주제와 목표](#research)
2. [연구 질문](#questions)
3. [범위와 아키텍처 원칙](#scope)
4. [각 구현의 개요 및 공통 시뮬레이션 규칙](#implementations)
5. [실험 설계와 조건 통제](#experiments)
6. [측정 지표와 계측 경계](#metrics)
7. [논문 구성](#paper)
8. [최종 검증·제출 기준](#completion)
9. [참고문헌과 근거 자료](#references)

<a id="research"></a>

## 1. 연구 주제와 목표

### 논문 제목

국문: **Boids 시뮬레이션에서 OOP, Sparse Set ECS 및 Archetype ECS의 성능 비교**

영문: **A Performance Comparison of OOP, Sparse Set ECS, and Archetype ECS
in a Boids Simulation**

### 배경과 연구 목적

ECS(Entity-Component-System)의 실행 비용은 컴포넌트 저장 방식과 반복 접근
경로에 따라 달라진다. Sparse Set은 타입별 저장소와 sparse/dense index를 사용하고,
Archetype은 동일한 컴포넌트 조합을 column으로 저장한다. OOP는 공통 부모에서
Boid와 Dummy를 상속하고, 부모 포인터를 통해 각 객체의 데이터와 메서드에 접근한다.

동일한 단일 스레드 3D Boids Simulation을 위 세 구조로 구현하고, 알고리즘과 초기
상태를 통제하여 성능을 정량적으로 분석한다. 특정 구조가 항상
빠르다는 결론을 전제하지 않는다. 관측된 차이를 데이터 배치, 쿼리·index 접근과
phase별 처리 비용 관점에서 설명한다.

연구 목표는 다음과 같다.

1. 경량 ECS 런타임과 headless(렌더링 없이)로 실행 가능한 3D Boids 시뮬레이션을 완성한다.
2. OOP, Sparse Set ECS, Archetype ECS로 동일한 시뮬레이션을 구현·검증한다.
3. Boid·Dummy 수와 Dummy 삭제·재생성 비율에 따른 전체 step 성능을 비교한다.
4. 개체 변경·조회 준비·이웃 계산·상태 갱신 시간을 통해 저장·접근 비용을 분석한다.
5. 초기화 시간, 프로세스 최대 메모리와 컨테이너 저장량을 비교한다.
6. 원자료로 재생성 가능한 표·그래프와 RQ(Research Question)별 결론을 졸업 논문으로 제시한다.

### 선행연구와 기여

[Reynolds(1987)](https://www.red3d.com/cwr/papers/1987/boids.html)는 개별 개체의
지역적 행동으로 군집 움직임을 만드는 Boids 모델의 출발점이다. 본 구현은
그 모델에서 착안한 단순화된 3D 실험 시뮬레이션이며 원전의 모든 기능을 재현하지 않는다.

[Cox 등(2025)](https://diglib.eg.org/items/6e291ae6-e32c-4c21-a89b-021fd9986ede)은
Sparse Set과 Archetype ECS의 순회 및 개체 구성 변경 비용을 비교한다.
본 연구는 3D Boids에서 상속 기반 OOP를 비교 기준으로 추가하고,
phase별 시간, 초기화와 메모리를 함께 분석한다.
기존 연구의 결론을 그대로 재현한다고 가정하지 않는다.

<a id="questions"></a>

## 2. 연구 질문

| 연구 질문 | 필요한 근거 | 논문에서 답할 내용 |
| --- | --- | --- |
| RQ1. 세 구조의 전체 step 성능은 개체 수와 Dummy 변경 비율에 따라 어떻게 달라지는가? | 동일 조건의 전체 `ms/step`, 산포, 구현 간 시간 비율과 신뢰구간 | 개체 수·변경 비율에 따른 성능 차이 |
| RQ2. 저장 구조 변경과 데이터 순회·조회 방식은 성능 차이와 어떤 관계가 있는가? | 네 측정 구간의 시간, 데이터 순회·조회 방식과 가능한 캐시 측정 | 개체 변경·조회 준비 비용과 계산 비용의 차이 |
| RQ3. 세 구조의 초기화 시간과 메모리 비용은 어떻게 다른가? | 초기화 범위·reserve 정책, 프로세스 메모리, 보조 구조를 포함한 컨테이너 저장량 | 생성 비용, 용량 정책, bytes per entity |

<a id="scope"></a>

## 3. 범위와 아키텍처 원칙

### 경량 게임 엔진의 정의

게임 엔진은 실시간 실행과 시뮬레이션 검증에 필요한 최소 런타임을 의미한다.

- 시뮬레이션 초기화·종료 및 고정 시간 간격의 반복 실행
- World와 ECS 기반 객체 관리: Entity 생성·삭제, Component 추가·조회, System 실행
- Boids 행동 계산과 위치·속도·방향 갱신
- Headless benchmark와 시간·메모리 측정

Boids는 이 런타임 위에서 실행되는 연구용 시뮬레이션이다. 대량 NPC·군중·파티클성 객체를
처리하는 상황을 단순화하지만, 결과를 모든 게임 workload에 일반화하지 않는다.
주 실험은 창과 OpenGL 컨텍스트 없이 CPU simulation만 실행한다.

### 기술과 모듈 경계

| 항목 | 기준 |
| --- | --- |
| 언어·빌드 | C++20, CMake, Visual Studio 2022 기본 검증, Release 성능 측정 |
| 의존성 | vcpkg 사용, 연구에 필요한 의존성만 추가 |
| `Engine/` | ECS, CPU 상태 데이터와 계측 도구 등 재사용 런타임 |
| `Boids/Common/` | 공통 설정과 `BoidParameters`; Engine/ECS에 의존하지 않음 |
| `Boids/OOP/` | 상속 기반 객체 시뮬레이션; `Engine/ECS`에 의존하지 않음 |
| `Boids/ECS/` 및 Archetype 구현 | ECS 기반 저장·시스템과 연구 도메인 행동 |
| `App/` | 구현 선택, 콜백 연결, 옵션·출력을 담당하는 얇은 조립 계층 |
| `Benchmarker/` | 실험 실행 순서와 결과 수집·직렬화; Boids를 직접 실행하고 Engine 계측 API 사용 |

`Engine`에 Boids 행동을 넣지 않는다. `App`에 ECS 쿼리나 Boids 시스템을 구현하지
않는다. 위치·회전·크기 데이터와 관련 계산 코드는 OpenGL 렌더링 코드와 분리한다. 시뮬레이션에서는 GLM 수학 라이브러리만 사용하는 Engine::RenderData Cmake 빌드 타겟을 통해 이를 사용한다. 시뮬레이션에 OpenGL 실행 의존성을 추가하지 않는다.
헤더와 소스는 기능별 폴더에 함께 둔다.

### 제외 범위

렌더링·시각화의 추가 구현과 FPS·frame time·render GPU 관련 측정은 프로젝트
완료 범위와 논문에서 제외한다. 3D는 위치·속도·행동 계산의 공간 차원을 뜻한다.
(외부 대회 포스터 시각 자료 용으로만 제한적으로 사용할 예정)

<a id="implementations"></a>

## 4. 각 구현의 개요 및 공통 시뮬레이션 규칙

### 최종 비교 경로

| 구현 | 저장 방식 | 주요 접근 경로 |
| --- | --- | --- |
| OOP | `std::vector<std::unique_ptr<SimulationObject>>` | 공통 부모 포인터로 객체에 접근하고 메서드 호출 |
| Sparse Set ECS | 타입별 sparse index, dense entity·component 배열 | 이웃 `CachedView`의 Entity·저장소별 dense index 재사용 |
| Archetype ECS | Boid·Dummy의 component 조합별 column 배열 | Boid에 해당하는 묶음의 column과 행 범위 직접 순회 |

OOP의 공통 부모 `SimulationObject`는 Transform과 Velocity를 가진다.
`BoidObject`는 이를 상속하고 Acceleration과 BoidParameters를 추가하며,
`DummyObject`는 공통 데이터만 가진다. 두 종류를 같은 부모 포인터 배열에 저장하고
각 객체의 수명은 `unique_ptr`로 관리한다. 연속된 것은 포인터 배열이며 객체 데이터가
연속 저장된다고 가정하지 않는다.

### Dummy와 개체 변경

- ECS의 Dummy는 `Transform + Velocity`를 가지며 `BoidParameters`는 가지지 않는다.
  OOP의 DummyObject도 공통 부모의 Transform과 Velocity만 가진다.
- Dummy는 이웃 탐색을 포함한 모든 Boids 계산·상태 갱신에서 제외한다.
- 초기 생성은 Boid와 Dummy를 번갈아 배치하고, 남는 개체는 이어서 생성한다.
- 매 step Dummy 중 `n%`를 삭제하고 같은 수·같은 구성으로 재생성한다.
- `n`은 실행 파라미터로 지정한다. 범위는 `0~100`, 변경 수는 `floor(D × n / 100)`이다.
  `D`는 Dummy 수이며 `n = 0`은 변경 없는 조건이다. 비율과 실제 변경 수를 함께 기록한다.
- 삭제 대상은 고정 seed로 선택하고 세 구현에서 같은 논리적 Dummy를 선택한다.
  대상 선택 계획은 측정 전에 준비한다. Boid 수와 step 종료 시 Dummy 수는 유지한다.
- 논리 번호는 최초 생성 순서의 index이며 삭제·재생성 후에도 유지한다.
  각 step의 대상 목록은 중복 없이 선택해 번호 오름차순으로 적용한다.
  대상 Dummy를 모두 삭제한 뒤 같은 순서로 재생성하고, 각각의 최초 위치·속도로 되돌린다.
  계획 생성과 난수 선택은 실제 삭제·재생성 구간에서 제외한다.
- warmup과 측정에 사용할 계획을 함께 준비한다. 측정 시작 시 시뮬레이션 상태를
  초기화하지 않고 warmup 다음 계획부터 이어서 사용한다.

### 공통 상태와 update 순서

동일한 생성 순서, seed, 초기 위치·속도, 파라미터와 fixed timestep을 사용한다.
한 step은 **개체 변경 → 조회 준비 → 이웃 계산 → 상태 갱신** 순서로 수행한다.
이웃 계산은 모든 Boid의 갱신 전 위치·속도를 사용한다. 상태 갱신은 acceleration
반영, 속도 제한·위치 적분, 경계 처리, 최종 velocity에 따른 방향 갱신 순서다.

상태 저장과 일반 계산에는 float를 사용한다. 방향 정규화·크기 제한·반경 비교의
제곱값이 float 범위를 벗어나면 해당 계산만 double로 보완하며 세 구현에 같은
규칙을 적용한다. 이웃 합산 등에서 생긴 비유한 벡터가 정규화·크기 제한에
전달되거나 가속도·적분한 위치를 float로 저장할 수 없으면 실행을 실패 처리한다.
속도·alignment 제한이 0이어도 같은 실패 규칙을 적용하며, 비유한 위치를
경계 충돌로 간주해 정상 범위로 되돌리지 않는다.
이런 실행을 정상 측정 원자료에 포함하지 않는다.

<a id="experiments"></a>

## 5. 실험 설계와 조건 통제

### 실험 조건

세 구현을 같은 조건에서 비교하는 하나의 벤치마크 실험으로 구성한다.
구현·Boid 수·Dummy 수·변경 비율별로 독립 프로세스를 반복 실행하며, 각 실행에서
전체·네 구간 시간, 초기화 시간과 메모리 지표를 함께 수집한다.

| 변경할 조건 | 값 |
| --- | --- |
| 구현 | OOP / Sparse Set CachedView / Archetype |
| Boid·Dummy 수 | 실행 설정으로 지정 |
| Dummy 삭제·재생성 비율 | `n = 0`(변경 없음)과 지정한 `n > 0` 조건 |

논문 측정의 시작안은 다음과 같다. 최종 수치는 예비 측정의 소요 시간과 변동을
확인한 뒤 확정하며, 결과를 보고 특정 구현에 유리한 조건만 고르지 않는다.

| 조건 | 시작안 |
| --- | --- |
| Boid 수 B | 256 / 512 / 1,024 |
| Dummy 수 D | 0 / B / 4B |
| Dummy 변경률 | 0 / 10 / 50 / 100%; D = 0일 때는 0%만 실행 |
| warmup | 200 step |
| 측정 | 5,000 step |
| 독립 프로세스 반복 | 조건별 10회 |
| 초기 상태·변경 계획 seed / dt | 42 / 0.016666667초 |
| 실행 순서 | 반복 안에서 조건을 섞음; 순서용 seed 2026, 시뮬레이션 seed와 별도 |

이 조합은 27가지 개체·변경률 조건 × 세 구현 × 10회로 총 810회다.
`benchmarks/thesis.json`은 이 시작안을 표현하며 확정된 논문 원자료로 간주하지 않는다.
`benchmarks/pilot.json`은 각 B에서 D = 4B·변경률 100%, 세 구현, warmup 200·측정
200 step·3회(총 27회)로 실행 시간의 규모를 먼저 확인한다. 예비 결과는 본 측정과
다른 폴더에 보존하고 통계에 섞지 않는다. 최대 변경 조건이 가장 느리다고 가정하지는 않는다.

2026-11-03의 최초 예비 측정은 27회 모두 성공했고 약 30초가 걸렸다.
B = 256·D = 1,024·변경률 100%에서 가장 짧은 평균 step은 약 0.275ms로,
200 step 구간이 약 55ms였다. 이 조건의 세 번 실행에서 최대 약 10%의 변동계수가
관측되어 측정 시작안을 1,000에서 5,000 step으로 늘렸다. 이는 실행 시간 규모를
확인한 결과이며 세 구현의 최종 우열이나 안정적인 warmup을 확인한 근거는 아니다.
긴 실행에서는 군집 상태가 더 변하므로 예비 ms/step을 그대로 외삽하지 않는다.

예비 측정에서 warmup 100·200·500 step도 비교해 초기 과도 현상이 충분히
제외되는지 확인한다. warmup은 군집의 상태도 바꾸므로 세 구현에 같은 값을 적용한다.
본 측정의 실제 측정 구간이 너무 짧으면 가장 빠른 조건에서도 대략 1초 이상이 되도록
측정 step 수를 늘리는 것을 출발 기준으로 삼는다. 이는 신뢰성을 보장하는 규칙은 아니며,
최종 warmup·측정 step 수는 전체 비교 조건에 공통으로 적용한다. 실행 중에 구현마다
step 수를 자동 조정하지 않는다. 같은 조건에서 M이 바뀌면 관측할 궤적과 공통 입력
메모리도 바뀌므로 확정 후에는 중간 결과와 혼합하지 않는다.

통계의 반복 단위는 독립 프로세스 한 번의 평균 ms/step이다. 연속된 5,000 step을
독립 표본 5,000개로 취급하지 않는다. 조건별 평균·표준편차와 95% 신뢰구간을 보고하고,
구현 간 비율도 실행 단위로 불확실성을 추정한다. 10회 예비 반복에서 평균의 신뢰구간
반폭이 평균의 5%를 넘으면 본 측정 전 전체 반복 수를 20회 이상으로 늘리는 것을
검토한다. 이 5%는 정밀도 목표이며, 구현 간 차이가 작으면 더 좁은 구간이 필요하다.
일부 조건의 결과가 유리해질 때까지 선택적으로 반복하지 않는다.
같은 seed의 반복은 실행 잡음을 평가하며 초기 상태에 대한 일반화까지 보장하지 않는다.
이를 확인할 때는 대표 조건에 추가 seed를 적용하고 별도 결과로 보고한다.

warmup과 반복 제어는 [Google Benchmark 문서](https://github.com/google/benchmark/blob/main/docs/user_guide.md),
실행 단위 변동과 신뢰구간을 고려하는 원칙은
[Kalibera·Jones의 연구](https://kar.kent.ac.uk/33611/45/p63-kaliber.pdf)를 참고했다.
위 수치와 정밀도 목표는 본 프로젝트의 예비 측정용 제안이다.

이 결과로 6-1과 6-2의 측정 지표를 측정하게 된다.
6-3의 캐시 측정은 별도의 벤치마킹 시도에서 Vtune같은 외부 프로그램으로 측정한다.

### 조건 통제

- Visual Studio 2022 Release, 동일 compiler·최적화 옵션 사용
- 단일 스레드 headless 실행, benchmark 프로세스 동시 실행 금지

실행 조건(Boid·Dummy 수, 변경 비율, seed, dt, warmup·측정 step 수)과 환경
(CPU·OS·컴파일러·빌드 옵션)을 결과에 함께 기록하고 실행별 원자료를 보존한다.

<a id="metrics"></a>

## 6. 측정 지표와 계측 경계

### 6-1. 시간 지표

| 지표 | 정의 | 별도 계측 필요 여부 | 비고 |
| --- | --- | --- | --- |
| A - 전체 step `ms/step` | 개체 변경부터 상태 갱신 종료까지의 경과 시간을 합산한 뒤 measured steps로 나눈 값 | O | 초기화, warmup과 결과 출력은 전체 step 측정에서 제외한다. |
| B - Updates per second | `1,000 / mean_step_ms`; 단위 시간당 simulation step 처리율 | A로부터 유도 | |
| C - 구간별 `ms/step` | 아래 네 구간 각각의 step당 시간 | O | |
| D - 이웃 Pair당 비용 |  `이웃 계산 ms/step % (N × (N − 1))`; N > 1 | C로부터 유도 | |
| E - 구현 간 시간 비율 | 기준 구현 시간 / 비교 구현 시간; 분자·분모와 통계 방법을 명시 | A나 C로부터 유도 | 같은 실험 조건에서 동일한 지표끼리 계산|
| F- Initialization time | Boid·Dummy와 component를 생성하는 범위를 명시한 초기화 경과 시간 | O | |

| 측정 구간 | 포함할 작업 | 현재 대응 System (Sparse Set ECS 기준) | RQ2에서 설명할 차이 |
| --- | --- | --- | --- |
| 개체 변경 | Dummy 삭제와 재생성 | TODO | 저장 배열의 원소 이동, index 수정, 생성·할당 비용 |
| 조회 준비 | 구조 변경 확인, 대상 목록·index 캐시 갱신 | `BoidSystem` 내부의 캐시 확인·생성·갱신 | 변경 후 다시 데이터를 순회할 준비 비용 |
| 이웃 계산 | 이웃 순회, 거리 검사, Separation·Alignment·Cohesion과 가속도 계산 | `BoidSystem` 내부의 이웃 순회·가속도 계산 | 반복적인 위치·속도 조회에서 나타나는 차이 |
| 상태 갱신 | 속도·위치 갱신, 경계 처리, 방향 갱신 | `MovementSystem` → `BoundarySystem` → `BoidOrientationSystem` | 개체를 순회하며 데이터를 읽고 쓰는 비용 차이 |

조회 준비에는 각 구현에서 실제로 필요한 작업만 포함한다. 별도 준비가 없는 구현에
같은 캐시를 억지로 추가하지 않는다.

전체 시간과 네 구간 시간은 같은 실행에서 측정한다. 전체 시간에는 구간별 계측 비용도
포함되며, 네 구간 계측의 합계가 콜백·디스패치·계측 비용까지 포함한 전체 시간과 같다고 가정하지 않는다.

최종 비교에서는 공통 초기 상태와 변경 계획을 초기화 계측 전에 생성한다.
초기화 시간은 준비된 입력으로 실제 객체·Entity·component와 관리용 저장소를
구성하는 범위이며, 해당 범위의 clear·reserve·할당·입력 적용을 포함한다.
공통 입력 생성, 설정 검증과 시뮬레이션 생성자 호출은 제외한다.

### 6-2. 메모리 지표

| 지표 | 범위와 한계 | 별도 계측 필요 여부 | 비고 |
| --- | --- | --- | --- |
| G - Peak Private Commit / Peak Working Set | OS가 기록한 프로세스 전체 실행의 최대값 | O (OS 조회) | 시간 측정 종료 후 `PeakPagefileUsage`와 `PeakWorkingSetSize`를 각각 조회한다. 초기화·warmup과 allocator·런타임 비용을 포함하며, 측정 step만의 최대값으로 해석하지 않는다. |
| H - Container size 기반 저장량 | 사용 중인 원소 수 × 원소 크기 및 보조 구조 저장량 | O (size 조회 후 계산) | 측정 종료 시점의 사용 중인 데이터 저장량이다. 프로세스 최대 메모리와 구분하고, allocator overhead·fragmentation은 포함하지 않는다. |
| I - Container capacity 기반 저장량 | 할당된 용량 × 원소 크기 및 보조 구조 저장량 | O (capacity 조회 후 계산) | 측정 종료 시점에 확보한 저장 공간이다. 프로세스 최대 메모리와 구분하고, allocator overhead·fragmentation은 포함하지 않는다. |
| J - Bytes per entity | 선택한 저장량 / 전체 개체 수(Boid + Dummy) | G·H·I로부터 유도 | 프로세스 최대값·사용량·할당량 중 어떤 값을 사용했는지 명시한다. |

컨테이너 추정에는 OOP의 포인터 배열과 개별 객체 저장량, Entity/generation/free list,
sparse/dense 배열, CachedView와 Archetype column을 포함한다. 객체 내부에 포함된 데이터를 중복 계산하지 않는다.
미지원·미측정 값은 실제 0과 구분하여 상태·사유와 함께 저장한다.

실험 실행기가 소유한 공통 초기 상태·변경 계획은 실험 종료까지 유지하며,
그 저장량을 실험 입력 항목으로 별도 보고한다. 시뮬레이션의 저장량과 구분하고
같은 할당을 양쪽에 더하지 않는다. 프로세스 최고값에는 공통 입력과 계측 도구의
저장 공간도 포함된다.

측정 step 수를 늘리면 미리 생성한 변경 계획도 커진다. 시작안의 최대 조건
(D = 4,096·변경률 100%·총 5,200 step)은 대상 논리 번호 배열만 약 162.5MiB다.
프로세스 최고값이 이 공통 입력의 영향을 크게 받을 수 있으므로 입력 저장량을 함께
제시하고, 프로세스 최고값의 차감으로 시뮬레이션만의 최대 메모리를 추정하지 않는다.

### 6-3. 캐시 지표

CPU가 데이터를 읽을 때 L1 캐시에서 찾으면 **L1 hit**, 찾지 못하면 **L1 miss**다.
L3 miss는 L3 캐시에서도 찾지 못한 경우다. 같은 시뮬레이션 조건에서 세 구현의
캐시 사용 차이를 살펴보는 보조 근거로 사용한다.

| 지표 | 정의 | 별도 계측 필요 여부 | 비고 |
| --- | --- | --- | --- |
| K - L1 hit·miss 추정 횟수 | 데이터를 읽을 때 L1 캐시에서 찾은 횟수와 찾지 못한 횟수의 추정값 | O (VTune) | `MEM_LOAD_RETIRED.L1_HIT`와 `MEM_LOAD_RETIRED.L1_MISS`를 각각 수집한다. |
| L - L1 miss 비율 | `L1 miss / (L1 hit + L1 miss) × 100` (%) | K로부터 유도 | 샘플 수가 아닌 추정 횟수로 계산한다. 분모가 0이면 계산하지 않는다. |
| M - L3 miss 추정 횟수 | 데이터를 읽을 때 L3 캐시에서도 찾지 못한 횟수의 추정값 | O (VTune) | `MEM_LOAD_RETIRED.L3_MISS`를 수집한다. L3 hit는 수집 대상이 아니므로 L3 miss 비율은 계산하지 않는다. |

VTune은 모든 hit·miss를 하나씩 기록하는 대신, 일정 횟수마다 **샘플**을 기록한다.
샘플 수와 샘플링 간격을 반영한 **추정 횟수**를 구분한다. 예를 들어 hit는 1,000회마다,
miss는 100회마다 기록했다면 두 샘플 수를 그대로 나눠 miss 비율을 구할 수 없다.
추정 횟수는 VTune의 `Hardware Event Count`, 샘플 수는 `Hardware Event Sample Count`를
사용한다. [VTune 지표 설명](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2023-0/window-summary-hardware-events.html)

캐시 측정은 일반 시간·메모리 벤치마크와 별도로 실행한다.

<a id="paper"></a>

## 7. 논문 구성

| 장 | 작성 내용 |
| --- | --- |
| 1. 서론 | 연구 배경, 목적과 연구 질문 |
| 2. 이론적 배경 | OOP, ECS·DOD, Sparse Set·Archetype과 Boids 알고리즘 |
| 3. 설계 및 구현 | 세 구현의 저장·조회 방식, Boid·Dummy 구성과 공통 처리 순서 |
| 4. 실험 방법 | 실행 환경과 조건, 측정 지표와 분석 방법 |
| 5. 결과 | 조건별 시간·메모리·캐시 측정 결과 |
| 6. 논의 | 관측된 성능 차이의 해석과 연구 질문에 대한 답 |
| 7. 한계 | 실험 범위와 결과를 일반화하기 어려운 부분 |
| 8. 결론 | 연구 결과 요약과 후속 연구 방향 |

표·그림은 실제 결과를 설명하는 데 필요한 것만 작성하고 단위와 실험 조건을 표시한다.

<a id="completion"></a>

## 8. 최종 검증·제출 기준

- 세 구현이 같은 조건에서 같은 시뮬레이션 결과를 내는지 허용 오차 내에서 검증한다.
- 최종 측정 결과로 RQ1~RQ3에 답하고, 실험 범위와 해석의 한계를 논문에 명시한다.
- 실행 가능한 소스, 측정 원자료와 실행·분석 방법, 학교 제출 형식에 맞춘 논문을 준비한다.

<a id="references"></a>

## 9. 참고문헌과 근거 자료

- Reynolds, C. W. (1987). *Flocks, Herds, and Schools: A Distributed Behavioral
  Model*. Computer Graphics, 21(4), 25–34.
  [저자 제공 원전](https://www.red3d.com/cwr/papers/1987/boids.html)
- Cox, L., Williams, B., Vickers, J., Ward, D., & Headleand, C. (2025).
  *Run-time Performance Comparison of Sparse-set and Archetype Entity-Component
  Systems*. Computer Graphics and Visual Computing (CGVC), The Eurographics Association.
  [출판사 자료](https://diglib.eg.org/items/6e291ae6-e32c-4c21-a89b-021fd9986ede),
  [DOI](https://doi.org/10.2312/cgvc.20251224)
