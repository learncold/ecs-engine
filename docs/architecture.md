# 아키텍처

이 문서는 현재 코드의 모듈 경계, 저장 방식과 실행 흐름을 설명한다.
연구에서 요구하는 비교 구조와 실험 기준은 [연구·실험·논문 기준](research-goals.md)을 따른다.

## 모듈 책임과 의존 관계

| 모듈 | 책임 |
| --- | --- |
| `Engine/` | ECS 저장·조회, CPU Transform 계산, 공통 벤치마크와 기존 그래픽 런타임 |
| `Engine/ECS/Archetype/` | 컴포넌트 조합별 column 저장, Entity 수명·행 위치와 저장량 보고 |
| `Boids/Common/` | 공통 설정·검증·행동 수식과 초기 상태·Dummy 변경 계획 생성 |
| `Boids/ECS/` | Registry와 System으로 Boids 시뮬레이션 구현 |
| `Boids/OOP/` | SimulationObject 기반 Boid·Dummy 소유와 BoidObject 행동 구현 |
| `Boids/Archetype/` | Boid·Dummy 테이블의 column 직접 순회와 공통 계획에 따른 Dummy 교체 |
| `Benchmarker/` | 초기화·warmup·고정 dt 측정 반복, 환경 수집과 결과 직렬화 |
| `App/` | 실행 옵션, 구현 선택, 콜백 연결과 결과 출력 |

`App`은 Engine과 Boids를 조립한다. Boids 행동은 `Boids`에 두고,
Engine에 도메인 로직을 넣거나 App에서 ECS 조회·Boids 계산을 수행하지 않는다.
`Boids::Common`은 GLM을 직접 제공하는 헤더 전용 타깃이며 Engine에 의존하지 않는다.
OOP는 `Engine::ECS`에 의존하지 않는다.
세 시뮬레이션은 GLM만 사용하는 `Engine::RenderData`로 CPU Transform을 계산한다.
`Boids/Common/simulation_validation.h`는 공통 입력 생성과 세 시뮬레이션의
설정·dt 검증을 제공하며, 저장소별 개체 수 한도는 각 구현에서 검사한다.
`Boids/Common/alignment.h`는 이웃 평균 속도로부터 alignment steering을 구하는
수식을 공유한다. 저장소별 이웃 순회와 합산은 각 구현이 담당한다.
`Boids/Common/vector_math.h`는 방향 정규화와 크기·속도 제한을 제공한다.
일반 입력은 기존 float 연산 순서를 유지하며, 벡터 길이·반경 제곱이
overflow하거나 subnormal·0으로 underflow하면 해당 계산만 double로 수행한다.
정규화·크기 제한에 전달된 비유한 벡터와 저장할 수 없는 가속도·적분 위치는
`std::overflow_error`로 실행을 중단한다. 제한값이 0인 경우도 검사하며
위치 overflow는 경계 처리 전에 거부한다. RenderData의
방향 회전 계산도 같은 범위 초과를 처리하며 Boids에 의존하지 않는다.

`Benchmarker::Experiment`는 실험 실행 함수를 제공한다. App은 구현을 선택하고 이
함수를 호출한다. 이 함수가 Boids를 직접 실행하며, Engine에는 콜백이나 시뮬레이션
타입을 전달하지 않는다. `Engine::Measurement`는 시간·메모리 계측 API만 제공한다.
Boids와 ECS 저장소는 이 API로 구간을 기록하고 소유한 저장량을 보고한다.

## 데이터와 저장 방식

`Boids/Common/simulation_input.h`는 `ObjectKind`, `InitialObjectState`,
`DummyStepPlan`, `SimulationInput`과 헤더 전용 입력 생성함수
`PrepareSimulationInput(config, total_steps)`를 제공한다.
초기 상태는 종류·최초 생성 index인 논리 번호·위치·속도로 구성되며 Engine의
Entity·Transform이나 객체 포인터를 포함하지 않는다. step별 변경 계획은 교체할
Dummy의 논리 번호 목록이다. 호출자가 입력을 소유하고 시뮬레이션은 변경 없이 빌려
사용하는 계약이다.

생성 순서는 Boid부터 두 종류를 번갈아 배치한 뒤 남은 종류를 이어 붙인다.
각 개체의 위치와 방향을 x·y·z 순으로 추출하고 방향을 정규화해 초기 속도를 만든다.
초기 상태와 변경 계획은 같은 seed의 독립된 `std::mt19937`을 사용한다.
계획은 매 step 전체 Dummy 논리 번호에서 `floor(dummy_count × dummy_change_percent / 100)`개를
중복 없이 표본 추출하고 오름차순으로 저장한다. 같은 설정·seed·표준 라이브러리에서
재현되며, 전체 step 수를 늘려도 앞부분의 상태와 계획은 바뀌지 않는다.
표준 난수 분포와 표본 추출 알고리즘의 구현이 다를 수 있어 다른 표준 라이브러리 사이의
동일한 난수 결과까지 보장하지는 않는다.

Boid·Dummy 수와 step 수는 0을 허용한다. 변경 비율은 0~100이며, 변경 수가 0이면
`total_steps`개의 빈 목록을 반환한다. 경계는 유한한 양수, 생성 범위는 경계 안의
유한한 비음수이고 양 끝의 차이가 float으로 표현 가능해야 한다. 초기 속도와 행동
파라미터는 유한한 비음수여야 한다. 잘못된 설정은 `std::invalid_argument`, 개체 수나
계획 저장량의 산술 overflow 및 컨테이너 크기 한도 초과는 할당 전에
`std::overflow_error`로 거부한다. 표현 가능한 크기라도 실제 메모리가 부족하면
표준 컨테이너의 할당 실패 예외가 전달된다.

호출자는 초기화 계측 전에 warmup과 측정 step 수의 합으로 입력을 한 번 생성한다.
warmup 이후 상태나 계획을 다시 시작하지 않고 다음 step 목록부터 이어 쓴다.
각 구현은 목록의 Dummy를 모두 삭제한 뒤 같은 순서로 재생성하며 최초 논리 번호와
`initial_objects[logical_id]`의 위치·속도를 복원한다. Transform·회전·가속도와 행동
파라미터의 저장은 각 구현의 책임이다. 입력 수명은 마지막 갱신과 저장량 보고까지
유지하고 시뮬레이션 저장량과 별도로 보고해야 한다.
OOP, Sparse Set과 Archetype은 공통 입력을 받는 초기화·갱신 API를 제공한다.
Benchmarker·CLI의 공통 입력·Dummy 변경 연결은 후속 작업이며 기존 실행 경로는 유지한다.

- Sparse Set ECS: Registry가 개체 수명과 타입별 ComponentStorage를 소유한다.
  컴포넌트는 dense 배열에 저장하고, sparse index로 개체와 배열 위치를 연결한다.
  삭제 시 마지막 원소를 옮기고 index를 갱신한다.
- Archetype ECS: 별도 Registry가 같은 컴포넌트 조합의 Table을 소유한다.
  각 Table은 Entity 목록과 타입별 연속 column을 가지며, 같은 행은 같은 Entity를 뜻한다.
  삭제 시 마지막 행의 모든 column과 Entity를 옮기고 해당 Entity의 위치를 갱신한다.
  Boids 시뮬레이션은 Boid의 `Transform + Velocity + Acceleration + BoidParameters`와
  Dummy의 `Transform + Velocity`를 별도 테이블에 저장한다.
- OOP: BoidSimulation이 `std::vector<std::unique_ptr<SimulationObject>>`로
  BoidObject와 DummyObject를 함께 소유한다. 부모는 Transform, Velocity, 종류와
  논리 번호 및 가상 소멸자를 가지며, BoidObject만 Acceleration과 BoidParameters를
  추가한다. 객체 데이터는 개별 할당되고 포인터 배열만 연속 저장된다.
  종류·논리 번호와 내부 번호 대응이 어긋나지 않도록 부모 객체의 복사·대입은 금지한다.

Sparse Set의 `Initialize(const SimulationInput&)`는 입력을 복사하지 않고 빌린다.
초기 개체 수·종류·논리 번호와 유한한 위치·속도를 검증한 뒤 입력 순서대로 생성한다.
Boid는 Transform·Velocity·Acceleration·BoidParameters를, Dummy는 Transform·Velocity만
가진다. 무인자 `Initialize()`는 공통 입력을 소유하고 기존 시각화용 MeshRenderer를
Boid에 추가한다. 재초기화는 기존 Entity를 무효화하고 논리 번호 대응을 다시 만든다.
`EntityFor(logical_id)`는 현재 Entity를 반환하고 없는 번호·삭제된 개체는
`std::out_of_range`로 처리한다.

Sparse Set의 `Update(dt, dummy_ids, timing)`는 전달된 목록 순서로 Dummy를 모두
삭제한 뒤 같은 순서로 재생성한다. Registry의 실제 Destroy/Create와 논리 번호→Entity
배열 갱신을 수행하고 빌린 입력에서 최초 위치·속도를 복원한다. Simulation은 난수나
step 번호를 관리하지 않는다. 호출자는 입력을 변경하지 않고 마지막 Update·저장량
보고까지 유지하며, warmup 다음 계획을 측정에 이어 전달한다. 계획 없는 Update는
계산된 변경 수가 0일 때만 허용하고, dt는 유한한 비음수여야 한다. 실행 도중 예외가
발생하면 재초기화와 새 recorder가 필요하다.

ECS의 View는 필요한 컴포넌트 조합을 조회하며, BoidSystem은
`CachedView<BoidParameters, Transform, Velocity>`의 Entity·dense index를 재사용한다.
Dummy가 없는 BoidParameters 배열을 기준으로 캐시를 구성해 Dummy 삭제로
Transform·Velocity의 dense 순서가 바뀌어도 Boid 이웃 합산 순서를 유지한다.
값 변경은 그대로 반영되고, 관련 저장 구조가 바뀌면 조회 준비 구간에서 캐시를
갱신한다. 이동·속도 제한·경계·방향 처리도 BoidParameters가 있는 개체만 대상으로
하며 순회 중에는 구조를 변경하지 않는다.

OOP의 `Initialize(const SimulationInput&)`는 Common의 입력을 복사하지 않고 빌려
사용한다. 호출자는 마지막 Update·저장량 보고까지 입력을 살아 있고 변경되지 않은
상태로 유지한다. 초기 개체 수·종류·논리 번호와 유한한 위치·속도를 검증한 후 입력
순서대로 객체를 생성한다. `Objects()`는 소유권 이전 없이 부모 포인터 배열을 조회한다.
논리 번호는 최초 index이며 현재 배열 index와 별개다.
경계·생성 범위와 초기 속도·행동 파라미터 조건은 생성자에서 검사해 입력을 빌리는
경로에서도 Common과 같은 설정을 요구한다.

OOP의 계획을 받는 Update는 전달된 Dummy 번호 순서대로 모두 삭제한 후, 같은
순서로 새 DummyObject를 생성한다. 삭제는 마지막 포인터를 빈자리로 옮기는
swap-and-pop이며, 논리 번호→현재 index 배열을 갱신한다. 재생성은 빌린 입력의 최초
위치·속도를 사용한다. 이웃 후보는 기존 번호 대응 배열을 논리 번호 순으로 순회해
조회하며, 종류를 검사해 Dummy를 제외한다. 삭제로 물리적 배열 순서가 바뀌어도
Boid의 부동소수점 합산 순서는 유지한다. 상태 갱신은 혼합 부모 포인터 배열에서
Boid만 처리한다. 별도 Boid 대상 캐시나 정렬은 없으며, 번호 대응 조회 비용은
`neighbor_calculation`에 포함한다. 상태 비교는 논리 번호와 명시한 허용 오차를 사용한다.
할당 실패나 수치 overflow 등으로 Update 실행 도중 예외가 발생하면 재초기화가
필요하며, 실패한 계측 실행은 폐기하고 새 recorder를 사용한다.

ECS의 네 System은 모두 SystemManager가 `unique_ptr<System>`로 소유한다.
조회 준비·이웃 계산 매니저와 상태 갱신 매니저를 분리하며, 준비된 캐시가 비었으면
상태 갱신 매니저를 건너뛰고 해당 구간을 명시적 0으로 기록한다.
System의 공통 Update는 Registry, dt와 같은 TimingRecorder 참조를 받는다.
SystemManager는 등록 순서대로 실행하며, 두 인자 Update는 비활성 recorder를
만들어 같은 실행 경로를 사용한다. 개별 System의 두 인자 호출도 공통 System
오버로드를 사용한다.

## 실행 흐름

Headless 벤치마크는 다음 순서로 실행한다.

1. App이 옵션을 읽고 `Benchmarker::Experiment`의 실행 함수를 선택한다.
2. 실행 함수가 환경 정보를 수집하고, 시뮬레이션을 생성한 뒤 `Initialize()`를 계측한다.
   생성자와 계측 도구 준비는 초기화 시간에서 제외된다.
3. 계측을 비활성화한 채 고정 dt로 warmup을 실행한다.
4. step별 기록 공간을 미리 할당하고 `begin_step_measurement/end_step_measurement` 사이에서 Update를 호출한다.
   BoidSystem은 `start/stop`으로 조회 준비·이웃 계산을 계측한다.
   SystemManager는 상태 갱신 System들의 연속된 구간을 감싸서 계측한다.
5. 마지막 step 이후 프로세스 최대 메모리를 먼저 조회하고, 시뮬레이션 종료 전
   컨테이너 저장량과 시간 결과를 수집한다.
6. App이 CSV 원자료를 저장하고 설정·환경·시간·메모리 요약을 출력한다.

두 구현은 같은 seed와 설정으로 초기화하고 naive all-pairs 이웃 탐색을 사용한다.
한 step은 개체 변경 → 조회 준비 → 모든 Boid의 가속도 계산 → 속도·위치 갱신 →
경계 처리 → 방향 갱신 순서다.
ECS는 SystemManager에 등록된 Boid·Movement·Boundary·Orientation 순서로,
OOP는 객체 메서드의 반복 호출로 수행한다. BoidSystem은 조회 준비·이웃 계산을
내부에서 계측하며, 세 상태 갱신 System은 `AddTimed("state_update", ...)`로
등록한다. 매니저는 같은 이름으로 연속 등록된 System들을 하나의 start/stop 구간으로
감싸므로, 상태 갱신에는 세 System 실행과 그 사이의 디스패치가 포함된다.
계측 구간 이름은 Boids 등록 코드에서 제공하며 Engine에 하드코딩하지 않는다.
이웃 계산에는 모든 Boid의 갱신 전 위치·속도를 사용한다.

전체 step 계측은 Update 호출을 감싸며 초기화·warmup·출력은 제외한다.
구간 시간과 별도로 측정하고 구간 계측 비용을 포함한다. 원자료 복사·집계는 step
경계 밖에서 수행한다. 빈 변경 목록은 명시적 0으로 기록한다. 비어 있지 않은
변경 목록은 대상 조회·파괴·생성·번호 대응 갱신을 `entity_changes`로 계측한다.
Sparse Set은 Boid가 없으면 이웃 계산과 상태 갱신도 명시적 0으로 기록한다.
캐시 확인·갱신은 빈 결과라도 실제 작업이 있으므로 조회 준비에서 계측한다.
OOP는 별도 조회 준비가 없어 해당 구간을 명시적 0으로 기록한다.

저장량은 시뮬레이션 객체 자체와 소유한 배열·개별 할당 객체의 size/capacity 기반
추정치다. ECS의 alive/generation/free ID 배열, component sparse/dense 배열과
CachedView, 논리 번호→Entity 배열을 포함한다. OOP는 부모 포인터 배열, 실제 파생 타입 크기의 개별 객체와
논리 번호 대응 배열을 보고하며 객체 내부 필드를 다시 더하지 않는다.
무인자 Initialize는 Common 생성함수로 만든 입력을 별도 소유하고 그 객체와
배열 저장량도 보고한다. 외부에서 빌린 입력은 시뮬레이션 저장량에 포함하지 않는다.
SystemManager는 등록 항목 배열의 size/capacity와 등록 시 저장한 `sizeof(SystemType)`을
사용하여 실제 파생 타입의 개별 할당 객체를 보고한다. 각 System의 가상
AppendStorageReport는 별도 소유한 동적 저장소만 추가한다. 따라서 BoidSystem의
CachedView 배열도 매니저를 통해 보고되고, Simulation은 System 타입별 항목을 나열하지
않는다. 계측 구간 이름은 소유한 `vector<char>` 버퍼로 저장하며 이 버퍼의 size/capacity도
보고한다. 등록 항목의 제어 필드와 각 System의 객체 내부 필드는 별도로 중복 계산하지 않는다.
Sparse Set의 두 매니저는 `systems`와 `state_systems` 접두사로 저장량을 구분한다.
SystemManager의 `AppendStorageReport(report, prefix)`로 매니저별 이름을 지정하며
기본 접두사는 `systems`다. 개별 System이 추가하는 별도 저장소 이름은 그대로 유지한다.
unordered_map의 항목 payload는 계산하지만 구현별 node 링크·bucket 저장량은
미지원으로 표시하며 ECS 합계는 알려진 항목의 부분합이다. allocator overhead와
fragmentation은 추정에 포함하지 않는다. 프로세스 최대값에는 계측 버퍼와 warmup도
포함되며, 조회 시점까지의 프로세스 전체 수명에 대한 값이다.

## Archetype 저장소 API

`Engine::ArchetypeECS`는 `Engine/ECS/Archetype/registry.h`를 공개하며,
namespace는 `engine::ecs::archetype`다. 기존 Sparse Set의 Registry와 독립적이고
같은 `engine::ecs::Entity` 표현을 재사용한다. Entity는 자신을 만든 Registry에서만 사용한다.
의존 타깃은 `Engine::Measurement`이며 Boids·GLM·OpenGL 의존성은 없다.

```cpp
engine::ecs::archetype::Registry registry;
const auto entity = registry.Create(Position{1.0F}, Velocity{2.0F});
auto& table = registry.GetOrCreateTable<Position, Velocity>();
auto positions = table.Column<Position>();
auto velocities = table.Column<Velocity>();
for (std::size_t row = 0; row < table.Size(); ++row) {
  positions[row].x += velocities[row].x;
}
registry.Destroy(entity); // 이후에는 table/column을 다시 조회한다.
```

예시의 Position·Velocity는 호출자가 정의하는 단순 컴포넌트 타입이다.
저장 타입은 cv/ref가 없는 trivially copyable·copy constructible 타입이며,
move 대입은 noexcept여야 한다. bool 값은 구조체로 감싸서 사용한다.
컴포넌트가 별도 동적 자원을 소유하는 모델과 생성 후 컴포넌트 조합 변경은 지원하지 않는다.

`GetOrCreateTable<T...>()`는 Entity 생성 없이 조합별 테이블을 준비하고,
`FindTable<T...>() const`는 없으면 nullptr을 반환한다. 타입 나열 순서는 조합을
바꾸지 않으며 중복 타입은 `std::invalid_argument`다. `Entities()[row]`와
`Column<T>()[row]`는 같은 Entity에 대응한다. const Table은 const span을 반환하고,
없는 column 요청은 `std::logic_error`다. 기존 테이블·column 조회는 동적 할당을 하지 않는다.
생성·삭제·Clear 후에는 빌린 참조와 span을 다시 조회하며 순회 중 구조를 변경하지 않는다.

Registry는 Entity ID별 테이블·행·generation과 free ID 배열을 유지한다.
삭제·재사용과 Clear 후에도 이전 Entity가 다시 유효해지지 않도록 generation 이력을 보존한다.
generation이 한도에 도달한 슬롯은 재사용하지 않으며 ID 한도 초과는
`std::overflow_error`다. 생성 중 할당 실패 시 추가 중이던 행을 되돌린다.
Registry와 Table은 복사·이동하지 않는다.

`AppendStorageReport(report, prefix)`는 테이블 포인터 배열, 위치·generation 및
free ID 배열, 별도 할당된 Table·column 객체와 그 원소 배열을 보고한다.
모든 저장소를 소유한 vector와 명시적 객체 할당으로 구성하므로 숨겨진 map node나
bucket 추정 항목은 없다. Registry 객체 자체는 호출자가 단독 객체 또는 상위 Simulation의
sizeof로 한 번 계산하고, 이 메서드는 별도 할당만 더한다. prefix를 달리하면 여러 Registry를
한 보고서에 넣을 수 있다. allocator overhead·fragmentation은 기존 기준대로 제외한다.

## Archetype Boids 시뮬레이션

`Boids::Archetype`는 `Boids/Archetype/boid_simulation.h`의
`boids::archetype::BoidSimulation`을 제공한다. 의존 타깃은 `Boids::Common`,
`Engine::ArchetypeECS`, `Engine::RenderData`, `Engine::Measurement`다.
Velocity·Acceleration은 기존 `Boids/ECS/Components/`의 단순 데이터 타입을
재사용하며 Sparse Set Registry·SystemManager나 OpenGL에는 의존하지 않는다.

생성자는 설정을 검사한다. `Initialize(const SimulationInput&)`는 개체 수·종류·
논리 번호와 위치·속도의 유한성을 검사하고 기존 Registry와 번호 대응을 정리한다.
입력 순서대로 Entity를 생성하며 두 테이블은 개체 수가 0이어도 초기화 시 준비한다.
초기 회전은 속도 방향, scale은 0.7, 가속도는 0, 행동 파라미터는 config 값이다.
입력은 복사하지 않고 빌리므로 호출자는 마지막 Update와 저장량 보고까지 수명과
불변성을 보장한다. 시뮬레이션에 난수 생성기나 계획 카운터는 없다.

`Update(dt, dummy_ids, timing)`는 초기화된 상태와 유한한 비음수 dt를 요구한다.
호출자는 Common에서 만든 중복 없는 Dummy 목록을 warmup부터 측정 종료까지
연속 전달한다. `entity_changes`에서 번호로 Entity를 찾고 목록 순서대로 모두
삭제한 뒤 같은 순서로 재생성한다. 재생성은 입력의 최초 Transform·Velocity를
복원하고 논리 번호→Entity 배열을 갱신한다. 배열 index와 Entity ID·테이블 행은
독립적이다. `EntityFor(logical_id)`는 현재 Entity를 반환하고 없는 번호는
`std::out_of_range`로 거부한다. `GetRegistry()`는 const 조회만 제공한다.
할당 실패나 수치 overflow 등으로 Update 실행 도중 예외가 발생하면
다시 Initialize한 뒤 실행해야 한다. 실패한 계측 실행은 폐기하고 새 recorder를 사용한다.

구조 변경이 끝난 후 `query_preparation`에서 Boid 테이블과 네 column span을
다시 조회한다. `neighbor_calculation`은 이 column의 행끼리 naive all-pairs로
비교하며 Entity별 component 조회나 별도 대상 캐시는 없다. Boid 테이블의 행 순서는
Dummy 교체에 영향받지 않아 이웃 합산 순서도 유지된다. 모든 가속도를 먼저 계산한
다음 `state_update`에서 가속도 반영·속도 제한·위치 적분, 경계 처리, 최종 속도의
방향 갱신을 순서대로 수행한다. Dummy는 모든 Boids 계산과 상태 갱신에서 제외된다.
빈 변경 목록의 `entity_changes`와 Boid가 없는 경우의 이웃 계산·상태 갱신은
`record_zero`로 기록한다. 테이블·column 조회가 있으므로 조회 준비는 계측한다.

`GetStorageReport()`는 Simulation 객체, 논리 번호 대응 배열과 Registry의 모든
별도 할당을 각각 한 번 보고한다. Registry 객체와 vector 제어 필드는 Simulation의
sizeof에 포함되므로 다시 더하지 않는다. 빌린 입력·계획, 전체 step 계측과 프로세스
최대 메모리 조회는 호출자 책임이다. `BoidsArchetypeTests`는 이 타깃과 GoogleTest만
링크하여 창·그래픽 초기화 없이 초기화·warmup·측정·저장량 보고를 직접 검증한다.
Dummy 변경 비율 0·40·100%에서 warmup 이후 계측을 활성화한 실행과 전체를
무계측으로 연속 실행한 결과를 매 step 비교한다. 논리 번호별 위치·속도·가속도와
부호를 맞춘 회전 성분의 절대 허용 오차는 `1e-5`이며, Entity generation과 최종
저장량도 비교하여 측정 전환 시 상태·계획을 다시 시작하지 않음을 검증한다.
별도 double 수식으로 실제 갱신 전 상태에서 다음 상태를 예측하는 테스트도 제공한다.
이 테스트는 production 수학 함수나 OOP 구현을 기대값 계산에 재사용하지 않고,
3개 seed·3개 변경 비율·3개 Boid 수에서 각각 60 step을 `1e-5` 허용 오차로 검사한다.
OOP와 Archetype의 통합 테스트는 3개 seed·Dummy 변경 비율 0·40·100%에서
각각 warmup 100 + 측정 길이 1,000 step의 연속 계획을 적용한다. 매 step 논리 번호별
위치·속도·가속도·회전·크기를 같은 `1e-5` 허용 오차로 비교한다.

현재 App/Benchmarker 비교 실행은 Boid 전용 ECS/OOP이며 Dummy 변경과 Archetype
선택을 연결하지 않았다. 세 구현의 공통 입력·Dummy 변경 API는 Common의
`simulation_input.h` 계약을 사용하며 warmup과 측정의 계획 번호는 호출자가 이어서
전달한다. OOP와 Sparse Set의 계획 없는 Update는 실제 Dummy 변경 수가 양수이면
오류로 처리한다. 세 구현 전체 동등성 검증과 App/Benchmarker 연결은 후속 작업이다.

Sparse Set 회귀 테스트는 변경 비율 0·40·100%에서 warmup 100회와 측정 1,000회를
연속 실행한다. Dummy의 초기 위치·속도와 교체 비율이 달라도 변경 없는 기준 실행과
논리 번호별 Boid 위치·속도·가속도 및 방향이 허용 오차 `1e-5` 내에서 일치하는지
검증한다. Dummy만 있는 경우, 빈 입력, 변경 수 내림, 반복 교체의 Entity 세대·번호
대응, 캐시 dense index 갱신과 재초기화, 네 구간의 기록·명시적 0도 검사한다.

계측 API 계약과 연결 예시는
[벤치마크 API](benchmarker-api.md)를 참고한다.

Headless 경로는 그래픽 초기화를 수행하지 않는다. 기존 시각화는 별도로
Engine::Core의 Application 프레임 루프에서 프레임별 dt를 사용한다.
