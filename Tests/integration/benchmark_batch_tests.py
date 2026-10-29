"""Batch validation regressions and optional real Release CLI acceptance check."""

import copy
import csv
import importlib.util
import io
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from contextlib import redirect_stdout

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/run_benchmarks.py"
spec = importlib.util.spec_from_file_location("batch", SCRIPT)
batch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(batch)


def fixture_rows(config, condition):
    rows = []

    def add(kind, name, value, step="", status="available", reason=""):
        rows.append(dict(kind=kind, name=name, step=str(step), value=str(value), status=status, reason=reason))

    add("metadata", "format_version", 2)
    settings = dict(condition, entity_count=condition["boid_count"] + condition["dummy_count"],
                    dummy_change_count=condition["dummy_count"] * condition["dummy_change_percent"] // 100,
                    seed=config["seed"],
                    warmup_steps=config["warmup_steps"], measurement_steps=config["measurement_steps"],
                    fixed_delta_seconds=batch.float32(config["dt"]))
    for name in ("simulation_scope", "reserve_policy", "initialization_scope", "step_scope",
                 "process_memory_scope", "storage_scope", "input_storage_scope"):
        settings[name] = "documented scope"
    for name in ("spawn_half_extent", "boundary_half_extent", "initial_speed", "neighbor_radius",
                 "separation_radius", "separation_weight", "alignment_weight", "cohesion_weight",
                 "preferred_speed", "max_speed", "max_alignment_force"):
        settings[name] = 1
    for name, value in settings.items():
        add("setting", name, value)
    environment = dict(cpu="test CPU", os="Windows", os_build="26200", os_display_version="25H2",
                       compiler="MSVC 19.44", build="Release", build_flags="/O2 /DNDEBUG",
                       pointer_bits=64, cxx_standard=20, simulation_threads=1, clock="steady_clock",
                       process_id=123, run_start_unix_ms=123456, run_id="123456-123")
    for name, value in environment.items():
        add("environment", name, value)
    step_ms = [0.1 * (step + 1) for step in range(config["measurement_steps"])]
    phase_ms = dict(entity_changes=0, query_preparation=0, neighbor_calculation=0.02, state_update=0.03)
    for step, elapsed in enumerate(step_ms):
        add("step", "overall_ms", elapsed, step)
        for phase in batch.PHASES:
            add("section_ms", phase, phase_ms[phase] * (step + 1), step)
            add("section_calls", phase, int(phase_ms[phase] > 0), step)
    phase_means = {}
    for phase in batch.PHASES:
        phase_means[phase] = sum(phase_ms[phase] * (step + 1) for step in range(len(step_ms))) / len(step_ms)
        add("section_mean_ms_per_step", phase, phase_means[phase])
    mean_ms = sum(step_ms) / len(step_ms)
    add("summary", "total_step_ms", sum(step_ms))
    add("summary", "mean_step_ms", mean_ms)
    add("summary", "updates_per_second", 1000 / mean_ms)
    boids = condition["boid_count"]
    if boids > 1:
        add("summary", "neighbor_ms_per_pair", phase_means["neighbor_calculation"] / (boids * (boids - 1)))
    else:
        add("summary", "neighbor_ms_per_pair", "", status="not_measured", reason="Needs N > 1")
    for name in ("initialization_ms", "storage_size_bytes_per_entity", "storage_capacity_bytes_per_entity"):
        add("summary", name, 1)
    for name in ("peak_private_commit", "peak_working_set"):
        for kind in ("process_memory_bytes", "process_bytes_per_entity"):
            add(kind, name, "", status="failed", reason="OS query failed")
    for kind in ("storage_size_bytes", "storage_capacity_bytes", "input_storage_size_bytes", "input_storage_capacity_bytes"):
        names = (("object", "initial_objects", "dummy_steps", "dummy_step_ids") if kind.startswith("input_")
                 else ("simulation.object", "objects.pointers", "objects.boids", "objects.dummies"))
        for name in names:
            add(kind, name, 8)
        add("summary", kind, 32)
    return rows


def write_csv(path, rows):
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, ("kind", "name", "step", "value", "status", "reason"))
        writer.writeheader()
        writer.writerows(rows)


class BatchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="benchmark batch spaces ")
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.config = json.loads((ROOT / "benchmarks/smoke.json").read_text())
        self.config["measurement_steps"] = 3
        self.config["conditions"] = self.config["conditions"][:3]
        self.config["executable"] = str(self.folder / "App.exe")
        self.config["output_directory"] = str(self.folder / "results")
        self.config_path = self.folder / "settings.json"
        self.csv_path = self.folder / "result.csv"

    def load(self):
        self.config_path.write_text(json.dumps(self.config), encoding="utf-8")
        return batch.load_config(self.config_path)

    def test_invalid_configuration_rejected_before_launch_or_output(self):
        original = copy.deepcopy(self.config)
        cases = [("repetitions", 0), ("repetitions", True), ("seed", 2**32), ("seed", -1),
                 ("dt", 0), ("dt", float("nan")), ("dt", 1e-50), ("dt", 1e100), ("dt", True),
                 ("warmup_steps", -1), ("measurement_steps", 0), ("measurement_steps", 2**64),
                 ("conditions", []), ("typo", 1), ("timeout_seconds", 0), ("output_directory", "")]
        for name, value in cases:
            with self.subTest(name=name, value=value):
                self.config = copy.deepcopy(original)
                self.config[name] = value
                with self.assertRaises(ValueError):
                    self.load()
        for name, value in (("dummy_change_percent", 101), ("dummy_count", -1), ("boid_count", 2**32),
                            ("implementation", "sparse"), ("boid_count", 1.5), ("dummy_count", False)):
            with self.subTest(name=name):
                self.config = copy.deepcopy(original)
                self.config["conditions"][-1][name] = value
                with self.assertRaises(ValueError):
                    self.load()
        self.assertFalse(Path(original["output_directory"]).exists())

    def test_relative_paths_and_duplicate_json_keys(self):
        self.config["executable"] = "relative/App.exe"
        loaded, raw = self.load()
        self.assertEqual(Path(loaded["executable"]), self.folder / "relative/App.exe")
        self.assertEqual(json.loads(raw), self.config)
        self.config_path.write_text('{"version": 1, "version": 2}')
        with self.assertRaisesRegex(ValueError, "Duplicate JSON key"):
            batch.load_config(self.config_path)

    def test_preflight_rejects_debug_and_other_generators(self):
        app = self.folder / "build/App/Release/App.exe"
        app.parent.mkdir(parents=True)
        app.write_bytes(b"test executable")
        cache = self.folder / "build/CMakeCache.txt"
        for generator, toolset in (("Ninja", ""), ("Visual Studio 18 2026", ""),
                                    ("Visual Studio 17 2022", "ClangCL")):
            cache.write_text(f"CMAKE_GENERATOR:INTERNAL={generator}\nCMAKE_GENERATOR_PLATFORM:INTERNAL=x64\n"
                             f"CMAKE_GENERATOR_TOOLSET:INTERNAL={toolset}\n")
            with self.assertRaises(ValueError):
                batch.verify_executable(app)
        cache.write_text("CMAKE_GENERATOR:INTERNAL=Visual Studio 17 2022\nCMAKE_GENERATOR_PLATFORM:INTERNAL=x64\n")
        self.assertEqual(batch.verify_executable(app), cache)
        debug = self.folder / "build/App/Debug/App.exe"
        debug.parent.mkdir()
        debug.write_bytes(b"debug")
        with self.assertRaises(ValueError):
            batch.verify_executable(debug)

    def test_csv_accepts_explicit_zero_and_missing_memory_with_reason(self):
        write_csv(self.csv_path, fixture_rows(self.config, self.config["conditions"][0]))
        batch.validate_csv(self.csv_path, self.config, self.config["conditions"][0])

    def test_csv_rejects_timing_summaries_that_disagree_with_raw_steps(self):
        condition = self.config["conditions"][0]
        original = fixture_rows(self.config, condition)
        summaries = [("summary", name) for name in (
            "total_step_ms", "mean_step_ms", "updates_per_second", "neighbor_ms_per_pair")]
        summaries += [("section_mean_ms_per_step", phase) for phase in batch.PHASES]
        for kind, name in summaries:
            with self.subTest(kind=kind, name=name):
                rows = copy.deepcopy(original)
                next(row for row in rows if row["kind"] == kind and row["name"] == name)["value"] = "999999"
                write_csv(self.csv_path, rows)
                with self.assertRaisesRegex(ValueError, "CSV timing summary mismatch"):
                    batch.validate_csv(self.csv_path, self.config, condition)
        for kind, name in (("step", "overall_ms"), ("section_ms", "neighbor_calculation")):
            with self.subTest(raw_kind=kind, raw_name=name):
                rows = copy.deepcopy(original)
                next(row for row in rows if row["kind"] == kind and row["name"] == name)["value"] = "1"
                write_csv(self.csv_path, rows)
                with self.assertRaisesRegex(ValueError, "CSV timing summary mismatch"):
                    batch.validate_csv(self.csv_path, self.config, condition)

    def test_csv_accepts_timing_summary_rounding(self):
        condition = self.config["conditions"][0]
        rows = fixture_rows(self.config, condition)
        for row in rows:
            if row["kind"] == "section_mean_ms_per_step" or (row["kind"] == "summary" and row["name"] in (
                    "total_step_ms", "mean_step_ms", "updates_per_second", "neighbor_ms_per_pair")):
                row["value"] = str(math.nextafter(float(row["value"]), math.inf))
        write_csv(self.csv_path, rows)
        batch.validate_csv(self.csv_path, self.config, condition)

    def test_csv_requires_unavailable_timing_only_when_not_derivable(self):
        condition = self.config["conditions"][0]
        for name in ("updates_per_second", "neighbor_ms_per_pair"):
            with self.subTest(unexpected_missing=name):
                rows = fixture_rows(self.config, condition)
                next(row for row in rows if row["kind"] == "summary" and row["name"] == name).update(
                    value="", status="not_measured", reason="Missing timing")
                write_csv(self.csv_path, rows)
                with self.assertRaises(ValueError):
                    batch.validate_csv(self.csv_path, self.config, condition)
        rows = fixture_rows(self.config, condition)
        for row in rows:
            if row["kind"] in ("step", "section_ms", "section_calls", "section_mean_ms_per_step") or (
                    row["kind"] == "summary" and row["name"] in (
                        "total_step_ms", "mean_step_ms", "neighbor_ms_per_pair")):
                row["value"] = "0"
            if row["kind"] == "summary" and row["name"] == "updates_per_second":
                row.update(value="", status="not_measured", reason="Mean step time is zero")
        write_csv(self.csv_path, rows)
        batch.validate_csv(self.csv_path, self.config, condition)
        next(row for row in rows if row["kind"] == "summary" and row["name"] == "updates_per_second").update(
            value="0", status="available", reason="")
        write_csv(self.csv_path, rows)
        with self.assertRaises(ValueError):
            batch.validate_csv(self.csv_path, self.config, condition)
        for boids in (0, 1):
            with self.subTest(boid_count=boids):
                small = dict(condition, boid_count=boids)
                rows = fixture_rows(self.config, small)
                write_csv(self.csv_path, rows)
                batch.validate_csv(self.csv_path, self.config, small)
                next(row for row in rows if row["kind"] == "summary" and row["name"] == "neighbor_ms_per_pair").update(
                    value="0", status="available", reason="")
                write_csv(self.csv_path, rows)
                with self.assertRaises(ValueError):
                    batch.validate_csv(self.csv_path, self.config, small)

    def test_csv_rejects_corrupt_incomplete_or_mismatched_raw_results(self):
        condition = self.config["conditions"][0]
        original = fixture_rows(self.config, condition)
        changes = [("setting", "implementation", "value", "ecs"),
                   ("setting", "seed", "value", "43"),
                   ("setting", "dummy_count", "value", "13"),
                   ("setting", "measurement_steps", "value", "4"),
                   ("setting", "fixed_delta_seconds", "value", "0.1"),
                   ("environment", "build", "value", "Debug"),
                   ("environment", "compiler", "value", "Clang 20"),
                   ("step", "overall_ms", "value", "nan"),
                   ("section_ms", "entity_changes", "value", "-1"),
                   ("section_calls", "state_update", "value", "0.5"),
                   ("section_ms", "query_preparation", "step", "99"),
                   ("process_memory_bytes", "peak_working_set", "reason", "")]
        for kind, name, field, value in changes:
            with self.subTest(kind=kind, name=name, field=field):
                rows = copy.deepcopy(original)
                next(row for row in rows if row["kind"] == kind and row["name"] == name)[field] = value
                write_csv(self.csv_path, rows)
                with self.assertRaises(ValueError):
                    batch.validate_csv(self.csv_path, self.config, condition)
        for kind in ("step", "section_ms", "section_calls", "storage_size_bytes", "input_storage_size_bytes"):
            rows = copy.deepcopy(original)
            del rows[next(i for i, row in enumerate(rows) if row["kind"] == kind)]
            write_csv(self.csv_path, rows)
            with self.assertRaises(ValueError, msg=kind):
                batch.validate_csv(self.csv_path, self.config, condition)
        write_csv(self.csv_path, original + [original[0]])
        with self.assertRaisesRegex(ValueError, "Duplicate CSV"):
            batch.validate_csv(self.csv_path, self.config, condition)
        with self.assertRaises(FileNotFoundError):
            batch.validate_csv(self.folder / "absent.csv", self.config, condition)

    def test_failures_continue_and_evidence_is_not_overwritten(self):
        config, raw = self.load()
        Path(config["executable"]).write_bytes(b"fixed executable")
        cache = self.folder / "CMakeCache.txt"
        cache.write_text("test cache")
        calls = []

        def process(command, **kwargs):
            index = len(calls)
            calls.append(command)
            kwargs["stdout"].write(b"child output\n")
            path = Path(command[-1])
            if index == 0:
                return subprocess.CompletedProcess(command, 7)
            if index == 1:
                return subprocess.CompletedProcess(command, 0)  # No CSV.
            if index == 2:
                raise subprocess.TimeoutExpired(command, 1)
            if index == 3:
                raise OSError("cannot launch")
            condition = config["conditions"][index % 3]
            rows = fixture_rows(config, condition)
            if index == 4:
                next(row for row in rows if row["kind"] == "summary" and row["name"] == "mean_step_ms")["value"] = "999999"
            write_csv(path, rows)
            return subprocess.CompletedProcess(command, 0)

        with patch.object(batch.subprocess, "run", side_effect=process), redirect_stdout(io.StringIO()):
            self.assertEqual(batch.run_suite(config, raw, self.config_path, cache), 1)
        folder = next(Path(config["output_directory"]).iterdir())
        with (folder / "results.csv").open(newline="", encoding="utf-8") as stream:
            results = list(csv.DictReader(stream))
        self.assertEqual(len(calls), 6)
        self.assertEqual([row["status"] for row in results], ["failed"] * 5 + ["success"])
        self.assertEqual([row["exit_code"] for row in results], ["7", "0", "", "", "0", "0"])
        self.assertIn("CSV timing summary mismatch", results[4]["error"])
        self.assertEqual(json.loads((folder / "summary.json").read_text())["failures"], 5)
        self.assertEqual((folder / "config.json").read_bytes(), raw)
        saved = {path: path.read_bytes() for path in folder.iterdir()}
        for result in results:
            self.assertTrue(Path(result["log_path"]).is_file())
            command = json.loads(Path(result["command_path"]).read_text())
            self.assertEqual(command["argv"][0], config["executable"])
        with patch.object(batch.subprocess, "run", side_effect=process), redirect_stdout(io.StringIO()):
            batch.run_suite(config, raw, self.config_path, cache)
        self.assertEqual(len(list(Path(config["output_directory"]).iterdir())), 2)
        self.assertTrue(all(path.read_bytes() == value for path, value in saved.items()))

    @unittest.skipUnless(sys.platform == "win32", "Windows named mutex")
    def test_second_runner_cannot_acquire_lock(self):
        # A child process, since a mutex can be acquired recursively by its owning thread.
        with batch.batch_lock():
            command = [sys.executable, "-c",
                       f"import sys; sys.path.insert(0, {str(SCRIPT.parent)!r}); "
                       "import run_benchmarks as b; b.batch_lock().__enter__()"]
            child = subprocess.run(command, capture_output=True, timeout=10)
        self.assertNotEqual(child.returncode, 0)
        self.assertIn(b"Another benchmark batch", child.stderr)
        with batch.batch_lock():
            pass

    def test_changed_executable_prevents_further_children(self):
        config, raw = self.load()
        executable = Path(config["executable"])
        executable.write_bytes(b"original")
        cache = self.folder / "CMakeCache.txt"
        cache.write_text("test cache")

        def process(command, **kwargs):
            write_csv(Path(command[-1]), fixture_rows(config, config["conditions"][0]))
            executable.write_bytes(b"rebuilt during experiment")
            return subprocess.CompletedProcess(command, 0)

        with patch.object(batch.subprocess, "run", side_effect=process) as launch, redirect_stdout(io.StringIO()):
            self.assertEqual(batch.run_suite(config, raw, self.config_path, cache), 1)
        self.assertEqual(launch.call_count, 1)
        folder = next(Path(config["output_directory"]).iterdir())
        with (folder / "results.csv").open(newline="", encoding="utf-8") as stream:
            results = list(csv.DictReader(stream))
        self.assertTrue(all(row["status"] == "failed" and "Executable changed" in row["error"] for row in results))


def integration(app, output):
    """Real matrix, two invocations, paths with spaces; keep acceptance artifacts."""
    output.mkdir(parents=True, exist_ok=True)
    config = json.loads((ROOT / "benchmarks/smoke.json").read_text())
    config["executable"] = str(app.resolve())
    config["output_directory"] = str((output / "results with spaces").resolve())
    config_path = output / "acceptance.json"
    config_path.write_text(json.dumps(config, indent=2), encoding="utf-8")
    saved = {}
    for invocation in range(2):
        previous_folders = set(Path(config["output_directory"]).glob("*"))
        subprocess.run([sys.executable, str(SCRIPT), str(config_path)], check=True, timeout=120)
        new_folders = set(Path(config["output_directory"]).iterdir()) - previous_folders
        assert len(new_folders) == 1
        if saved:
            assert all(path.read_bytes() == value for path, value in saved.items()), "Earlier results overwritten"
        folder = new_folders.pop()
        summary = json.loads((folder / "summary.json").read_text())
        assert summary == {"successes": 12, "failures": 0, "total": 12}, summary
        with (folder / "results.csv").open(newline="", encoding="utf-8") as stream:
            results = list(csv.DictReader(stream))
        assert len(results) == 12
        run_ids = set()
        previous_end = ""
        for index, result in enumerate(results):
            assert int(result["order"]) == index + 1
            assert int(result["condition"]) == index % 6 + 1
            assert int(result["repetition"]) == index // 6 + 1
            assert result["status"] == "success" and result["exit_code"] == "0"
            assert result["started_utc"] >= previous_end
            previous_end = result["finished_utc"]
            assert Path(result["log_path"]).stat().st_size > 0
            condition = config["conditions"][int(result["condition"]) - 1]
            environment = batch.validate_csv(Path(result["csv_path"]), config, condition)
            assert int(environment["process_id"]) > 0
            run_ids.add(environment["run_id"])
        assert len(run_ids) == 12, "Every experiment must have a separate run identity"
        assert len({row["csv_path"] for row in results}) == 12
        saved.update({path: path.read_bytes() for path in folder.iterdir()})
    # A valid float32 dt can still overflow simulation integration. Check the
    # actual CLI's failure code and continuation into a Dummy-only success.
    config.update(repetitions=1, dt=3.4028234e38, warmup_steps=0, measurement_steps=1,
                  conditions=[dict(implementation="oop", boid_count=8, dummy_count=0, dummy_change_percent=0),
                              dict(implementation="archetype", boid_count=0, dummy_count=5, dummy_change_percent=40)])
    failure_config = output / "failure-continuation.json"
    failure_config.write_text(json.dumps(config, indent=2), encoding="utf-8")
    previous_folders = set(Path(config["output_directory"]).iterdir())
    child = subprocess.run([sys.executable, str(SCRIPT), str(failure_config)], timeout=30)
    assert child.returncode == 1, "A failed experiment must fail the batch CLI"
    assert all(path.read_bytes() == value for path, value in saved.items())
    new_folders = set(Path(config["output_directory"]).iterdir()) - previous_folders
    assert len(new_folders) == 1
    folder = new_folders.pop()
    assert json.loads((folder / "summary.json").read_text()) == {"successes": 1, "failures": 1, "total": 2}
    with (folder / "results.csv").open(newline="", encoding="utf-8") as stream:
        failed, succeeded = list(csv.DictReader(stream))
    assert failed["status"] == "failed" and failed["exit_code"] == "1"
    assert not Path(failed["csv_path"]).exists()
    assert "Runner failure" in Path(failed["log_path"]).read_text(encoding="utf-8")
    assert succeeded["status"] == "success" and Path(succeeded["csv_path"]).is_file()
    print("Batch acceptance: 24 matrix runs preserved; real failure followed by success verified")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--integration":
        integration(Path(sys.argv[2]), Path(sys.argv[3]))
    else:
        unittest.main()
