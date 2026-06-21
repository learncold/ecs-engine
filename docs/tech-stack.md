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
| CMake | 빌드 구성 및 타깃 관리 |
| vcpkg | GLFW 등 외부 라이브러리 설치 및 버전 관리 |

## Dependency Policy

GLFW는 vcpkg 매니페스트(`vcpkg.json`)로 관리한다. GLAD는 OpenGL API 버전과 프로파일에 따라 생성되는 코드이므로 `third_party/glad`에 프로젝트 의존성으로 포함한다.

## Initial Runtime Scope

초기 런타임은 다음 기능을 목표로 한다.

- 창 생성
- OpenGL Context 초기화
- GLAD 로딩
- 기본 게임 루프
- 프레임 단위 화면 clear
- 추후 ECS, 단순 3D 렌더링, 입력 시스템 확장

## 3D Rendering Scope

초기 3D 렌더링은 연구용 시각화에 필요한 최소 기능만 구현한다.

- 기본 카메라
- 3D 위치 기반 에이전트 표시
- 저폴리 삼각형 또는 원뿔 형태의 에이전트 메시
- 대량 렌더링을 위한 Instanced Rendering

다음 기능은 초기 연구 범위에 포함하지 않는다.

- 범용 3D 모델 로딩
- PBR
- 고급 조명 및 그림자
- 스켈레탈 애니메이션
- 씬 에디터
