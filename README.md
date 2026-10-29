# ECS Engine

C++20 기반 ECS 경량 게임 엔진 프로토타입을 만드는 졸업 연구 프로젝트입니다.
고정 시간 간격으로 실행하는 단일 스레드 headless 3D Boids 시뮬레이션에서
OOP, Sparse Set ECS, Archetype ECS의 저장·조회 비용과 성능을 비교하는 것이 목표입니다.

연구는 동일한 naive all-pairs 이웃 탐색을 기준으로 Boid·Dummy 수와 Dummy
삭제·재생성 비율에 따른 실행 시간·메모리 비용을 비교합니다. 렌더링 확장과
렌더링 성능 측정은 연구 범위에서 제외하며, 기존 그래픽 코드와 의존성은 유지합니다.

## 빌드

필수 환경: CMake 3.20+, Visual Studio 2022 C++ 워크로드, vcpkg와 `VCPKG_ROOT` 환경 변수.

```powershell
cmake --preset vs2022
cmake --build --preset vs2022-debug
```

실행 파일: `build/vs2022/App/Debug/App.exe`

clangd 사용 시 파일 배치나 CMake 구성을 변경한 뒤 `cmake --preset clangd`로
`build/clangd/compile_commands.json`을 갱신합니다.

## 실행

성능 측정은 Release 빌드에서 `--benchmark`로 실행합니다.
`oop`, `ecs`(Sparse Set), `archetype`을 같은 실행 경로로 지원합니다.

```powershell
cmake --build --preset vs2022-release
.\build\vs2022\App\Release\App.exe --benchmark --implementation ecs --agents 1000 --dummies 1000 --dummy-change-percent 40 --seed 42 --dt 0.016666667 --warmup 100 --steps 1000 --output out/ecs.csv
.\build\vs2022\App\Release\App.exe --benchmark --implementation oop --agents 1000 --dummies 1000 --dummy-change-percent 40 --seed 42
.\build\vs2022\App\Release\App.exe --benchmark --implementation archetype --agents 1000 --dummies 1000 --dummy-change-percent 40 --seed 42
.\build\vs2022\App\Release\App.exe --help
```

`--help`에서 전체 옵션을 확인할 수 있습니다. 옵션 없이 실행하면 기존 3D 시각화가 열립니다.
원자료는 `--output`으로 지정한 CSV에 저장하며, 생략하면 `out/benchmarks/`에 실행별로
저장합니다. 벤치마크 실행은 한 번에 하나씩 수행합니다.
`--agents`는 Boid 수이며 Boid·Dummy 수와 warmup은 0을 허용합니다.
변경 비율은 0~100, 측정 step 수와 dt는 양수입니다. 공통 초기 상태·변경 계획은
초기화 계측 전에 준비하며, CSV에 실제 조건과 공통 입력의 별도 저장량을 기록합니다.

## 구조

- `Engine/`: ECS, 공통 런타임과 벤치마크 지원
- `Boids/`: 공통 설정과 Boids 시뮬레이션 구현
- `Benchmarker/`: 실험 실행, 환경 수집과 결과 저장 형식
- `App/`: 실행 옵션, 모듈 연결과 결과 출력
- `Tests/`: 검증 테스트

## 문서

- [연구·실험·논문 기준](docs/research-goals.md)
- [아키텍처와 모듈 경계](docs/architecture.md)
- [기술 스택과 의존성](docs/tech-stack.md)
- [벤치마크 API 사용법](docs/benchmarker-api.md)

구현 작업과 진행 상태는 GitHub 이슈에서 관리합니다.
