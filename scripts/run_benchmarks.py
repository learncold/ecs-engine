"""Run independent, sequential VS2022 Release experiments (Python stdlib only)."""

import argparse
import csv
import ctypes
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys
import uuid
from contextlib import contextmanager


PHASES = ("entity_changes", "query_preparation", "neighbor_calculation", "state_update")
UINT32_MAX = 2**32 - 1
SIZE_MAX = 2**64 - 1


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, name, minimum=0, maximum=SIZE_MAX):
    require(type(value) is int and minimum <= value <= maximum,
            f"{name} must be an integer in [{minimum}, {maximum}]")
    return value


def float32(value):
    return struct.unpack("f", struct.pack("f", value))[0]


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def keys(value, required, optional=()):
    require(type(value) is dict, "Expected a JSON object")
    require(set(required) <= value.keys(), f"Missing fields: {set(required) - value.keys()}")
    require(value.keys() <= set(required) | set(optional),
            f"Unknown fields: {value.keys() - set(required) - set(optional)}")


def load_config(path):
    raw = path.read_bytes()
    config = json.loads(raw.decode("utf-8-sig"), object_pairs_hook=unique_object)
    keys(config, ("version", "executable", "output_directory", "repetitions", "seed",
                  "dt", "warmup_steps", "measurement_steps", "conditions"), ("timeout_seconds",))
    require(type(config["version"]) is int and config["version"] == 1, "Expected version 1")
    for name in ("executable", "output_directory"):
        require(type(config[name]) is str and config[name].strip() and "\0" not in config[name],
                f"{name} must be a nonempty path")
        config[name] = str((path.parent / config[name]).resolve())
    integer(config["repetitions"], "repetitions", 1)
    integer(config["seed"], "seed", maximum=UINT32_MAX)
    warmup = integer(config["warmup_steps"], "warmup_steps")
    steps = integer(config["measurement_steps"], "measurement_steps", 1)
    require(warmup + steps <= SIZE_MAX, "Total step count overflows size_t")
    dt = config["dt"]
    try:
        require(type(dt) in (int, float) and math.isfinite(dt) and dt > 0,
                "dt must be finite and positive")
        require(math.isfinite(float32(dt)) and float32(dt) > 0,
                "dt must be representable as a positive float32")
    except (OverflowError, struct.error) as error:
        raise ValueError("dt is outside float32 range") from error
    if "timeout_seconds" in config:
        integer(config["timeout_seconds"], "timeout_seconds", 1, 2**31 - 1)
    conditions = config["conditions"]
    require(type(conditions) is list and conditions, "conditions must be a nonempty list")
    for index, condition in enumerate(conditions, 1):
        keys(condition, ("implementation", "boid_count", "dummy_count", "dummy_change_percent"))
        require(condition["implementation"] in ("oop", "ecs", "archetype"),
                f"Condition {index}: implementation must be oop, ecs or archetype")
        boids = integer(condition["boid_count"], "boid_count")
        dummies = integer(condition["dummy_count"], "dummy_count")
        percent = integer(condition["dummy_change_percent"], "dummy_change_percent", 0, 100)
        # ECS IDs are uint32; reject an unusable condition before starting the suite.
        require(boids + dummies <= UINT32_MAX, "Entity count exceeds ECS ID range")
        changes = dummies * percent // 100
        # InitialObjectState is 40 bytes and vector<size_t> is 24 on MSVC x64
        # with this project's default GLM packing.
        input_bytes = (boids + dummies) * 40 + (warmup + steps) * (24 + changes * 8)
        require(input_bytes + (dummies * 8 if changes else 0) <= 2**63 - 1,
                "Common input exceeds MSVC x64 container/address limits")
    return config, raw


def verify_executable(path):
    require(path.is_file(), f"Executable does not exist: {path}")
    require(path.name.lower() == "app.exe" and path.parent.name == "Release"
            and path.parent.parent.name == "App", "Expected <VS2022 build>/App/Release/App.exe")
    cache = path.parents[2] / "CMakeCache.txt"
    values = {}
    for line in cache.read_text(encoding="utf-8").splitlines():
        if "=" in line and not line.startswith(("#", "//")):
            key, value = line.split("=", 1)
            values[key.split(":", 1)[0]] = value
    require(values.get("CMAKE_GENERATOR") == "Visual Studio 17 2022"
            and values.get("CMAKE_GENERATOR_PLATFORM") == "x64"
            and not values.get("CMAKE_GENERATOR_TOOLSET", ""),
            "Expected the default VS2022 x64 MSVC toolset")
    return cache


def digest(path):
    with path.open("rb") as stream:
        checksum = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(chunk)
        return checksum.hexdigest()


@contextmanager
def batch_lock():
    # A named OS mutex also serializes different output roots and executables.
    require(os.name == "nt", "Benchmark batches require Windows and VS2022 Release")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateMutexW.argtypes = (ctypes.c_void_p, ctypes.c_int, ctypes.c_wchar_p)
    kernel.CreateMutexW.restype = ctypes.c_void_p
    kernel.WaitForSingleObject.argtypes = (ctypes.c_void_p, ctypes.c_uint32)
    kernel.WaitForSingleObject.restype = ctypes.c_uint32
    kernel.ReleaseMutex.argtypes = (ctypes.c_void_p,)
    kernel.CloseHandle.argtypes = (ctypes.c_void_p,)
    handle = kernel.CreateMutexW(None, False, "Local\\ECS-engine-benchmark-batch")
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    acquired = False
    try:
        acquired = kernel.WaitForSingleObject(handle, 0) in (0, 0x80)
        require(acquired, "Another benchmark batch is running; wait for it to finish")
        yield
    finally:
        if acquired:
            kernel.ReleaseMutex(handle)
        kernel.CloseHandle(handle)


def validate_csv(path, config, condition):
    """Validate CSV v2, including every raw step, phase, status and memory row."""
    rows = {}
    with path.open(newline="", encoding="utf-8-sig") as stream:
        reader = csv.DictReader(stream, strict=True)
        require(reader.fieldnames == ["kind", "name", "step", "value", "status", "reason"],
                "Invalid CSV header")
        for row in reader:
            require(None not in row and None not in row.values(), "Malformed CSV row")
            key = (row["kind"], row["name"], row["step"])
            require(key not in rows, f"Duplicate CSV row: {key}")
            require(row["status"] in ("available", "partial", "not_measured", "unsupported", "failed"),
                    f"Invalid status: {key}")
            if row["status"] in ("not_measured", "unsupported", "failed"):
                require(row["value"] == "" and row["reason"], f"Missing value needs a reason: {key}")
            else:
                require(row["value"] != "", f"Missing available value: {key}")
                require(row["status"] != "partial" or row["reason"], f"Partial value needs a reason: {key}")
            rows[key] = row

    def get(kind, name, step="", statuses=("available",)):
        key = (kind, name, str(step))
        require(key in rows, f"Missing CSV row: {key}")
        row = rows[key]
        require(row["status"] in statuses, f"Unexpected status: {key}: {row['status']}")
        return row["value"]

    def number(kind, name, step="", statuses=("available",)):
        value = get(kind, name, step, statuses)
        if value == "":
            return None
        result = float(value)
        require(math.isfinite(result) and result >= 0, f"Invalid numeric value: {kind}/{name}/{step}")
        return result

    require(get("metadata", "format_version") == "2", "Expected CSV format version 2")
    expected = dict(condition, entity_count=condition["boid_count"] + condition["dummy_count"],
                    dummy_change_count=condition["dummy_count"] * condition["dummy_change_percent"] // 100,
                    seed=config["seed"], warmup_steps=config["warmup_steps"],
                    measurement_steps=config["measurement_steps"])
    for name, value in expected.items():
        require(get("setting", name) == str(value), f"CSV setting mismatch: {name}")
    require(float(get("setting", "fixed_delta_seconds")) == float32(config["dt"]), "CSV dt mismatch")
    for name in ("simulation_scope", "reserve_policy", "initialization_scope", "step_scope",
                 "process_memory_scope", "storage_scope", "input_storage_scope"):
        get("setting", name)
    for name in ("spawn_half_extent", "boundary_half_extent", "initial_speed", "neighbor_radius",
                 "separation_radius", "separation_weight", "alignment_weight", "cohesion_weight",
                 "preferred_speed", "max_speed", "max_alignment_force"):
        number("setting", name)
    environment = {name: get("environment", name) for name in (
        "cpu", "os", "os_build", "os_display_version", "compiler", "build", "build_flags",
        "pointer_bits", "cxx_standard", "simulation_threads", "clock", "process_id",
        "run_start_unix_ms", "run_id")}
    require(environment["build"] == "Release" and environment["compiler"].startswith("MSVC ")
            and environment["pointer_bits"] == "64" and environment["cxx_standard"] == "20"
            and environment["simulation_threads"] == "1" and environment["os"] == "Windows",
            "CSV is not a single-threaded Windows x64 MSVC Release experiment")

    steps = config["measurement_steps"]
    required_samples = {("step", "overall_ms")} | {
        (kind, phase) for kind in ("section_ms", "section_calls") for phase in PHASES}
    for kind, name in required_samples:
        sample_steps = {step for k, n, step in rows if k == kind and n == name}
        require(sample_steps == {str(step) for step in range(steps)}, f"Wrong sample steps: {kind}/{name}")
        for step in range(steps):
            value = number(kind, name, step)
            if kind == "section_calls":
                require(value.is_integer(), f"Non-integer call count: {name}/{step}")

    def timing_summary(kind, name, expected):
        statuses = ("not_measured",) if expected is None else ("available",)
        actual = number(kind, name, statuses=statuses)
        # CSV preserves double precision; allow summation/rounding differences
        # between the C++ producer and Python without accepting inconsistent data.
        require(expected is None or math.isclose(actual, expected, rel_tol=1e-9, abs_tol=1e-12),
                f"CSV timing summary mismatch: {kind}/{name}")

    total_ms = sum(number("step", "overall_ms", step) for step in range(steps))
    mean_ms = total_ms / steps
    number("summary", "initialization_ms")
    timing_summary("summary", "total_step_ms", total_ms)
    timing_summary("summary", "mean_step_ms", mean_ms)
    timing_summary("summary", "updates_per_second", 1000 / mean_ms if mean_ms > 0 else None)
    phase_means = {}
    for phase in PHASES:
        phase_means[phase] = sum(number("section_ms", phase, step) for step in range(steps)) / steps
        timing_summary("section_mean_ms_per_step", phase, phase_means[phase])
    boids = condition["boid_count"]
    timing_summary("summary", "neighbor_ms_per_pair",
                   phase_means["neighbor_calculation"] / (boids * (boids - 1)) if boids > 1 else None)
    for name in ("peak_private_commit", "peak_working_set"):
        for kind in ("process_memory_bytes", "process_bytes_per_entity"):
            number(kind, name, statuses=("available", "not_measured", "unsupported", "failed"))
    for kind in ("storage_size_bytes", "storage_capacity_bytes",
                 "input_storage_size_bytes", "input_storage_capacity_bytes"):
        entries = [name for k, name, step in rows if k == kind and step == ""]
        require(entries, f"Missing itemized storage: {kind}")
        for name in entries:
            number(kind, name, statuses=("available", "not_measured", "unsupported", "failed"))
        number("summary", kind, statuses=("available", "partial"))
    for name in ("storage_size_bytes_per_entity", "storage_capacity_bytes_per_entity"):
        number("summary", name, statuses=("available", "partial", "not_measured"))
    for kind in ("input_storage_size_bytes", "input_storage_capacity_bytes"):
        for name in ("object", "initial_objects", "dummy_steps", "dummy_step_ids"):
            number(kind, name)
    # Pair itemized size/capacity and verify known subtotals. This also catches
    # a truncated item list even if the file still contains its summary rows.
    for prefix in ("storage", "input_storage"):
        size_kind, capacity_kind = prefix + "_size_bytes", prefix + "_capacity_bytes"
        sizes = {name: row for (kind, name, step), row in rows.items() if kind == size_kind and not step}
        capacities = {name: row for (kind, name, step), row in rows.items() if kind == capacity_kind and not step}
        require(sizes.keys() == capacities.keys(), f"Unpaired storage entries: {prefix}")
        if prefix == "storage":
            number(size_kind, "simulation.object")
        totals = [0, 0]
        for name, size_row in sizes.items():
            capacity_row = capacities[name]
            require(size_row["status"] == capacity_row["status"], f"Storage status mismatch: {name}")
            if size_row["status"] == "available":
                size, capacity = int(size_row["value"]), int(capacity_row["value"])
                require(0 <= size <= capacity, f"Storage size exceeds capacity: {name}")
                totals[0] += size
                totals[1] += capacity
        for kind, total in zip((size_kind, capacity_kind), totals):
            require(int(get("summary", kind, statuses=("available", "partial"))) == total,
                    f"Storage subtotal mismatch: {kind}")
    return environment


def command_for(config, condition, output):
    command = [config["executable"], "--benchmark"]
    options = dict(implementation=condition["implementation"], agents=condition["boid_count"],
                   dummies=condition["dummy_count"], **{"dummy-change-percent": condition["dummy_change_percent"]},
                   seed=config["seed"], dt=config["dt"], warmup=config["warmup_steps"],
                   steps=config["measurement_steps"], output=str(output))
    for name, value in options.items():
        command.extend(("--" + name, str(value)))
    return command


def write_json(path, value):
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, ensure_ascii=False, allow_nan=False)
        stream.write("\n")


def run_suite(config, raw_config, source_path, cache):
    executable = Path(config["executable"])
    checksum = digest(executable)
    root = Path(config["output_directory"])
    root.mkdir(parents=True, exist_ok=True)
    folder = root / (datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ-") + uuid.uuid4().hex)
    folder.mkdir()  # Never reuse an earlier result directory.
    (folder / "config.json").write_bytes(raw_config)
    (folder / "CMakeCache.txt").write_bytes(cache.read_bytes())
    write_json(folder / "manifest.json", {
        "config_source": str(source_path), "resolved_config": config,
        "executable_sha256": checksum, "runner_sha256": digest(Path(__file__)),
        "python": sys.version, "working_directory": str(folder),
        "order": "repetition, then condition in configuration order (both one-based)",
    })
    print(f"Results: {folder}", flush=True)
    fields = ["order", "condition", "repetition", "implementation", "boid_count", "dummy_count",
              "dummy_change_percent", "seed", "dt", "warmup_steps", "measurement_steps",
              "started_utc", "finished_utc", "exit_code", "status", "error", "csv_path", "log_path",
              "command_path", "command"]
    successes = failures = 0
    baseline = None
    with (folder / "results.csv").open("x", newline="", encoding="utf-8") as listing:
        writer = csv.DictWriter(listing, fields)
        writer.writeheader()
        listing.flush()
        order = 0
        for repetition in range(1, config["repetitions"] + 1):
            for index, condition in enumerate(config["conditions"], 1):
                order += 1
                stem = f"{order:06d}-condition-{index:04d}-repeat-{repetition:04d}"
                csv_path = folder / (stem + ".csv")
                log_path = folder / (stem + ".log")
                command_path = folder / (stem + ".command.json")
                command = command_for(config, condition, csv_path)
                write_json(command_path, {"argv": command, "cwd": str(folder),
                                          "windows_command_line": subprocess.list2cmdline(command)})
                code = ""
                error = ""
                started = datetime.now(timezone.utc).isoformat()
                with log_path.open("xb") as log:
                    try:
                        require(digest(executable) == checksum, "Executable changed during suite")
                        # run() waits, and kills/reaps a timed-out child before raising.
                        completed = subprocess.run(command, cwd=folder, stdout=log, stderr=subprocess.STDOUT,
                                                   timeout=config.get("timeout_seconds"), check=False)
                        code = completed.returncode
                        require(code == 0, f"Process exited with code {code}")
                        require(digest(executable) == checksum, "Executable changed during experiment")
                        environment = validate_csv(csv_path, config, condition)
                        comparable = {key: environment[key] for key in (
                            "cpu", "os", "os_build", "compiler", "build", "build_flags",
                            "pointer_bits", "cxx_standard", "simulation_threads")}
                        require(baseline is None or baseline == comparable, "Build/environment changed during suite")
                        baseline = comparable
                    except (OSError, ValueError, csv.Error, subprocess.SubprocessError) as problem:
                        error = str(problem)
                        if isinstance(problem, subprocess.TimeoutExpired):
                            code = ""  # Timed out/killed; no ordinary child exit code was observed.
                        log.write(("\nRunner failure: " + error + "\n").encode("utf-8"))
                status = "failed" if error else "success"
                failures += bool(error)
                successes += not error
                writer.writerow(dict(order=order, condition=index, repetition=repetition, **condition,
                                     **{name: config[name] for name in ("seed", "dt", "warmup_steps", "measurement_steps")},
                                     started_utc=started, finished_utc=datetime.now(timezone.utc).isoformat(),
                                     exit_code=code, status=status, error=error, csv_path=str(csv_path),
                                     log_path=str(log_path), command_path=str(command_path),
                                     command=subprocess.list2cmdline(command)))
                listing.flush()
                print(f"[{order}] condition {index}, repetition {repetition}: {status}" +
                      (f" ({error})" if error else ""), flush=True)
    write_json(folder / "summary.json", {"successes": successes, "failures": failures,
                                         "total": successes + failures})
    print(f"Success: {successes}; failed: {failures}", flush=True)
    return int(failures != 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("config", type=Path, help="JSON suite configuration; paths are relative to this file")
    args = parser.parse_args()
    try:
        source = args.config.resolve()
        config, raw = load_config(source)
        cache = verify_executable(Path(config["executable"]))
        with batch_lock():
            return run_suite(config, raw, source, cache)
    except (OSError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
