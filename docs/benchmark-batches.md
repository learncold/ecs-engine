# 조건별 반복 벤치마크

`scripts/run_benchmarks.py`는 단일 실험용 `App.exe --benchmark`를 조건별로
별도 프로세스에서 순차 실행한다. Windows, Python 3.10 이상과 VS2022 x64
기본 MSVC toolset의 Release 빌드가 필요하다. Python 외부 패키지는 사용하지 않는다.
연구 조건과 지표의 정의는 [연구 기준 5~6절](research-goals.md#experiments)을 따른다.

## 빌드·검증·실행

프로젝트 루트에서 다음 명령을 실행한다. 전체 테스트에는 세 구현의 상태 동등성 검사가
포함된다. Release 전체 테스트의 `BenchmarkBatchAcceptance`도 실제 App을 사용한다.

```powershell
cmake --preset vs2022
cmake --build --preset vs2022-debug
ctest --test-dir build/vs2022 -C Debug --output-on-failure
cmake --build --preset vs2022-release
ctest --test-dir build/vs2022 -C Release --output-on-failure
python -B scripts/run_benchmarks.py benchmarks/smoke.json
```

[smoke.json](../benchmarks/smoke.json)은 16 Boid·12 Dummy, 변경률 0·40%, 세 구현을
각각 2회 실행하는 작은 검증 예시다. warmup은 7, 측정은 20 step이다.
논문용 최종 규모·반복 수를 확정하는 설정이 아니며, 실제 실험용으로 복사해 조정한다.
조건 수와 반복 수에 코드상의 고정값은 없다.

## 설정 계약

JSON의 모든 필드는 `timeout_seconds`를 제외하면 필수다. 경로는 현재 작업 폴더가
아닌 **설정 파일 위치 기준**으로 해석한다. 알 수 없는 필드·중복 JSON 키도 거부한다.

| 필드 | 의미와 유효 범위 |
| --- | --- |
| `version` | 설정 형식 버전, 정수 `1` |
| `executable` | `<build>/App/Release/App.exe`; 인접 `CMakeCache.txt`의 VS2022 x64 기본 toolset 확인 |
| `output_directory` | 실행별 새 결과 폴더를 만들 부모 경로 |
| `repetitions` | 양의 정수; 전체 조건 목록을 반복할 횟수 |
| `seed` | 세 구현에 공통으로 적용할 uint32 정수 |
| `dt` | 양의 유한 float32로 표현 가능한 고정 timestep(초) |
| `warmup_steps` | 0 이상의 정수 |
| `measurement_steps` | 양의 정수 |
| `conditions` | 아래 네 필드를 가진 비어 있지 않은 조건 목록 |
| `conditions[].implementation` | `oop`, `ecs`, `archetype` |
| `conditions[].boid_count` | 0 이상의 정수 |
| `conditions[].dummy_count` | 0 이상의 정수 |
| `conditions[].dummy_change_percent` | 0~100 정수 |
| `timeout_seconds` | 선택 사항, 실행당 제한 시간(양의 정수 초); 생략 시 제한 없음 |

모든 조건을 실행 전에 검사한다. bool·소수인 개수, ECS ID 범위를 넘는 총 개체 수,
step 수나 공통 입력 저장량의 산술 범위 초과도 거부한다. 실제 메모리 부족과 수치
overflow 등 실행 중 실패는 개별 실패로 남긴다. 실행 순서는 **반복 번호 → 설정의
조건 순서**이며, 동일한 seed와 dt를 모든 조건·반복에 사용한다.

## 결과와 실패 처리

매 호출은 UTC 시각과 UUID로 새 폴더를 만들며 이전 폴더를 재사용하지 않는다.
파일 이름에는 전체 실행 순서·조건 번호·반복 번호(모두 1부터 시작)를 포함한다.

| 파일 | 보존 내용 |
| --- | --- |
| `config.json` | 원본 설정 파일의 바이트 그대로 |
| `manifest.json` | 절대 경로로 해석한 설정, 실행 파일·도구 SHA-256, Python 버전, 실행 순서 정책 |
| `CMakeCache.txt` | 실행 파일의 빌드 설정 사본 |
| `*.command.json` | 실제 argv 배열, Windows 명령 문자열과 자식 작업 폴더 |
| `*.csv` | App의 CSV v2 원자료; 실행 실패 시 없을 수 있음 |
| `*.log` | 해당 자식의 stdout·stderr와 실행 도구의 실패 사유 |
| `results.csv` | 조건, 반복 번호, 순서, 시작·종료 UTC, 종료 코드, 판정·사유와 파일 경로·명령 |
| `summary.json` | 성공·실패·전체 실행 수 |

도구는 자식 프로세스가 종료된 후에만 다음 실행을 시작한다. timeout 시 자식을
종료하고 회수한 다음 계속한다. 이름 있는 Windows mutex로 이 도구의 중복 실행을
거부한다. 수동 App 실행이나 외부 프로파일링은 함께 실행하지 않는다.
실행 파일 SHA-256을 매 실행 전후 확인해 도중 빌드가 바뀐 실행을 실패 처리한다.

종료 코드 0이어도 CSV가 없거나 잘못되면 실패다. 형식 버전, 실제 조건·float32 dt,
Release·MSVC·단일 스레드 환경, step별 전체 시간과 네 구간의 시간·호출 수,
초기화·요약·프로세스 최대 메모리·저장량 항목을 확인한다. 중복·누락·범위 밖 step,
비유한·음수 측정값, 저장량 size/capacity 대응과 알려진 합계 오류를 거부한다.
전체 시간 합계·평균·updates/s, 구간별 평균과 이웃 Pair당 비용도 step별 원자료로
재계산해 대조한다(상대 허용 오차 `1e-9`, 절대 허용 오차 `1e-12`, 각 지표 단위 기준).
평균 전체 시간이 0일 때만 updates/s를, Boid 수가 0 또는 1일 때만 이웃 Pair당 비용을
`not_measured`로 허용한다. 전체 시간은 네 구간 시간의 합계와 별도로 검증한다.
실행 간 CPU·OS·컴파일러·빌드 플래그도 비교한다.

메모리 API의 미지원·실패는 빈 값과 상태·사유가 있으면 보존할 수 있다.
Sparse Set의 알려진 저장량 부분합(`partial`)도 허용한다. 이들은 실제 숫자 0과
구분되며, 결과가 성공이어도 모든 메모리 지표가 수집되었다는 뜻은 아니다.
`results.csv`의 종료 코드는 CSV 검증 실패라면 `0`일 수 있으며, timeout이나 시작
실패처럼 정상 종료 코드를 얻지 못한 경우 빈 값이다. 판정에는 `status`와 `error`를 사용한다.

각 실행 후 결과 목록을 flush하고 실패 후에도 나머지를 계속한다. 마지막에 성공·실패
수를 출력하고 하나라도 실패하면 도구의 종료 코드는 `1`, 모두 성공하면 `0`이다.
실행 전 설정·환경 오류도 `1`이다. 도구 자체 중단이나 디스크 오류는 전체 실행을
중단할 수 있으며, 이 경우 완료된 `summary.json`이 없으므로 미완료 실행으로 취급한다.

## 자동 검증 범위

`BenchmarkBatchTests`는 잘못된 설정·Debug/다른 generator 거부, CSV 누락·손상·조건
불일치, 명시적 0·미측정 메모리, 비정상 종료·시작 실패·timeout 후 계속 실행과
중복 실행 방지를 검사한다. `BenchmarkBatchAcceptance`는 Release App으로 예시의
12개 실행을 두 번 수행한다. 총 24개의 독립 프로세스와 원자료·로그·실행 순서,
이전 결과 보존을 검사하고 `build/vs2022/Tests/batch-acceptance/`에 증거를 남긴다.
추가로 유효한 큰 dt에서 실제 수치 overflow를 일으킨 실행 뒤 Dummy 전용 실행을
이어 수행해, 자식·도구의 실패 종료 코드와 계속 실행을 확인한다(총 26개 실행).
