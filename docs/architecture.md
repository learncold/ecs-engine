# 아키텍처

## 개요

이 프로젝트는 ECS 기반 3D Boids 시뮬레이션과 OOP 기준 구현의 성능을 비교하기
위한 경량 런타임이다. 구현은 책임에 따라 `Engine`, `Boids`, `App`으로 나뉜다.

- `Engine`: 재사용 가능한 실행, 입력, ECS, 렌더링 데이터와 렌더링 기능
- `Boids`: Boids 규칙과 ECS 시뮬레이션
- `App`: Engine과 Boids를 생성하고 콜백으로 연결하는 실행 계층

의존 방향은 `App → Engine`, `App → Boids`, `Boids → Engine`이다. `Engine`은
Boids나 App을 알지 못하며 Boids는 App에 의존하지 않는다.

Engine, Boids, App은 기능별 폴더에 헤더와 구현 파일을 함께 둔다. 예를 들어
`Engine/Core/application.h`와 `application.cpp`, `App/UI/performance_panel.h`와
`performance_panel.cpp`가 각각 같은 폴더에 있다. 별도 `include/`, `src/` 트리는
사용하지 않는다. Tests와 third_party는 기존 구조를 유지한다.

CMake 타깃은 프로젝트 루트를 헤더 검색 경로로 사용하므로
`#include "Engine/Core/application.h"` 같은 모듈 경로를 유지한다.
헤더 검색 경로가 공유되어도 위의 모듈 의존 방향과 타깃 간 링크 관계는 유지한다.

## 실행 흐름

1. App이 창과 OpenGL Context를 초기화한다.
2. Boids ECS 시뮬레이션과 Engine 렌더러를 초기화한다.
3. Engine Core가 입력과 프레임 시간을 수집한다.
4. Boids가 `BoidSystem → MovementSystem → BoundarySystem →
   BoidOrientationSystem` 순서로 시뮬레이션을 갱신한다.
5. `Engine::ECSRenderer`가 `Transform + MeshRenderer` View에서
   `RenderInstance` 배열을 추출한다.
6. `AgentRenderManager`가 Transform을 Model 행렬로 변환하고 Grid, 경계선,
   Agent를 렌더링한다.
7. App의 PerformancePanel이 Boid 수, FPS, 프레임 시간과 VSync를 표시한다.

## Engine 모듈

- `Core`: 애플리케이션 수명과 메인 루프
- `Input`: GLFW 입력을 엔진 입력 상태로 변환
- `DebugUI`: Dear ImGui 백엔드 수명과 프레임 처리
- `ECS`: Entity, Component 저장소, View, System 실행
- `Renderer`: 공통 `Transform`, `MeshRenderer`, Model 행렬 변환, 카메라,
  Orbit 조작, Grid/경계선, Instanced Agent 렌더링
- `ECSRenderer`: ECS View를 공통 Renderer 입력으로 바꾸는 어댑터
- `Benchmark`: 이후 추가할 공통 성능 측정 기능

`Transform`은 position, rotation quaternion, 균일 scale을 가진다. Model 행렬은
`Translation × Rotation × Scale` 순서로 만든다. 렌더링 포함 여부는 별도 visible
플래그가 아니라 `MeshRenderer` 컴포넌트의 존재 여부로 표현한다.

이 데이터와 함수는 `Engine/Renderer/`와 `engine::renderer` namespace에 둔다.
별도 Scene 폴더나 namespace는 없다. 빌드에서는 GLM만 사용하는
`Engine::RenderData` 타깃에 `transform.cpp`를 두고, `Engine::Renderer`와
`Boids::ECS`가 이를 사용한다. 따라서 Boids는 Transform 계산을 위해
OpenGL 렌더러에 의존할 필요가 없다.

## ECS 렌더 경계

공통 Renderer는 Registry나 Boid 타입을 알지 못하고 다음 데이터만 받는다.

```cpp
struct RenderInstance {
  engine::renderer::Transform transform;
  engine::renderer::MeshKind mesh;
};
```

ECS 경로에서는 `EcsRenderSystem`이 Registry로부터 이 배열을 만든다. 향후 OOP
경로에서는 객체 배열로부터 같은 `RenderInstance` 배열을 만들어
`AgentRenderManager`를 직접 호출한다. 따라서 OOP 구현은 `Engine::ECS`에 의존할
필요가 없다.

## Boids 모듈

`Boids/`는 다음 세 폴더로 나뉜다.

- `Common/`: `boids::BoidParameters`와 `boids::BoidSimulationConfig` 공통 데이터.
  헤더 전용 `Boids::Common` 타깃이며 Engine과 ECS 구현에 의존하지 않는다.
- `ECS/`: `boids::ecs` namespace의 Component, System, 시뮬레이션.
  `Boids::ECS` 타깃은 `Boids::Common`을 사용한다.
- `OOP/`: 향후 OOP 비교 구현을 추가할 위치. 현재 구현과 빌드 타깃은 없다.

공통 헤더는 `Boids/Common/`에, ECS 헤더와 소스는 `Boids/ECS/` 및 그 아래
`Components/`, `Systems/`에 둔다. 공통 헤더는 `Boids/Common/...`으로 포함한다.

`BoidParameters`는 개체 전체가 아닌 행동 매개변수 묶음이며, ECS에서는
이 공통 타입을 그대로 컴포넌트로 저장한다. 설정의 `parameters` 멤버가 초기값을
제공한다. `max_alignment_force`는 `alignment_weight`를 적용하기 전 정렬 조향력의
크기만 제한하며, 분리·응집이나 최종 합산 가속도의 상한은 아니다.

Entity 생성 시 Engine의 Transform과 MeshRenderer를
추가하고, 최종 Velocity가 바뀐 후 OrientationSystem이 local `+Z` 방향을 속도
방향에 맞춘다.

App에는 System 구현, Registry View 순회, Model 행렬 생성, OpenGL 호출을 두지
않는다. Material 시스템, 범용 Mesh 로딩, Scene Graph와 OOP Boids 구현은 현재
범위에 포함하지 않는다.
