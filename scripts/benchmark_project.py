"""Project-local settings, editor state and read-only run history."""

import json
import os
from pathlib import Path

import run_benchmarks as batch


ROOT = Path(__file__).resolve().parents[1]
PROJECT_FILE = "project.json"
FIELD_NAMES = ("executable", "repetitions", "warmup_steps", "measurement_steps", "dt",
               "seed", "order_seed", "timeout_seconds")


class BenchmarkProject:
    def __init__(self, directory, state):
        self.directory = Path(directory).resolve()
        self.path = self.directory / PROJECT_FILE
        self.results = self.directory / "Result"
        self.state = state

    @classmethod
    def create(cls, directory):
        directory = Path(directory).resolve()
        directory.mkdir(parents=True, exist_ok=True)
        batch.require(not (directory / PROJECT_FILE).exists(), "이미 프로젝트가 있습니다. 프로젝트 불러오기를 사용하세요.")
        batch.require(not (directory / "Result").exists() or (directory / "Result").is_dir(), "Result 경로가 폴더가 아닙니다.")
        state = dict(format="boids-benchmark-project", version=1,
                     settings=dict(executable=str(ROOT / "build/vs2022/App/Release/App.exe"),
                                   repetitions="10", warmup_steps="200", measurement_steps="1000",
                                   dt="0.016666667", seed="42", order_seed="2026", timeout_seconds=""),
                     conditions=[], generator={}, view={}, messages={})
        project = cls(directory, state)
        batch.require(project.results.resolve().parent == directory, "Result는 프로젝트 안의 폴더여야 합니다.")
        project.results.mkdir(exist_ok=True)
        project.save()
        return project

    @classmethod
    def open(cls, directory):
        directory = Path(directory).resolve()
        state = json.loads((directory / PROJECT_FILE).read_text(encoding="utf-8-sig"))
        batch.require(isinstance(state, dict) and state.get("format") == "boids-benchmark-project"
                      and type(state.get("version")) is int and state["version"] == 1,
                      "지원하는 Benchmarker 프로젝트가 아닙니다.")
        batch.require(isinstance(state.get("settings"), dict)
                      and all(isinstance(state["settings"].get(key), str) for key in FIELD_NAMES),
                      "프로젝트 설정 형식이 올바르지 않습니다.")
        batch.require(isinstance(state.get("conditions"), list), "조건 목록이 올바르지 않습니다.")
        # Invalid text fields are legitimate drafts; structural data still must be safe.
        for condition in state["conditions"]:
            batch.keys(condition, ("implementation", "boid_count", "dummy_count", "dummy_change_percent"))
            batch.require(condition["implementation"] in ("oop", "ecs", "archetype"), "잘못된 구현 이름")
            for key in ("boid_count", "dummy_count"):
                batch.integer(condition[key], key)
            batch.integer(condition["dummy_change_percent"], "변경률", 0, 100)
            batch.require(condition["boid_count"] + condition["dummy_count"] <= batch.UINT32_MAX, "개체 수 범위 초과")
        for key in ("generator", "view", "messages"):
            batch.require(isinstance(state.get(key, {}), dict), f"잘못된 프로젝트 상태: {key}")
            state.setdefault(key, {})
        project = cls(directory, state)
        batch.require(project.results.resolve().parent == directory, "Result는 프로젝트 안의 폴더여야 합니다.")
        project.results.mkdir(exist_ok=True)
        return project

    def save(self):
        raw = json.dumps(self.state, ensure_ascii=False, indent=2, allow_nan=False) + "\n"

        def write(path):
            with path.open("w", encoding="utf-8") as stream:
                stream.write(raw)
                stream.flush()
                os.fsync(stream.fileno())
        batch.atomic_file(self.path, write)

    def run_folder(self, name):
        batch.require(isinstance(name, str) and name not in ("", ".", "..") and Path(name).name == name,
                      "실행 기록 이름이 올바르지 않습니다.")
        folder = (self.results / name).resolve()
        batch.require(folder.parent == self.results.resolve(), "이 프로젝트의 실행 기록이 아닙니다.")
        return folder

    def run_config(self, name):
        folder = self.run_folder(name)
        manifest = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
        batch.require(isinstance(manifest, dict), "실행 기록 형식이 올바르지 않습니다.")
        batch.require(manifest.get("resume_version") == 1, "재개를 지원하는 실행 기록이 아닙니다.")
        config = batch.resolve_config(manifest["resolved_config"], folder)
        batch.require(Path(config["output_directory"]).resolve() == self.results.resolve(),
                      "실행 기록의 결과 경로가 현재 프로젝트와 다릅니다.")
        return config

    def history(self):
        records = []
        for folder in sorted(self.results.iterdir(), reverse=True):
            if not folder.is_dir() or not (folder / "manifest.json").exists():
                continue
            try:
                self.run_folder(folder.name)
                config = self.run_config(folder.name)
                attempts = []
                journal = folder / "attempts.jsonl"
                if journal.exists():
                    with journal.open("rb") as stream:
                        for line in stream:
                            if not line.endswith(b"\n"):
                                break
                            row = json.loads(line)
                            batch.require(isinstance(row, dict), "잘못된 실행 기록 형식")
                            attempts.append(row)
                total = len(config["conditions"]) * config["repetitions"]
                latest = {}
                for row in attempts:
                    batch.require(type(row.get("order")) is int and 1 <= row["order"] <= total,
                                  "잘못된 실행 순서")
                    batch.require(row.get("status") in ("success", "failed", "interrupted"), "잘못된 실행 상태")
                    latest[row["order"]] = row
                successes = sum(row["status"] == "success" for row in latest.values())
                failures = sum(row["status"] == "failed" for row in latest.values())
                finished = (folder / "summary.json").is_file()
                status = "완료" if finished and successes == total else "실패 포함" if finished else "중단됨"
                records.append(dict(name=folder.name, folder=folder, config=config, attempts=attempts,
                                    total=total, successes=successes, failures=failures,
                                    completed=successes + failures, status=status, error=""))
            except (OSError, ValueError, KeyError, TypeError) as error:
                records.append(dict(name=folder.name, folder=folder, config=None, attempts=[],
                                    total=0, successes=0, failures=0, completed=0,
                                    status="기록 확인 필요", error=str(error)))
        return records
