"""Run independent, sequential VS2022 Release experiments (Python stdlib only)."""

import argparse
import copy
import csv
import ctypes
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import random
import struct
import subprocess
import sys
import time
import uuid
from contextlib import contextmanager
from xml.sax.saxutils import escape
from zipfile import ZIP_DEFLATED, ZipFile


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
    return resolve_config(config, path.parent), raw


def resolve_config(config, directory):
    """Validate settings shared by CLI and GUI without changing the input object."""
    config = copy.deepcopy(config)
    keys(config, ("version", "executable", "output_directory", "repetitions", "seed",
                  "dt", "warmup_steps", "measurement_steps", "conditions"), ("timeout_seconds", "order_seed"))
    require(type(config["version"]) is int and config["version"] == 1, "Expected version 1")
    for name in ("executable", "output_directory"):
        require(type(config[name]) is str and config[name].strip() and "\0" not in config[name],
                f"{name} must be a nonempty path")
        config[name] = str((directory / config[name]).resolve())
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
    if "order_seed" in config:
        integer(config["order_seed"], "order_seed", maximum=UINT32_MAX)
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
    return config


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


RESULT_FIELDS = [
    "order", "condition", "repetition", "implementation", "boid_count", "dummy_count",
    "dummy_change_percent", "seed", "dt", "warmup_steps", "measurement_steps", "attempt",
    "started_utc", "finished_utc", "exit_code", "status", "error", "csv_path", "log_path",
    "command_path", "command",
]
METRICS = [("summary", name) for name in (
    "initialization_ms", "total_step_ms", "mean_step_ms", "updates_per_second", "neighbor_ms_per_pair",
    "storage_size_bytes", "storage_capacity_bytes", "storage_size_bytes_per_entity",
    "storage_capacity_bytes_per_entity", "input_storage_size_bytes", "input_storage_capacity_bytes",
)] + [("section_mean_ms_per_step", name) for name in PHASES] + [
    ("process_memory_bytes", name) for name in ("peak_private_commit", "peak_working_set")]
METRIC_FIELDS = [field for _, name in METRICS for field in (name, name + "_status", name + "_reason")]
NUMERIC_FIELDS = set(RESULT_FIELDS[:3] + RESULT_FIELDS[4:12] + [name for _, name in METRICS])


def comparable_environment(environment):
    return {key: environment[key] for key in (
        "cpu", "os", "os_build", "os_display_version", "compiler", "build", "build_flags",
        "pointer_bits", "cxx_standard", "simulation_threads", "clock")}


def metrics_from_csv(path):
    with path.open(newline="", encoding="utf-8-sig") as stream:
        rows = {(row["kind"], row["name"]): row for row in csv.DictReader(stream) if row["step"] == ""}
    result = {}
    for kind, name in METRICS:
        row = rows[kind, name]
        result.update({name: row["value"], name + "_status": row["status"], name + "_reason": row["reason"]})
    return result


def task_list(config):
    tasks = []
    for repetition in range(1, config["repetitions"] + 1):
        indices = list(range(1, len(config["conditions"]) + 1))
        if "order_seed" in config:
            random.Random(config["order_seed"] + repetition).shuffle(indices)
        for index in indices:
            condition = config["conditions"][index - 1]
            tasks.append(dict(order=len(tasks) + 1, condition=index, repetition=repetition, **condition,
                              **{name: config[name] for name in ("seed", "dt", "warmup_steps", "measurement_steps")}))
    return tasks


def atomic_file(path, write):
    temporary = path.with_name(path.name + ".tmp-" + uuid.uuid4().hex)
    try:
        write(temporary)
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def write_workbook(path, sheets):
    """Small, dependency-free OOXML report. Text is always literal, never a formula."""
    ns = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"

    def column(index):
        value = ""
        while index:
            index, remainder = divmod(index - 1, 26)
            value = chr(65 + remainder) + value
        return value

    def xml_text(value):
        # XML 1.0 cannot represent control bytes sometimes present in OS errors.
        return escape("".join(c for c in str(value)[:32767] if c in "\t\n\r" or ord(c) >= 32))

    def export(target):
        with ZipFile(target, "w", compression=ZIP_DEFLATED) as archive:
            archive.writestr("[Content_Types].xml", '<?xml version="1.0" encoding="UTF-8"?>'
                '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
                '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
                '<Default Extension="xml" ContentType="application/xml"/>'
                '<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>'
                '<Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>' +
                ''.join(f'<Override PartName="/xl/worksheets/sheet{i}.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>'
                        for i in range(1, len(sheets) + 1)) + '</Types>')
            archive.writestr("_rels/.rels", '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
                '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>')
            archive.writestr("xl/workbook.xml", f'<workbook xmlns="{ns}" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets>' +
                ''.join(f'<sheet name="{name}" sheetId="{i}" r:id="rId{i}"/>' for i, (name, _, _) in enumerate(sheets, 1)) + '</sheets></workbook>')
            archive.writestr("xl/_rels/workbook.xml.rels", '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">' +
                ''.join(f'<Relationship Id="rId{i}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet{i}.xml"/>'
                        for i in range(1, len(sheets) + 1)) +
                '<Relationship Id="styles" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>')
            archive.writestr("xl/styles.xml", f'<styleSheet xmlns="{ns}"><fonts count="2"><font><sz val="11"/><name val="Calibri"/></font>'
                '<font><b/><color rgb="FFFFFFFF"/><sz val="11"/><name val="Calibri"/></font></fonts>'
                '<fills count="3"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill>'
                '<fill><patternFill patternType="solid"><fgColor rgb="FF24476B"/><bgColor indexed="64"/></patternFill></fill></fills>'
                '<borders count="1"><border/></borders><cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs>'
                '<cellXfs count="2"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>'
                '<xf numFmtId="0" fontId="1" fillId="2" borderId="0" xfId="0" applyFont="1" applyFill="1"/></cellXfs>'
                '<cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles></styleSheet>')
            for index, (_, fields, records) in enumerate(sheets, 1):
                require(len(records) < 1048576 and len(fields) <= 16384, "Excel worksheet size limit exceeded")
                lines = []
                for row_index, record in enumerate([dict(zip(fields, fields))] + records, 1):
                    cells = []
                    for col, field in enumerate(fields, 1):
                        value = record.get(field, "")
                        address = f'{column(col)}{row_index}'
                        if value == "":
                            continue
                        if row_index > 1 and field in NUMERIC_FIELDS:
                            cells.append(f'<c r="{address}"><v>{float(value):.17g}</v></c>')
                        else:
                            style = ' s="1"' if row_index == 1 else ''
                            cells.append(f'<c r="{address}" t="inlineStr"{style}><is><t xml:space="preserve">{xml_text(value)}</t></is></c>')
                    lines.append(f'<row r="{row_index}">' + ''.join(cells) + '</row>')
                cols = ''.join(f'<col min="{i}" max="{i}" width="{min(48, max(14, len(field) + 2))}" customWidth="1"/>' for i, field in enumerate(fields, 1))
                archive.writestr(f"xl/worksheets/sheet{index}.xml", f'<worksheet xmlns="{ns}"><sheetViews><sheetView workbookViewId="0">'
                    '<pane ySplit="1" topLeftCell="A2" activePane="bottomLeft" state="frozen"/></sheetView></sheetViews>'
                    f'<cols>{cols}</cols><sheetData>' + ''.join(lines) +
                    f'</sheetData><autoFilter ref="A1:{column(len(fields))}{len(records) + 1}"/></worksheet>')
    atomic_file(path, export)


def publish_results(folder, tasks, attempts, manifest, on_warning=None):
    latest = {int(row["order"]): row for row in attempts}
    records = [latest.get(task["order"], dict(task, status="pending")) for task in tasks]
    fields = RESULT_FIELDS + METRIC_FIELDS

    def export_csv(path):
        with path.open("w", newline="", encoding="utf-8-sig") as stream:
            writer = csv.DictWriter(stream, fields, extrasaction="ignore")
            writer.writeheader()
            writer.writerows(row for row in records if row["status"] != "pending")
    atomic_file(folder / "results.csv", export_csv)
    metadata = [{"name": name, "value": json.dumps(value, ensure_ascii=False) if isinstance(value, (dict, list)) else value}
                for name, value in manifest.items()]
    try:
        write_workbook(folder / "results.xlsx", [
            ("Runs", fields, records), ("Attempts", fields, attempts),
            ("Metadata", ["name", "value"], metadata)])
    except PermissionError:
        # Excel normally locks open workbooks. The durable journal and CSV still advance.
        message = "Excel file is open: results.xlsx will refresh after it is closed. CSV/journal saved."
        print(message, flush=True)
        if on_warning:
            on_warning(message)


def read_attempts(path):
    if not path.exists():
        return []
    rows = []
    valid_bytes = 0
    with path.open("rb") as stream:
        for line in stream:
            if not line.endswith(b"\n"):
                break  # Power loss can leave only the final append incomplete.
            rows.append(json.loads(line))
            valid_bytes += len(line)
    with path.open("r+b") as stream:
        stream.truncate(valid_bytes)
    return rows


def append_attempt(path, record):
    with path.open("ab") as stream:
        stream.write((json.dumps(record, ensure_ascii=False, allow_nan=False) + "\n").encode("utf-8"))
        stream.flush()
        os.fsync(stream.fileno())


@contextmanager
def child_job():
    """Kill the child even if the runner is forcibly closed on Windows."""
    if os.name != "nt":
        yield lambda child: None
        return
    from ctypes import wintypes

    class BasicLimits(ctypes.Structure):
        _fields_ = [("process_time", ctypes.c_int64), ("job_time", ctypes.c_int64),
                    ("flags", wintypes.DWORD), ("min_working_set", ctypes.c_size_t),
                    ("max_working_set", ctypes.c_size_t), ("active_processes", wintypes.DWORD),
                    ("affinity", ctypes.c_size_t), ("priority", wintypes.DWORD), ("scheduling", wintypes.DWORD)]

    class IoCounters(ctypes.Structure):
        _fields_ = [(name, ctypes.c_uint64) for name in ("read_ops", "write_ops", "other_ops", "read_bytes", "write_bytes", "other_bytes")]

    class Limits(ctypes.Structure):
        _fields_ = [("basic", BasicLimits), ("io", IoCounters)] + [
            (name, ctypes.c_size_t) for name in ("process_memory", "job_memory", "peak_process", "peak_job")]

    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateJobObjectW.argtypes = (ctypes.c_void_p, wintypes.LPCWSTR)
    kernel.CreateJobObjectW.restype = wintypes.HANDLE
    kernel.SetInformationJobObject.argtypes = (wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD)
    kernel.AssignProcessToJobObject.argtypes = (wintypes.HANDLE, wintypes.HANDLE)
    kernel.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
    job = kernel.CreateJobObjectW(None, None)
    if not job:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        limits = Limits()
        limits.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        if not kernel.SetInformationJobObject(job, 9, ctypes.byref(limits), ctypes.sizeof(limits)):
            raise ctypes.WinError(ctypes.get_last_error())

        def assign(child):
            handle = kernel.OpenProcess(0x0101, False, child.pid)  # SET_QUOTA | TERMINATE
            if not handle:
                if child.poll() is None:
                    raise ctypes.WinError(ctypes.get_last_error())
                return
            try:
                if not kernel.AssignProcessToJobObject(job, handle) and child.poll() is None:
                    raise ctypes.WinError(ctypes.get_last_error())
            finally:
                kernel.CloseHandle(handle)
        yield assign
    finally:
        kernel.CloseHandle(job)


class BenchmarkStopped(Exception):
    """Cooperative GUI stop; the current process is retried on resume."""


def run_process(command, *, cwd, stdout, stderr, timeout, progress, stop_requested=None):
    started = time.monotonic()
    with child_job() as assign, subprocess.Popen(command, cwd=cwd, stdout=stdout, stderr=stderr,
                          creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0) as child:
        try:
            assign(child)
            while True:
                if stop_requested and stop_requested():
                    raise BenchmarkStopped()
                elapsed = time.monotonic() - started
                if timeout is not None and elapsed >= timeout:
                    raise subprocess.TimeoutExpired(command, timeout)
                try:
                    code = child.wait(timeout=min(1, timeout - elapsed) if timeout is not None else 1)
                    return subprocess.CompletedProcess(command, code)
                except subprocess.TimeoutExpired:
                    progress()
        except BaseException:
            child.kill()
            child.wait()
            raise


def run_suite(config, raw_config, source_path, cache, *, fresh=False, resume=None,
              on_event=None, stop_requested=None):
    def emit(event, **values):
        if on_event:
            on_event(dict(event=event, **values))

    executable = Path(config["executable"])
    checksum = digest(executable)
    root = Path(config["output_directory"])
    root.mkdir(parents=True, exist_ok=True)
    identity = {"resolved_config": config, "executable_sha256": checksum,
                "runner_sha256": digest(Path(__file__)), "cache_sha256": digest(cache), "python": sys.version}
    folder = Path(resume).resolve() if resume else None
    if folder is None and not fresh:
        for candidate in sorted(root.iterdir(), reverse=True):
            if not (candidate / "manifest.json").is_file():
                continue
            saved = json.loads((candidate / "manifest.json").read_text(encoding="utf-8"))
            if saved.get("resume_version") == 1 and all(saved.get(key) == value for key, value in identity.items()):
                folder = candidate
                break
    if folder is not None:
        manifest = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
        require(manifest.get("resume_version") == 1 and all(manifest.get(k) == v for k, v in identity.items()),
                "Resume requires identical settings, executable, runner and CMake cache; use --new for a new suite")
        print(f"Resuming: {folder}", flush=True)
    else:
        folder = root / (datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ-") + uuid.uuid4().hex)
        folder.mkdir()
        (folder / "config.json").write_bytes(raw_config)
        (folder / "CMakeCache.txt").write_bytes(cache.read_bytes())
        manifest = dict(identity, resume_version=1, **{
        "config_source": str(source_path), "resolved_config": config,
        "executable_sha256": checksum, "runner_sha256": digest(Path(__file__)),
        "python": sys.version, "working_directory": str(folder),
        "order": "repetition, then seeded shuffled conditions" if "order_seed" in config else
                 "repetition, then condition in configuration order (both one-based)",
        })
        write_json(folder / "manifest.json", manifest)
    print(f"Results: {folder}", flush=True)
    tasks = task_list(config)
    emit("preparing", folder=str(folder), total=len(tasks))
    attempts = read_attempts(folder / "attempts.jsonl")
    latest = {}
    baseline = None
    for row in attempts:
        order = row.get("order")
        require(type(order) is int and 1 <= order <= len(tasks), "Invalid journal task")
        task = tasks[order - 1]
        require(all(row.get(key) == value for key, value in task.items()), "Journal settings mismatch")
        require(row.get("status") in ("success", "failed", "interrupted"), "Invalid journal status")
        latest[order] = row
    # Do not trust a checkpoint alone: every skipped CSV must still be complete.
    for order, row in latest.items():
        if stop_requested and stop_requested():
            emit("stopped", folder=str(folder))
            return 130
        if row["status"] != "success":
            continue
        condition = config["conditions"][tasks[order - 1]["condition"] - 1]
        try:
            environment = validate_csv(Path(row["csv_path"]), config, condition)
        except (OSError, ValueError, csv.Error) as error:
            row.update(status="failed", error=f"Saved CSV failed revalidation: {error}")
            continue
        comparable = comparable_environment(environment)
        require(baseline is None or baseline == comparable, "Saved results have different environments")
        baseline = comparable
        row.update(metrics_from_csv(Path(row["csv_path"])))
    skipped = sum(row["status"] == "success" for row in latest.values())
    suite_started = time.monotonic()
    processed = 0
    successes, failures = skipped, 0
    # An interrupted resumption must not leave an earlier 'finished' summary behind.
    (folder / "summary.json").unlink(missing_ok=True)
    warn = lambda message: emit("warning", message=message)
    publish_results(folder, tasks, attempts, manifest, on_warning=warn)
    emit("ready", total=len(tasks), completed=skipped, skipped=skipped, successes=successes, failures=0)
    try:
        for task in tasks:
            if stop_requested and stop_requested():
                emit("stopped", folder=str(folder))
                return 130
            order, index, repetition = task["order"], task["condition"], task["repetition"]
            if order in latest and latest[order]["status"] == "success":
                continue
            condition = config["conditions"][index - 1]
            attempt = max((row.get("attempt", 1) for row in attempts if row["order"] == order), default=0) + 1
            stem = f"{order:06d}-condition-{index:04d}-repeat-{repetition:04d}"
            # A process may have finished immediately before the parent was killed.
            # Keep all orphaned artifacts and start the task in a fresh attempt.
            while any(folder.glob(stem + f"-attempt-{attempt:04d}.*")):
                attempt += 1
            stem += f"-attempt-{attempt:04d}"
            csv_path, log_path, command_path = [folder / (stem + suffix) for suffix in (".csv", ".log", ".command.json")]
            command = command_for(config, condition, csv_path)
            write_json(command_path, {"argv": command, "cwd": str(folder),
                                      "windows_command_line": subprocess.list2cmdline(command)})
            code, error, interrupted = "", "", False
            started = datetime.now(timezone.utc).isoformat()
            run_started = time.monotonic()

            def progress():
                fraction = (skipped + processed) / len(tasks)
                bar = "#" * int(fraction * 24) + "-" * (24 - int(fraction * 24))
                print(f"\r[{bar}] {fraction:6.1%} {skipped + processed}/{len(tasks)} "
                      f"{condition['implementation']} B={condition['boid_count']} D={condition['dummy_count']} "
                      f"change={condition['dummy_change_percent']}% repeat={repetition} "
                      f"current={time.monotonic() - run_started:.0f}s "
                      f"elapsed={time.monotonic() - suite_started:.0f}s", end="", flush=True)
                emit("progress", task=task, total=len(tasks), completed=skipped + processed,
                     skipped=skipped, successes=successes, failures=failures,
                     current_seconds=time.monotonic() - run_started,
                     elapsed_seconds=time.monotonic() - suite_started)

            progress()
            metrics = {}
            with log_path.open("xb") as log:
                try:
                    require(digest(executable) == checksum, "Executable changed during suite")
                    result = run_process(command, cwd=folder, stdout=log, stderr=subprocess.STDOUT,
                                         timeout=config.get("timeout_seconds"), progress=progress,
                                         stop_requested=stop_requested)
                    code = result.returncode
                    require(code == 0, f"Process exited with code {code}")
                    require(digest(executable) == checksum, "Executable changed during experiment")
                    environment = validate_csv(csv_path, config, condition)
                    comparable = comparable_environment(environment)
                    require(baseline is None or baseline == comparable, "Build/environment changed during suite")
                    baseline = comparable
                    metrics = metrics_from_csv(csv_path)
                except (KeyboardInterrupt, BenchmarkStopped):
                    error, interrupted = "Interrupted by user; retry on resume", True
                except (OSError, ValueError, csv.Error, subprocess.SubprocessError) as problem:
                    error = str(problem)
                if error:
                    log.write(("\nRunner failure: " + error + "\n").encode("utf-8"))
            record = dict(task, attempt=attempt, started_utc=started,
                          finished_utc=datetime.now(timezone.utc).isoformat(), exit_code=code,
                          status="interrupted" if interrupted else "failed" if error else "success",
                          error=error, csv_path=str(csv_path), log_path=str(log_path),
                          command_path=str(command_path), command=subprocess.list2cmdline(command), **metrics)
            append_attempt(folder / "attempts.jsonl", record)
            attempts.append(record)
            latest[order] = record
            processed += 1
            successes += not error
            failures += bool(error) and not interrupted
            print(f" -> {record['status']}" + (f" ({error})" if error else ""), flush=True)
            publish_results(folder, tasks, attempts, manifest, on_warning=warn)
            emit("run_finished", record=record, total=len(tasks), completed=skipped + processed,
                 skipped=skipped, successes=successes, failures=failures,
                 elapsed_seconds=time.monotonic() - suite_started)
            if interrupted:
                print("Stopped. Run the same command to resume.", flush=True)
                emit("stopped", folder=str(folder))
                return 130
    except KeyboardInterrupt:
        print("\nStopped between runs. Run the same command to resume.", flush=True)
        emit("stopped", folder=str(folder))
        return 130
    successes = sum(row["status"] == "success" for row in latest.values())
    failures = len(tasks) - successes
    atomic_file(folder / "summary.json", lambda path: write_json(path, {
        "successes": successes, "failures": failures, "total": len(tasks)}))
    print(f"[########################] 100.0% {len(tasks)}/{len(tasks)} (skipped {skipped})", flush=True)
    print(f"Success: {successes}; failed: {failures}", flush=True)
    emit("finished", successes=successes, failures=failures, total=len(tasks), completed=len(tasks),
         skipped=skipped, folder=str(folder))
    return int(failures != 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("config", type=Path, help="JSON suite configuration; paths are relative to this file")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--new", action="store_true", help="Start a new suite instead of resuming matching results")
    mode.add_argument("--resume", type=Path, help="Resume this exact result directory")
    args = parser.parse_args()
    try:
        source = args.config.resolve()
        config, raw = load_config(source)
        cache = verify_executable(Path(config["executable"]))
        with batch_lock():
            return run_suite(config, raw, source, cache, fresh=args.new, resume=args.resume)
    except KeyboardInterrupt:
        print("\nStopped. Run the same command to resume.", file=sys.stderr)
        return 130
    except (OSError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
