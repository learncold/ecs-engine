# ECS Engine

ECS 기반 경량 게임 엔진 프로토타입 졸업 프로젝트입니다. 현재 개발 환경은 C++20, OpenGL 3.3 Core Profile, GLFW, GLAD, CMake, vcpkg 기준으로 구성되어 있습니다.

본 프로젝트는 범용 3D 게임 엔진 전체를 구현하는 것이 아니라, ECS와 데이터 지향 설계의 성능 특성을 검증하기 위한 단순화된 3D Boids 시뮬레이션 환경을 갖춘 경량 게임 엔진 프로토타입을 구현하는 것을 목표로 합니다.

## Engine Definition

이 프로젝트에서의 게임 엔진은 Unity나 Unreal Engine 같은 범용 제작 도구가 아니라, 실시간 게임 실행 환경을 구성하는 최소 런타임 계층을 의미합니다.

포함 범위:

- Application 및 game loop
- Window 및 input 처리
- Time step 관리
- Scene 또는 World 관리
- ECS 기반 객체 관리
- Rendering
- Simulation update
- Debug 및 benchmark 측정

Boids 시뮬레이션은 별도의 단독 계산 프로그램이 아니라, 위 런타임 계층 위에서 실행되는 연구용 데모로 사용합니다.

## Tech Stack

- Language: C++20
- Graphics API: OpenGL 3.3 Core Profile
- Window/Input: GLFW
- OpenGL Loader: GLAD
- Build System: CMake
- Dependency Management: vcpkg + vendored GLAD

## Research Scope

- 3D 공간 기반 Boids 군집 시뮬레이션
- 단순 3D 에이전트 렌더링
- Instanced rendering을 통한 대량 에이전트 시각화
- OOP 방식과 ECS 방식의 성능 비교
- Grid 기반 공간 분할 최적화 비교

범위 밖 항목:

- 범용 3D 모델 로딩
- PBR 및 고급 조명 시스템
- 애니메이션 시스템
- 물리 엔진
- 씬 에디터

## Prerequisites

- CMake 3.20+
- Visual Studio 2022 또는 2026 C++ workload
- vcpkg
- `VCPKG_ROOT` environment variable

## Build

```powershell
cmake --preset vs2022
cmake --build --preset vs2022-debug
```

Visual Studio 2026을 사용한다면 다음 명령을 사용할 수 있습니다.

```powershell
cmake --preset vs2026
cmake --build --preset vs2026-debug
```

빌드 후 실행 파일은 `build/<preset>/App/Debug/App.exe` 아래에 생성됩니다.

VS Code의 clangd는 별도의 컴파일 정보인 `build/clangd/compile_commands.json`을
사용합니다. 파일을 이동하거나 CMake의 소스 목록·헤더 경로·의존성을 변경했다면
다음 명령으로 편집기 정보도 갱신합니다.

```powershell
cmake --preset clangd
```

갱신 후에도 이전 오류 표시가 남으면 명령 팔레트에서
`clangd: Restart language server`를 실행합니다.

## Run and Benchmark

옵션 없이 실행하면 기존 3D 시각화를 사용합니다.

```powershell
.\build\vs2022\App\Debug\App.exe
```

성능 측정에는 Release 빌드를 사용합니다. `--benchmark`는 창이나 OpenGL
컨텍스트를 생성하지 않고 ECS naive 시뮬레이션만 실행합니다.

```powershell
cmake --build --preset vs2022-release
.\build\vs2022\App\Release\App.exe --benchmark --agents 100 --seed 42 --warmup 100 --steps 1000 --dt 0.016666667
```

`--benchmark`만 지정해도 위 기본 조건으로 실행됩니다. `--agents`, `--seed`,
`--warmup`, `--steps`, `--dt`는 벤치마크 전용 옵션이며 `--help`로 사용법을
확인할 수 있습니다. 워밍업은 0도 허용하지만 에이전트 수, 측정 스텝 수와
timestep은 양수여야 합니다. Boid 행동 파라미터는 `BoidSimulationConfig`의
기본값을 사용하며 실행 조건과 함께 출력합니다.

`Benchmarker`는 고정 timestep으로 업데이트 콜백을 동기 호출합니다.
초기화·워밍업·렌더 데이터 추출·UI·결과 출력은 측정에서 제외하고,
각 업데이트 호출의 경과 시간을 합산해 총 시간과 평균 `ms/step`, `updates/s`를
콘솔에 출력합니다. 업데이트에는 기존 `BoidOrientationSystem`도 포함됩니다.
고정 timestep은 가상 시간의 진행량이며 실행 속도를 제한하지 않습니다.
측정값은 CPU 사용 시간이 아닌 실제 경과 시간으로, 스케줄링 지연과 콜백 호출
비용을 포함합니다. 반복 호출 시 Benchmarker가 시뮬레이션을 초기화하지는 않습니다.

OOP/Grid 구현, CSV 저장, 자동 반복 실험과 CPU·메모리·캐시 분석은 후속 범위입니다.
실행 파일은 시각화 모드와 공유하므로 기존 그래픽 라이브러리 빌드/배포 의존성은
유지되지만, `Engine::Benchmark` 타깃 자체는 표준 C++ 라이브러리만 사용합니다.

## Project Layout

```text
ECS-engine/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ vcpkg.json
├─ README.md
├─ docs/
│  ├─ tech-stack.md
│  ├─ research-goals.md
│  └─ architecture.md
├─ Engine/
│  ├─ Core/
│  ├─ Benchmark/
│  ├─ ECS/
│  ├─ ECSRenderer/
│  ├─ Renderer/
│  ├─ Input/
│  └─ DebugUI/
├─ Boids/
│  ├─ Common/
│  ├─ ECS/
│  └─ OOP/
├─ App/
│  ├─ main.cpp
│  └─ UI/
├─ Tests/
└─ third_party/
   └─ glad/
```

`Engine`은 공통 런타임·Renderer·ECS를 제공하고, `Boids`는 연구 도메인의
ECS 시뮬레이션만 포함합니다. `App`은 두 모듈을 조립해 실행하는 얇은 계층입니다.
렌더링은 ECS와 무관한 `RenderInstance` 배열을 입력으로 받으므로 향후 OOP 구현도
같은 Renderer를 공유할 수 있습니다.

`Boids/Common`은 공통 설정과 `BoidParameters`를 제공하고, `Boids/ECS`는 현재
시뮬레이션을 구현합니다. `Boids/OOP`는 향후 OOP 구현을 추가할 위치입니다.

Engine·Boids·App은 기능별 폴더에 `.h`와 `.cpp`를 함께 배치합니다.
예를 들어 `Engine/Core/application.h`와 `Engine/Core/application.cpp`가 한 폴더에
있습니다. CMake의 헤더 검색 경로는 프로젝트 루트이므로
`#include "Engine/Core/application.h"`처럼 모듈 이름을 포함해 사용합니다.
