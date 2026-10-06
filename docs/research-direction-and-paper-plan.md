# OOP·Sparse Set ECS·Archetype ECS 비교 연구 계획

작성 기준일: 2026-09-25
현재 상태 갱신일: 2026-09-30

## 1. 연구 주제

### 국문 제목

**3D Boids 시뮬레이션에서 OOP, Sparse Set ECS 및 Archetype ECS의 성능 비교**

### 영문 제목

**A Performance Comparison of OOP, Sparse Set ECS, and Archetype ECS in a
3D Boids Simulation**

### 연구 목적

동일한 단일 스레드 3D Boids를 세 가지 저장 구조로 구현하고 성능과 메모리를
측정하여, 각 구조의 데이터 배치와 접근 방식이 실제 실행 비용에 어떤 영향을
주는지 분석한다.

비교 대상은 다음 세 가지다.

1. 연속 객체 배열 기반 OOP
2. Sparse Set 기반 ECS
3. Archetype 기반 ECS

특정 구조가 항상 빠르다는 결론을 전제하지 않는다. OOP, Sparse Set과 Archetype이
각각 유리하거나 불리한 조건을 측정 결과로 설명하는 것이 목표다.

[Cox 등(2025)의 Sparse Set·Archetype 비교](https://doi.org/10.2312/cgvc.20251224)와
달리, 동일한 3D Boids에서 연속 배열 OOP를 기준선으로 두고 CachedView 적용
전후, phase별 시간과 메모리를 함께 분석한다.

## 2. 범위

### 연구 범위

- C++20 기반 경량 엔진 런타임
- 동일한 3D Boids 규칙과 초기 상태
- OOP, Sparse Set ECS, Archetype ECS 구현
- 단일 스레드 simulation update
- headless simulation benchmark
- 개체 수에 따른 실행 시간 변화
- 전체 및 주요 phase별 update 시간
- 초기화 시간과 메모리 사용량
- 반복 실행, 통계 집계와 결과 그래프

## 3. 비교 구현

| 구현 | 저장 방식 | 반복 접근 방식 |
|---|---|---|
| OOP | `std::vector<BoidObject>` | 객체 배열 직접 순회 |
| Sparse Set ECS | 타입별 sparse/dense 배열 | 최적화된 반복 순회 |
| Archetype ECS | 동일 조합별 component column | column과 행 범위 직접 순회 |

### OOP

OOP는 위치, 회전, 속도, 가속도와 행동 파라미터를 한 객체에 보관한다. 공정한
기준선을 위해 상속, 가상 함수와 개별 heap 할당을 넣지 않고 연속 객체 배열을
사용한다.

### Sparse Set ECS

컴포넌트 타입마다 sparse index, dense entity와 dense component 배열을 갖는다.
벤치마크에서는 최적화된 `CachedView`를 사용한다. 이웃 순회 중 sparse 조회는
없지만, 캐시된 dense index로 컴포넌트를 접근하는 비용은 남는다.

### Archetype ECS

동일한 component 조합을 가진 Boid를 하나의 archetype에 저장한다.

```text
Boid Archetype
├─ Entity[]
├─ Transform[]
├─ MeshRenderer[]
├─ Velocity[]
├─ Acceleration[]
└─ BoidParameters[]
```

System은 column과 행 범위를 한 번 얻고 직접 순회한다. 본 논문은 구조가 안정된
하나의 Boid archetype을 평가하며, 이를 모든 Archetype ECS의 일반적인 결과로
확장하지 않는다.

## 4. 연구 질문과 가설

### RQ1

동일한 3D Boids에서 OOP, Sparse Set ECS, Archetype ECS의 전체 update 시간은
개체 수에 따라 어떻게 달라지는가?

### RQ2

CachedView의 index 접근과 Archetype의 직접 column 순회 등 구현별 접근 경로는
관측된 phase별 시간 차이와 어떤 관계가 있는가?

### RQ3

세 구조는 초기화 시간과 메모리 사용량에서 어떤 차이를 보이는가?

### 가설

- CachedView의 index 접근을 포함한 Sparse Set 반복 경로는 Archetype의 직접
  column 순회보다 비용이 클 것이다.
- Archetype은 Sparse Set보다 빠르지만 동종 Boids에서는 OOP와 비슷할 것이다.
- 작은 개체 수에서는 고정 비용이, 큰 개체 수에서는 데이터 접근량이 더 크게
  작용할 것이다.

## 5. 실험 설계

### 주 실험

| 조건 | 저장 구조 | 이웃 탐색 |
|---|---|---|
| A1 | OOP | naive all-pairs |
| A2 | Sparse Set ECS | naive all-pairs |
| A3 | Archetype ECS | naive all-pairs |

세 구현에서 다음 조건을 동일하게 유지한다.

- 개체 수와 생성 순서
- 난수 seed와 초기 위치·속도
- fixed timestep
- Separation, Alignment, Cohesion 파라미터
- 속도 제한, 경계 처리와 방향 갱신
- update phase 순서
- compiler와 Release 최적화 설정
- warmup 및 measured step 수

### 개체 수

기본 측정 구간은 다음과 같다.

```text
100, 500, 1,000, 3,000
```

5,000 이상은 naive `O(N²)` 실행 시간이 과도하지 않을 때만 별도 확장 구간으로
측정한다.

### Phase별 분석

한 simulation step을 다음 네 구간으로 나눈다.

1. 이웃 순회 및 acceleration 계산
2. velocity 제한과 position 적분
3. boundary 처리
4. orientation 갱신

전체 update 시간은 별도로 측정한다. Phase별 결과는 어느 구간이 전체 차이를
만드는지 설명하는 데 사용한다. 계측 부담을 확인하기 위해 전체 시간과 phase별
시간은 별도 실행에서 측정하고, `O(N²)` 이웃 탐색은 pair당 비용도 비교한다.
일반 View와 CachedView의 차이는 보조 실험으로 분리한다.

### 메모리와 초기화

다음 항목을 측정한다.

- N개 Boid 초기화 시간
- 프로세스 private memory 또는 working set
- 컨테이너 size/capacity 기반 저장량
- 추정 bytes per entity

프로세스 메모리는 allocator와 런타임 영향을 포함하므로 컨테이너 기반 저장량과
구분해 보고한다. 컨테이너 저장량에는 entity index와 CachedView 등 보조 구조를
포함하고, 초기화 시간에는 각 구현의 `reserve` 정책을 명시한다.

## 6. 측정 지표와 통제

### 필수 지표

- 전체 simulation update `ms/step`
- phase별 `ms/step`
- updates per second
- 구현 간 시간 비율
- initialization time
- memory usage 및 bytes per entity
- P-core의 L1 load miss 비율과 pair당 miss 수; L3는 보조 지표

### 통계

- 최소 9회, 권장 12회 독립 프로세스 반복
- 조건별 median과 IQR
- mean과 standard deviation
- min/max
- 구현 간 차이의 신뢰구간; 변동이 크면 반복 횟수 확대

### 실행 통제

- Visual Studio 2022 Release 빌드
- 단일 스레드 및 headless 실행
- 각 조건을 새 프로세스에서 초기화
- 실행 순서를 반복마다 교차
- 같은 조건에서 seed, timestep과 step 수 고정
- commit, dirty state, CPU, OS, compiler와 build 설정 기록
- checksum 또는 허용 오차 기반 상태 동등성 검증
- 실패와 이상치를 임의로 삭제하지 않고 처리 기준 기록
- 동시에 여러 benchmark를 실행하지 않음

VTune에서 `L1_MISS / (L1_HIT + L1_MISS)`를 근사 비율로 계산하고, 앱 귀속 샘플 수와 계측 부담을
확인한다. 신뢰할 만한 비율을 얻지 못하면 캐시 우위를 성능 차이의 원인으로 주장하지
않는다. CPU cycles는 선택 지표로 둔다.

주 지표는 headless simulation의 update 시간이며, 렌더링은 데모의 정상 동작
확인에 사용한다.

## 7. 현재 상태

### 구현 완료 또는 진행 중

- OOP 3D Boids
- Sparse Set Registry, ComponentStorage와 View
- Sparse Set 반복 순회 최적화 및 구조 변경 감지
- OOP/Sparse 상태 동등성 테스트
- headless fixed timestep benchmark
- 별도 worktree의 Archetype 저장소 및 Archetype Boids
- OOP/Sparse/Archetype 예비 동등성 테스트

### Archetype 예비 결과

계획서 작성 당시 별도 worktree에서 12회 반복해 얻은 중앙값은 다음과 같다.

| Boids | OOP ms | Sparse ECS ms | Archetype ms |
|---:|---:|---:|---:|
| 100 | 0.021824 | 0.041320 | 0.018291 |
| 500 | 0.416651 | 0.828367 | 0.408272 |
| 1,000 | 1.479830 | 3.018313 | 1.492199 |
| 3,000 | 16.557296 | 30.751978 | 16.661528 |

Archetype의 중앙값은 Sparse ECS보다 약 1.85~2.26배 빨랐고, 500개 이상에서는
OOP와 약 -2.0%~+0.8% 차이였다. 동등성의 증거로 해석하지 않으며, 최종 통합
코드에서 원자료와 산포를 포함해 다시 측정한다.

### 후속 실험 상태

Sparse Set의 기본 비교 경로는 현재의 `CachedView`다. 강제 inline, 직접 이웃
배열 접근과 AoS/SoA 스냅샷은 별도 worktree에서 시험한 후보이며 기본 구현에
채택하지 않았다. Archetype의 지역 값 복사는 실제 App에서 개선이 재현되지
않아 되돌렸으며, 기존 고정 시그니처 컬럼 직접 순회를 유지한다.

후속 측정에서도 Archetype은 Sparse Set보다 빨랐고, 실제 App에서 OOP 대비
100개에서는 약 18% 빠르지만 500~10,000개에서는 대체로 비슷하거나 소폭 빨랐다.
10,000개를 포함한 상태 동등성 검증과 Debug/Release 테스트는 통과했다.
측정 조건이 달라 위의 초기 예비 결과와 절대 시간을 직접 비교하지 않는다.
후속 Archetype 실험은 조건별 6~8회 반복으로, 최종 실험의 9회 이상 반복 및
공통 phase·초기화·메모리·캐시 계측 조건을 모두 충족한 결과는 아니다.

## 8. 구현 및 실험 순서

### 1단계. Sparse Set 확정

- 반복 순회 경로 구현과 테스트 정리
- BoidSystem의 반복 접근 방식 확정
- 최적화 후보를 채택할 경우 최종 비교에 사용할 실제 Update 경로에서 효과 재검증
- 구조 변경 시 순회 데이터 갱신 규칙 문서화
- Debug/Release 전체 테스트

### 2단계. Archetype 통합

- 별도 worktree 구현을 현재 기준 코드에 맞게 통합
- Archetype benchmark CLI 연결
- 세 구현의 초기 상태 및 여러 step 후 위치·속도·가속도·방향 동등성 검증

### 3단계. 공통 계측

- 전체 및 phase별 timer
- initialization timer
- memory metadata
- 세 구현의 출력 형식 통일

### 4단계. 반복 runner

- 구현, 개체 수와 반복 조합을 독립 프로세스로 실행
- 실행별 CSV 저장
- 환경 metadata 저장
- 원자료 보존 후 통계 집계와 그래프 생성

CSV 최소 열은 다음과 같다.

```text
implementation, agent_count, seed, dt,
warmup_steps, measured_steps, repetition, measurement_mode,
total_update_ms, mean_update_ms, neighbor_ms, movement_ms,
boundary_ms, orientation_ms,
initialization_ms, process_memory_bytes, container_memory_bytes,
checksum, status, commit, dirty, cpu, os, compiler
```

### 5단계. 최종 측정

1. 코드와 실험 옵션 동결
2. Debug/Release 테스트
3. 세 구현의 correctness 확인
4. 반복 benchmark
5. CSV 검증
6. 통계 및 그래프 생성
7. 대표 조건 재실행
8. 논문 결과와 원본 CSV 대조

## 9. 논문 구성

### 1장. 서론

- 연구 배경과 문제 정의
- 선행연구와 3D Boids·OOP 기준선의 차별점
- 연구 질문과 기여

### 2장. 이론적 배경

- OOP 객체 배열과 AoS
- ECS와 Data-Oriented Design
- Sparse Set과 query 비용
- Archetype과 component column
- 3D Boids와 naive 이웃 탐색
- [Reynolds의 Boids 원전](https://www.red3d.com/cwr/papers/1987/boids.html)
  및 Cox 등의 Sparse Set·Archetype 비교 연구

### 3장. 설계 및 구현

- 경량 엔진과 benchmark 경로
- OOP Boids
- Sparse Set 저장 구조와 반복 접근 방식
- Archetype 저장소와 Boids
- 세 구현의 공통 update 순서와 동등성

### 4장. 실험 방법

- 실험 조건과 환경
- 개체 수, seed와 timestep
- 반복 실행과 통계
- 시간, 초기화와 메모리 측정

### 5장. 결과

- 전체 update 시간
- phase별 시간
- 저장 구조별 phase 비용
- 초기화 시간과 메모리
- 개체 수에 따른 scaling

### 6장. 논의

- OOP와 Archetype이 비슷한 이유
- 일반 View 대비 CachedView 효과와 캐시된 index 접근 비용
- 관측된 차이와 저장 배치·접근 경로의 관계 및 인과 해석의 한계
- 저장 구조별 적용 조건

### 7장. 한계

- 단일 CPU, Windows와 MSVC 환경
- 단일 스레드만 평가
- 동종 Boids 하나만 사용
- Archetype migration 및 높은 구조 변경 빈도 미평가
- headless simulation의 CPU 성능을 대상으로 함
- 결과가 모든 OOP/ECS 구현을 대표하지 않음

### 8장. 결론

- 연구 질문별 답변
- 세 구조의 성능과 메모리 특성
- workload에 따른 저장 구조 선택 기준

## 10. 완료 조건

### 구현

- [ ] Sparse Set 반복 순회 경로 확정
- [ ] Archetype 통합
- [ ] OOP/Sparse/Archetype benchmark 선택 가능
- [ ] 세 구현 수치 동등성 검증
- [ ] 전체/phase별 시간 측정
- [ ] 초기화 및 memory 측정
- [ ] 반복 runner와 CSV 출력

### 실험

- [ ] 조건별 독립 반복 9회 이상
- [ ] 실행 순서 교차
- [ ] median, IQR, mean, SD와 구현 간 차이의 신뢰구간 산출
- [ ] 실패 및 이상치 처리 기준 기록
- [ ] 표와 그래프 재생성 가능

### 논문

- [ ] RQ1~RQ3에 측정 결과로 답변
- [ ] OOP/Sparse/Archetype 구현 범위 명시
- [ ] 일반 View를 Sparse Set query 비용의 보조 결과로 분류
- [ ] Archetype 결과의 적용 범위를 본 구현과 workload로 명시
- [ ] cache miss 원인 분석은 실제 측정 결과에 근거
- [ ] 단일 스레드와 동종 workload의 한계 명시

## 11. 작업 순서

```text
Sparse Set 반복 순회 경로 확정
        ↓
Archetype 통합 및 세 구현 동등성 검증
        ↓
시간·초기화·메모리 계측
        ↓
반복 runner와 CSV
        ↓
최종 측정·통계·그래프
        ↓
논문 결과·논의·결론 작성
```
