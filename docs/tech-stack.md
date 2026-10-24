# 기술 스택

C++20과 CMake로 경량 ECS 런타임과 headless 3D Boids 시뮬레이션을 구현한다.
이 문서는 사용 기술과 빌드·의존성 기준을 설명한다.

## 사용 기술

| 기술 | 역할 |
| --- | --- |
| C++20 | 런타임, ECS와 Boids 시뮬레이션 구현 |
| CMake 3.20+ | 빌드 타깃·프리셋과 테스트 구성 |
| Visual Studio 2022 / MSVC | 기본 x64 빌드·검증 환경 |
| vcpkg | 매니페스트 기반 외부 패키지 설치 |
| GLM | 3D 벡터·회전과 CPU Transform 계산 |
| GoogleTest / CTest | 단위·통합 테스트와 실행 검증 |
| OpenGL 3.3 Core Profile | 기존 시각화의 렌더링 API |
| GLFW | 기존 시각화의 창·컨텍스트·입력 |
| GLAD | OpenGL 함수 로딩 |
| Dear ImGui | 기존 시각화 UI |

## 빌드와 의존성

- 기본 빌드는 `vs2022` 프리셋을 사용하고, 성능 측정은 Release 구성에서 수행한다.
  Visual Studio 2026용 프리셋도 제공한다.
- vcpkg 경로는 `VCPKG_ROOT` 환경 변수로 지정한다. `vcpkg.json`에서
  GLFW, GLM, GoogleTest와 Dear ImGui를 관리한다.
- Dear ImGui는 GLFW·OpenGL 3 백엔드를 활성화한다.
- GLAD는 `third_party/glad/`에 포함된 소스를 사용한다. 다른 OpenGL 로더를
  추가하지 않으며, 재생성 시 API 버전과 프로파일을 기록한다.
- clangd는 별도의 `clangd` 프리셋(Ninja·clang-cl)으로 생성한
  `build/clangd/compile_commands.json`을 사용한다.
- 새 의존성은 연구에 필요하거나 구현 위험을 실질적으로 줄이는 경우에만 추가한다.

빌드·실행 명령은 [README](../README.md)를 따른다.

## 시뮬레이션과 그래픽 의존성

Headless 실행은 창과 OpenGL 컨텍스트를 초기화하지 않고 CPU 시뮬레이션만 수행한다.
현재 `App` 빌드는 기존 그래픽 모듈도 포함하므로 그래픽 의존성이 필요하다.
렌더링 확장과 시각화·렌더링 성능 측정은 연구 완료 범위에서 제외한다.

CPU Transform 계산은 GLM에만 의존하는 `Engine::RenderData` 타깃으로 분리되어 있다.
Boids는 이 타깃을 사용하며 OpenGL 렌더링 모듈에 의존하지 않는다.
헤더 전용 `Boids::Common`도 입력 타입의 `glm::vec3`와 초기 방향 정규화를 위해
`glm::glm`을 직접 제공한다. 공통 입력 생성은 표준 C++ 난수·표본 추출을 사용하며
Engine 의존성이나 추가 vcpkg 패키지는 없다. `BoidsCommonTests`는
`Boids::Common`과 GoogleTest만 링크해 이 경계를 검증한다.
`Boids::ECS`는 공통 입력의 Boid·Dummy 생성과 Dummy 삭제·재생성을 지원한다.
`BoidsEcsTests`는 이 타깃과 GoogleTest만 링크해 그래픽 초기화 없이 실행한다.
입력을 받는 초기화에는 렌더 컴포넌트가 없으며, 기존 무인자 초기화는 시각화용
MeshRenderer 데이터를 Boid에 유지한다. 추가 패키지나 그래픽 의존성은 없다.
`Engine::ArchetypeECS`는 표준 C++20의 vector·span과 RTTI로 타입별 column을
관리하며 `Engine::Measurement`에만 의존한다. 별도 패키지나 그래픽 의존성은 없고,
`EngineArchetypeEcsTests`는 이 타깃과 GoogleTest만 링크한다.
`Boids::Archetype`는 이 저장소와 `Boids::Common`, `Engine::RenderData`,
`Engine::Measurement`를 사용한다. 기존 Velocity·Acceleration 헤더를 재사용하지만
Sparse Set ECS 실행 코드나 그래픽 라이브러리는 링크하지 않는다.
`BoidsArchetypeTests`도 이 타깃과 GoogleTest만 링크해 headless 실행을 검증한다.
공통 계측은 `Engine::Measurement`에서 `std::chrono::steady_clock`과 이름별
`start/stop` API로 수행한다. Windows 프로세스 최대 메모리는 PSAPI의
`GetProcessMemoryInfo`로 수집한다. Windows 헤더는 구현 파일에만 포함하고
`psapi` 시스템 라이브러리를 링크한다.

`Benchmarker::Experiment`는 콜백 없이 Boids의 실행 API를 직접 호출하는 별도
모듈이다. CPU·OS 정보는 Windows Registry API로 읽으며 `advapi32` 시스템
라이브러리를 링크한다. 추가 vcpkg 패키지는 없다. 현재 실행 진입점은 App의
`--benchmark`이며 step별 시간과 설정·환경·메모리 원자료를 CSV로 저장한다.

모듈 경계는 [아키텍처](architecture.md), 연구 범위와 측정 기준은
[연구·실험·논문 기준](research-goals.md)을 따른다.
