# Tech Stack

본 프로젝트는 C++ 기반의 ECS 경량 게임 엔진 프로토타입을 구현하며, 렌더링 백엔드는 OpenGL 3.3 Core Profile을 사용한다. 윈도우 및 입력 처리는 GLFW를 통해 관리하고, OpenGL 확장 함수 로딩은 GLAD를 이용한다. 빌드 환경은 CMake를 사용하여 플랫폼 독립적인 프로젝트 구성을 제공하며, 외부 패키지 관리는 vcpkg 매니페스트 모드를 사용한다.

본 프로젝트의 3D 범위는 범용 3D 게임 엔진 구현이 아니라, 대규모 Boids 군집 시뮬레이션을 3D 공간에서 수행하고 단순한 3D 에이전트로 시각화하는 데 한정한다.

## Engine Scope

이 프로젝트에서 엔진은 게임 제작 도구가 아니라 실시간 실행 환경을 의미한다. 따라서 기술 스택은 창 생성, 입력, 시간 관리, ECS 객체 관리, 렌더링, 시뮬레이션 갱신, 성능 측정을 구현하는 데 필요한 범위로 제한한다.

## Components

| Technology | Role |
| --- | --- |
| C++20 | 엔진 코어, ECS 구조, 런타임 로직 구현 |
| OpenGL 3.3 Core Profile | 단순 3D 렌더링 백엔드 |
| GLFW | 윈도우 생성, OpenGL Context 생성, 입력 이벤트 처리 |
| GLAD | OpenGL 함수 포인터 로딩 |
| Dear ImGui | 디버그·벤치마크 UI와 글자 렌더링 |
| CMake | 빌드 구성 및 타깃 관리 |
| vcpkg | GLFW 등 외부 라이브러리 설치 및 버전 관리 |

## Dependency Policy

GLFW와 Dear ImGui는 vcpkg 매니페스트(`vcpkg.json`)로 관리한다. Dear ImGui는 공식 GLFW 및 OpenGL 3 백엔드만 활성화한다. GLAD는 OpenGL API 버전과 프로파일에 따라 생성되는 코드이므로 `third_party/glad`에 프로젝트 의존성으로 포함한다.

## Initial Runtime Scope

초기 런타임은 다음 기능을 목표로 한다.

- 창 생성
- OpenGL Context 초기화
- GLAD 로딩
- 기본 게임 루프
- 프레임 단위 화면 clear
- 키보드와 마우스 입력 상태 수집
- 단순 3D 렌더링과 Orbit Camera 조작
- FPS, 프레임 시간, 에이전트 수와 VSync 상태를 보여 주는 최소 UI
- `--benchmark`로 창 없이 고정 스텝 시뮬레이션을 실행하고 업데이트 시간 측정

`Engine::Benchmark`는 `std::chrono::steady_clock`과 업데이트 콜백을 사용하는
표준 C++ 전용 타깃이다. 벤치마크 모드에서는 GLFW/OpenGL/ImGui를 초기화하지
않는다. 실행 파일은 시각화 모드와 공유하므로 기존 링크 의존성은 유지한다.

런타임 코드는 `Engine`, 연구 도메인은 `Boids`, 실행 조립은 `App` CMake 타깃으로
분리한다. ECS 렌더 어댑터는 별도 `Engine::ECSRenderer` 타깃이며 공통
`Engine::Renderer`는 ECS에 의존하지 않는다.

Transform과 MeshRenderer 등 공통 렌더 데이터는 `Engine/Renderer/`와
`engine::renderer` namespace에 둔다. GLM만 사용하는 `Engine::RenderData`
타깃을 별도로 링크하므로 Boids는 OpenGL 없이 Transform 함수를 사용할 수 있다.

`Boids/Common`은 Engine 의존성이 없는 헤더 전용 `Boids::Common` 타깃으로
공통 설정과 행동 매개변수를 제공한다. `Boids/ECS`의 `Boids::ECS` 타깃이 이를
사용하며, `Boids/OOP`는 향후 비교 구현을 위한 폴더다.

Engine·Boids·App은 기능별 폴더에 헤더와 소스를 함께 배치한다. 각 타깃의
헤더 검색 경로는 `${PROJECT_SOURCE_DIR}`이며, 기존 모듈 이름을 포함한
include 경로와 CMake 타깃 간 의존 관계를 유지한다.

## 3D Rendering Scope

초기 3D 렌더링은 연구용 시각화에 필요한 최소 기능만 구현한다.

- 기본 카메라
- 3D 위치 기반 에이전트 표시
- 저폴리 삼각형 또는 원뿔 형태의 에이전트 메시
- 대량 렌더링을 위한 Instanced Rendering
- `Transform + MeshRenderer` 기반 ECS 렌더 데이터 추출
- ECS와 향후 OOP 경로가 공유하는 `RenderInstance` 입력

다음 기능은 초기 연구 범위에 포함하지 않는다.

- 범용 3D 모델 로딩
- PBR
- 고급 조명 및 그림자
- 스켈레탈 애니메이션
- 씬 에디터
