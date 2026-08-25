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

빌드 후 실행 파일은 `build/<preset>/Sandbox/Debug/Sandbox.exe` 아래에 생성됩니다.

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
├─ Sandbox/
└─ third_party/
   └─ glad/
```
