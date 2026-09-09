# OOP Boids

OOP 비교 구현을 추가할 위치입니다. 현재 구현과 CMake 타깃은 없습니다.

공통 설정과 행동 매개변수는 `Boids::Common`의 `boids::BoidSimulationConfig`와
`boids::BoidParameters`를 사용합니다. OOP 구현은 `Boids::ECS`와
`Engine::ECS`에 의존하지 않습니다.
