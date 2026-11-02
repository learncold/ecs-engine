"""Desktop editor and controller for the existing headless benchmark runner."""

import argparse
import copy
import json
import os
from pathlib import Path
import queue
import re
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, scrolledtext, ttk

import run_benchmarks as batch
from benchmark_project import BenchmarkProject, FIELD_NAMES


ROOT = Path(__file__).resolve().parents[1]
IMPLEMENTATIONS = ("oop", "ecs", "archetype")
LABELS = {"oop": "OOP", "ecs": "Sparse Set", "archetype": "Archetype"}
SETTINGS = (
    ("repetitions", "반복 횟수"), ("warmup_steps", "Warmup step"),
    ("measurement_steps", "측정 step"), ("dt", "dt (초)"),
    ("seed", "시뮬레이션 seed"), ("order_seed", "순서 seed (선택)"),
    ("timeout_seconds", "실행 제한 초 (선택)"),
)


def integer_text(value, name):
    value = value.strip()
    batch.require(bool(re.fullmatch(r"[0-9]+", value)), f"{name}: 0 이상의 정수를 입력하세요.")
    return int(value)


def condition_for(implementation, boids, dummies, percent):
    batch.require(implementation in IMPLEMENTATIONS, "구현을 선택하세요.")
    batch.integer(boids, "Boid 수")
    batch.integer(dummies, "Dummy 수")
    batch.integer(percent, "변경률", 0, 100)
    batch.require(boids + dummies <= batch.UINT32_MAX, "전체 개체 수가 ECS ID 범위를 초과합니다.")
    return dict(implementation=implementation, boid_count=boids,
                dummy_count=dummies, dummy_change_percent=percent)


def generate_conditions(boid_text, dummy_text, percent_text, implementations):
    def tokens(value, name):
        result = [part for part in re.split(r"[,\s]+", value.strip()) if part]
        batch.require(result, f"{name} 목록을 입력하세요.")
        return list(dict.fromkeys(result))

    boids = [integer_text(value, "Boid 수") for value in tokens(boid_text, "Boid 수")]
    dummy_tokens = tokens(dummy_text, "Dummy 수")
    percents = [integer_text(value, "변경률") for value in tokens(percent_text, "변경률")]
    for percent in percents:
        batch.integer(percent, "변경률", 0, 100)
    batch.require(implementations, "하나 이상의 구현을 선택하세요.")
    batch.require(len(boids) * len(dummy_tokens) * len(percents) * len(implementations) <= 10000,
                  "한 번에 추가할 조건은 10,000개 이하로 나누어 입력하세요.")
    result, seen = [], set()
    for boid_count in boids:
        for token in dummy_tokens:
            ratio = re.fullmatch(r"([0-9]*)[bB]", token)
            dummy_count = boid_count * int(ratio[1] or "1") if ratio else integer_text(token, "Dummy 수")
            for percent in ([0] if dummy_count == 0 else percents):
                for implementation in implementations:
                    condition = condition_for(implementation, boid_count, dummy_count, percent)
                    key = tuple(condition.values())
                    if key not in seen:
                        seen.add(key)
                        result.append(condition)
    return result


def config_from_form(values, conditions):
    config = dict(version=1, conditions=copy.deepcopy(conditions))
    for key in ("executable", "output_directory"):
        config[key] = values[key].strip()
    for key, label in SETTINGS:
        value = values[key].strip()
        if not value and key in ("order_seed", "timeout_seconds"):
            continue
        if key == "dt":
            try:
                config[key] = float(value)
            except ValueError as error:
                raise ValueError("dt: 양수인 초 단위 숫자를 입력하세요.") from error
        else:
            config[key] = integer_text(value, label)
    return batch.resolve_config(config, ROOT)


def save_settings(path, config):
    # Absolute paths preserve executable/output locations when saving elsewhere.
    resolved = batch.resolve_config(config, path.parent)
    raw = (json.dumps(resolved, ensure_ascii=False, indent=2, allow_nan=False) + "\n").encode("utf-8")
    batch.atomic_file(path, lambda temporary: temporary.write_bytes(raw))
    return resolved


class SuiteWorker:
    """No Tk calls here: all notifications cross the thread boundary via a queue."""

    def __init__(self, events):
        self.events = events
        self.stop_event = threading.Event()
        self.thread = None

    @property
    def running(self):
        return self.thread is not None and self.thread.is_alive()

    def start(self, config, raw, source, *, fresh=False, resume=None):
        batch.require(not self.running, "이미 측정 중입니다.")
        self.stop_event.clear()
        self.thread = threading.Thread(target=self._run,
                                       args=(copy.deepcopy(config), raw, source, fresh, resume),
                                       name="benchmark-controller")
        self.thread.start()

    def _run(self, config, raw, source, fresh, resume):
        code = 1
        try:
            cache = batch.verify_executable(Path(config["executable"]))
            with batch.batch_lock():
                code = batch.run_suite(config, raw, source, cache, fresh=fresh, resume=resume,
                                       on_event=self.events.put, stop_requested=self.stop_event.is_set)
        except Exception as error:
            self.events.put(dict(event="error", message=str(error)))
        finally:
            self.events.put(dict(event="worker_done", code=code))

    def stop(self):
        self.stop_event.set()


class BenchmarkerWindow:
    def __init__(self, root, project):
        self.root, self.project = root, project
        self.events = queue.Queue()
        self.worker = SuiteWorker(self.events)
        self.conditions, self.editable = [], []
        self.config_path = project.path
        self.result_folder = self.active_run = None
        self.records = {}
        self.history_loading = False
        self.dirty = self.loading = self.running = self.closing = False
        self.values = {key: tk.StringVar() for key in FIELD_NAMES}
        self.path_label = tk.StringVar(value="저장하지 않은 설정")
        self.summary = tk.StringVar()
        self.status = tk.StringVar(value="조건을 확인한 뒤 시작하세요.")
        self.progress_text = tk.StringVar(value="0 / 0 · 성공 0 · 실패 0")
        self.current = tk.StringVar(value="대기 중")
        self.result_text = tk.StringVar(value="결과 폴더가 아직 없습니다.")
        self.boid_list = tk.StringVar()
        self.dummy_list = tk.StringVar()
        self.percent_list = tk.StringVar()
        self.generator_preview = tk.StringVar()
        self.implementations = {key: tk.BooleanVar(value=True) for key in IMPLEMENTATIONS}
        self._build()
        for variable in self.values.values():
            variable.trace_add("write", self._changed)
        for variable in (self.boid_list, self.dummy_list, self.percent_list, *self.implementations.values()):
            variable.trace_add("write", self._update_generator_preview)
        self._restore_project()
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        self.poll_id = self.root.after(100, self._poll)

    def _button(self, parent, text, command, *, editable=True):
        button = ttk.Button(parent, text=text, command=command)
        if editable:
            self.editable.append(button)
        return button

    def _entry(self, parent, variable, **kwargs):
        entry = ttk.Entry(parent, textvariable=variable, **kwargs)
        self.editable.append(entry)
        return entry

    def _build(self):
        self.root.title(f"Benchmarker · {self.project.directory.name}")
        self.root.geometry("1080x820")
        self.root.minsize(920, 720)
        style = ttk.Style(self.root)
        if "vista" in style.theme_names():
            style.theme_use("vista")
        self.root.option_add("*Font", "{맑은 고딕} 10")
        style.configure("Treeview", rowheight=27)
        outer = ttk.Frame(self.root, padding=14)
        outer.pack(fill="both", expand=True)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(1, weight=1)
        header = ttk.Frame(outer)
        header.grid(row=0, column=0, sticky="ew", pady=(0, 10))
        self._button(header, "프로젝트 저장", self.save_project, editable=False).pack(side="right")
        ttk.Label(header, textvariable=self.path_label, wraplength=850).pack(side="left")
        self.notebook = ttk.Notebook(outer)
        self.notebook.grid(row=1, column=0, sticky="nsew")
        settings = ttk.Frame(self.notebook, padding=10)
        execution = ttk.Frame(self.notebook, padding=10)
        self.notebook.add(settings, text="실험 설정")
        self.notebook.add(execution, text="실행")
        settings.columnconfigure(0, weight=1)
        settings.rowconfigure(1, weight=1)
        common = ttk.LabelFrame(settings, text="공통 측정 조건", padding=10)
        common.grid(row=0, column=0, sticky="ew", pady=(0, 10))
        for col in range(4):
            common.columnconfigure(col * 2 + 1, weight=1)
        ttk.Label(common, text="Release 실행 파일").grid(row=0, column=0, sticky="w")
        self._entry(common, self.values["executable"]).grid(row=0, column=1, columnspan=6, sticky="ew", padx=8)
        self._button(common, "찾아보기", lambda: self._browse("executable")).grid(row=0, column=7)
        for index, (key, label) in enumerate(SETTINGS):
            row, col = 1 + index // 4, (index % 4) * 2
            ttk.Label(common, text=label).grid(row=row, column=col, sticky="w", pady=6)
            self._entry(common, self.values[key], width=12).grid(row=row, column=col + 1, sticky="ew", padx=(8, 12))
        imports = ttk.Frame(common)
        imports.grid(row=3, column=0, columnspan=8, sticky="ew", pady=(5, 0))
        self._button(imports, "기존 설정 JSON 가져오기", self.load_dialog).pack(side="left")
        self._button(imports, "실행 설정 JSON 내보내기", self.save_dialog).pack(side="left", padx=6)
        ttk.Label(imports, text="실행 결과는 프로젝트의 Result 폴더에 저장됩니다.", foreground="#526278").pack(side="right")
        conditions = ttk.Frame(settings)
        conditions.grid(row=1, column=0, sticky="nsew")
        conditions.columnconfigure(0, weight=1)
        conditions.rowconfigure(2, weight=1)
        generator = ttk.LabelFrame(conditions, text="새 조건 추가 · 입력한 조합을 아래 표에 추가합니다", padding=8)
        generator.grid(row=0, column=0, sticky="ew")
        for col in (1, 3, 5):
            generator.columnconfigure(col, weight=1)
        for col, label, var in ((0, "Boid 수", self.boid_list), (2, "Dummy 수", self.dummy_list), (4, "변경률 %", self.percent_list)):
            ttk.Label(generator, text=label).grid(row=0, column=col, sticky="w")
            self._entry(generator, var, width=18).grid(row=0, column=col + 1, sticky="ew", padx=(8, 14))
        ttk.Label(generator, textvariable=self.generator_preview, foreground="#526278").grid(
            row=1, column=0, columnspan=6, sticky="w", pady=(6, 0))
        tools = ttk.Frame(conditions)
        tools.grid(row=1, column=0, sticky="ew", pady=8)
        for key in IMPLEMENTATIONS:
            check = ttk.Checkbutton(tools, text=LABELS[key], variable=self.implementations[key])
            check.pack(side="left", padx=(0, 12))
            self.editable.append(check)
        self.add_button = self._button(tools, "표에 조건 추가", self.add_combinations)
        self.add_button.pack(side="left")
        ttk.Label(tools, text="쉼표로 구분 · Dummy는 B, 4B 사용 가능", foreground="#526278").pack(side="left", padx=12)
        self._button(tools, "선택 삭제", self.delete_selected).pack(side="right")
        self._button(tools, "전체 지우기", self.clear_conditions).pack(side="right", padx=6)
        table_frame = ttk.Frame(conditions)
        table_frame.grid(row=2, column=0, sticky="nsew")
        table_frame.columnconfigure(0, weight=1)
        table_frame.rowconfigure(0, weight=1)
        self.table = ttk.Treeview(table_frame, columns=("implementation", "boids", "dummies", "percent", "changes"), show="headings", height=6)
        for key, text in (("implementation", "구현"), ("boids", "Boid 수"), ("dummies", "Dummy 수"), ("percent", "변경률 (%)"), ("changes", "step당 변경 수")):
            self.table.heading(key, text=text)
            self.table.column(key, width=145, anchor="center")
        self.table.grid(row=0, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(table_frame, command=self.table.yview)
        scroll.grid(row=0, column=1, sticky="ns")
        self.table.configure(yscrollcommand=scroll.set)
        self.table.bind("<Double-1>", self.edit_selected)
        self.table.bind("<Delete>", lambda _: self.delete_selected())
        bottom = ttk.Frame(conditions)
        bottom.grid(row=3, column=0, sticky="ew", pady=(8, 0))
        ttk.Label(bottom, textvariable=self.summary).pack(side="left")
        ttk.Label(bottom, text="행을 두 번 클릭하면 수정", foreground="#526278").pack(side="right")


        execution.columnconfigure(0, weight=1)
        execution.rowconfigure(0, weight=1)
        history = ttk.LabelFrame(execution, text="실행 기록 · 선택한 기록의 조건과 진행 상태를 확인합니다", padding=10)
        history.grid(row=0, column=0, sticky="nsew", pady=(0, 10))
        history.columnconfigure(0, weight=1)
        history.rowconfigure(0, weight=2)
        history.rowconfigure(2, weight=1)
        self.history_table = ttk.Treeview(history, columns=("name", "status", "completed", "total", "successes", "failures"), show="headings", height=7)
        for key, label, width in (("name", "실행 시각 (UTC)", 330), ("status", "상태", 120),
                                  ("completed", "처리", 80), ("total", "전체", 80),
                                  ("successes", "성공", 80), ("failures", "실패", 80)):
            self.history_table.heading(key, text=label)
            self.history_table.column(key, width=width, anchor="center")
        self.history_table.grid(row=0, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(history, command=self.history_table.yview)
        scroll.grid(row=0, column=1, sticky="ns")
        self.history_table.configure(yscrollcommand=scroll.set)
        self.history_table.bind("<<TreeviewSelect>>", self._history_selected)
        self.record_summary = tk.StringVar(value="새 실험을 실행하거나 기록을 선택하세요.")
        ttk.Label(history, textvariable=self.record_summary, wraplength=960).grid(row=1, column=0, sticky="w", pady=6)
        self.log = scrolledtext.ScrolledText(history, height=5, wrap="word", state="disabled")
        self.log.grid(row=2, column=0, columnspan=2, sticky="nsew")
        progress = ttk.LabelFrame(execution, text="실행 상태", padding=10)
        progress.grid(row=1, column=0, sticky="ew")
        progress.columnconfigure(0, weight=1)
        actions = ttk.Frame(progress)
        actions.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        self.start_button = self._button(actions, "새 실험 실행", self.start)
        self.start_button.pack(side="left")
        self.resume_button = self._button(actions, "선택 기록 이어서 실행", lambda: self.start(fresh=False))
        self.resume_button.pack(side="left", padx=6)
        self.stop_button = self._button(actions, "중단", self.stop, editable=False)
        self.stop_button.pack(side="left")
        self.stop_button.state(["disabled"])
        self._button(actions, "결과 폴더", lambda: self.open_result(False), editable=False).pack(side="right")
        self._button(actions, "Excel 열기", lambda: self.open_result(True), editable=False).pack(side="right", padx=6)
        ttk.Label(progress, textvariable=self.status, wraplength=980).grid(row=1, column=0, sticky="w")
        self.progress = ttk.Progressbar(progress, maximum=1)
        self.progress.grid(row=2, column=0, sticky="ew", pady=7)
        ttk.Label(progress, textvariable=self.progress_text).grid(row=3, column=0, sticky="w")
        ttk.Label(progress, textvariable=self.current, wraplength=980).grid(row=4, column=0, sticky="w", pady=3)
        ttk.Label(progress, textvariable=self.result_text, foreground="#526278", wraplength=980).grid(row=5, column=0, sticky="w")

    def _changed(self, *_):
        if self.loading:
            return
        self.dirty = True
        self._update_title()
        self._update_summary()
        self._update_generator_preview()

    def _update_title(self):
        self.path_label.set(f"{self.project.directory.name} · {self.project.directory}" +
                            ("  · 변경됨 (종료 시 저장)" if self.dirty else ""))

    def _update_summary(self):
        try:
            repetitions = integer_text(self.values["repetitions"].get(), "반복 횟수")
            self.summary.set(f"{len(self.conditions):,}개 조건 × {repetitions:,}회 = 총 {len(self.conditions) * repetitions:,}회")
        except ValueError:
            self.summary.set(f"{len(self.conditions):,}개 조건 · 반복 횟수를 입력하세요.")

    def _refresh_table(self):
        self.table.delete(*self.table.get_children())
        for index, condition in enumerate(self.conditions):
            self.table.insert("", "end", iid=str(index), values=(LABELS[condition["implementation"]],
                condition["boid_count"], condition["dummy_count"], condition["dummy_change_percent"],
                condition["dummy_count"] * condition["dummy_change_percent"] // 100))
        self._update_summary()
        self._update_generator_preview()

    def _reset_generator(self):
        for variable in (self.boid_list, self.dummy_list, self.percent_list):
            variable.set("")
        present = {condition["implementation"] for condition in self.conditions}
        for key, selected in self.implementations.items():
            selected.set(key in present if present else True)
        self._update_generator_preview()

    def _new_conditions(self):
        generated = generate_conditions(self.boid_list.get(), self.dummy_list.get(), self.percent_list.get(),
                                        [key for key, selected in self.implementations.items() if selected.get()])
        key = lambda c: (c["implementation"], c["boid_count"], c["dummy_count"], c["dummy_change_percent"])
        existing = {key(condition) for condition in self.conditions}
        return [condition for condition in generated if key(condition) not in existing], len(generated)

    def _update_generator_preview(self, *_):
        if not hasattr(self, "add_button"):
            return
        count = 0
        if not any(variable.get().strip() for variable in (self.boid_list, self.dummy_list, self.percent_list)):
            self.generator_preview.set("새 조건을 입력하세요. 저장·측정 대상은 아래 표입니다.")
        else:
            try:
                added, total = self._new_conditions()
                count = len(added)
                repetitions = self.values["repetitions"].get()
                suffix = f" · 반복 {repetitions}회 적용" if repetitions.isdecimal() else ""
                self.generator_preview.set(f"추가 예정: 새 조건 {count:,}개 · 기존 중복 {total - count:,}개 제외" + suffix)
            except ValueError as error:
                self.generator_preview.set(str(error))
        self.add_button.state(["disabled"] if self.running or count == 0 else ["!disabled"])

    def apply_config(self, config, path=None):
        self.loading = True
        try:
            self.conditions = copy.deepcopy(config["conditions"])
            for key, variable in self.values.items():
                variable.set(str(config.get(key, "")))
            self.config_path = self.project.path
            self.dirty = False
            self._reset_generator()
            self._refresh_table()
            self._update_title()
        finally:
            self.loading = False

    def form_config(self):
        values = {key: var.get() for key, var in self.values.items()}
        values["output_directory"] = str(self.project.results)
        return config_from_form(values, self.conditions)

    def _show_error(self, error):
        messagebox.showerror("확인이 필요합니다", str(error), parent=self.root)

    def _confirm_replace(self):
        if not self.dirty:
            return True
        answer = messagebox.askyesnocancel("설정 변경", "현재 편집 상태를 프로젝트에 저장한 뒤 가져올까요?", parent=self.root)
        return self.save_project() if answer else answer is False

    def load_path(self, path):
        config, _ = batch.load_config(Path(path).resolve())
        self.apply_config(config)
        self.dirty = True
        self._update_title()

    def load_dialog(self):
        if not self._confirm_replace():
            return
        path = filedialog.askopenfilename(parent=self.root, title="기존 측정 설정 가져오기", filetypes=[("JSON 설정", "*.json")])
        if path:
            try:
                self.load_path(Path(path))
                self.status.set("설정을 가져왔습니다. 새 실험에 사용할 조건을 확인하세요.")
            except (OSError, ValueError) as error:
                self._show_error(error)

    def save_dialog(self):
        try:
            config = self.form_config()
            chosen = filedialog.asksaveasfilename(parent=self.root, title="CLI 실행 설정 내보내기",
                defaultextension=".json", initialfile="experiment.json", filetypes=[("JSON 설정", "*.json")])
            if not chosen:
                return False
            path = Path(chosen).resolve()
            batch.require(path != self.project.path, "project.json은 프로젝트 저장 버튼으로 저장하세요.")
            save_settings(path, config)
            return True
        except (OSError, ValueError) as error:
            self._show_error(error)
            return False

    def save_project(self):
        try:
            self._persist_project()
            self.status.set("프로젝트를 저장했습니다.")
            return True
        except (OSError, ValueError) as error:
            self._show_error(error)
            return False

    def _browse(self, key):
        if key == "executable":
            path = filedialog.askopenfilename(parent=self.root, title="Release App.exe 선택", filetypes=[("실행 파일", "*.exe")])
        else:
            path = filedialog.askdirectory(parent=self.root, title="결과 저장 위치 선택")
        if path:
            self.values[key].set(path)

    def add_combinations(self):
        if self.running:
            return
        try:
            added, _ = self._new_conditions()
            if not added:
                self.status.set("입력한 조건이 이미 표에 있습니다. 추가된 조건은 없습니다.")
                return
            self.conditions.extend(added)
            self._changed()
            self._refresh_table()
            self.status.set(f"표에 새 조건 {len(added):,}개를 추가했습니다. 저장·측정할 목록을 확인하세요.")
        except ValueError as error:
            self._show_error(error)

    def delete_selected(self):
        if self.running:
            return
        selected = {int(item) for item in self.table.selection()}
        self.conditions = [condition for index, condition in enumerate(self.conditions) if index not in selected]
        self._changed()
        self._refresh_table()

    def clear_conditions(self):
        self.conditions = []
        self._changed()
        self._refresh_table()

    def edit_selected(self, _=None):
        if self.running or not self.table.selection():
            return
        index = int(self.table.selection()[0])
        condition = self.conditions[index]
        dialog = tk.Toplevel(self.root)
        dialog.title("측정 조건 수정")
        dialog.transient(self.root)
        dialog.resizable(False, False)
        variables = {key: tk.StringVar(value=str(value)) for key, value in condition.items()}
        for row, (key, label) in enumerate((("implementation", "구현"), ("boid_count", "Boid 수"), ("dummy_count", "Dummy 수"), ("dummy_change_percent", "변경률 (%)"))):
            ttk.Label(dialog, text=label).grid(row=row, column=0, padx=12, pady=8, sticky="w")
            widget = ttk.Combobox(dialog, textvariable=variables[key], values=IMPLEMENTATIONS, state="readonly") if key == "implementation" else ttk.Entry(dialog, textvariable=variables[key])
            widget.grid(row=row, column=1, padx=12, pady=8)

        def apply():
            try:
                self.conditions[index] = condition_for(variables["implementation"].get(),
                    integer_text(variables["boid_count"].get(), "Boid 수"), integer_text(variables["dummy_count"].get(), "Dummy 수"),
                    integer_text(variables["dummy_change_percent"].get(), "변경률"))
                self._changed()
                self._refresh_table()
                dialog.destroy()
            except ValueError as error:
                messagebox.showerror("입력 확인", str(error), parent=dialog)
        ttk.Button(dialog, text="적용", command=apply).grid(row=4, column=1, sticky="e", padx=12, pady=12)
        dialog.bind("<Return>", lambda _: apply())
        dialog.bind("<Escape>", lambda _: dialog.destroy())
        dialog.grab_set()

    def _refresh_history(self, select=None):
        self.history_loading = True
        try:
            previous = select or (self.history_table.selection() or (None,))[0]
            if not isinstance(previous, str):
                previous = None
            self.records = {row["name"]: row for row in self.project.history()}
            self.history_table.delete(*self.history_table.get_children())
            for name, row in self.records.items():
                status = "실행 중" if self.running and name == self.active_run else row["status"]
                self.history_table.insert("", "end", iid=name, values=(name.split("Z-")[0] + "Z", status,
                    row["completed"], row["total"], row["successes"], row["failures"]))
            selected = previous if previous in self.records else next(iter(self.records), None)
            if selected:
                self.history_table.selection_set(selected)
                self.history_table.see(selected)
                if not self.running:
                    self._show_record(selected)
            elif not self.running:
                self.result_folder = None
                self.resume_button.state(["disabled"])
        finally:
            self.history_loading = False

    def _history_selected(self, _=None):
        if self.history_loading:
            return
        selection = self.history_table.selection()
        if self.running:
            if self.active_run in self.records and selection != (self.active_run,):
                self.history_table.selection_set(self.active_run)
            return
        if selection:
            self._show_record(selection[0])

    def _show_record(self, name):
        row = self.records[name]
        self.result_folder = row["folder"]
        self.result_text.set(str(self.result_folder))
        self.progress.configure(maximum=max(1, row["total"]), value=row["completed"])
        skipped = self.project.state["view"].get("skipped", {}).get(name, 0)
        fraction = row["completed"] / row["total"] if row["total"] else 0
        self.progress_text.set(f"{row['completed']:,} / {row['total']:,} ({fraction:.1%}) · 성공 {row['successes']:,} · 실패 {row['failures']:,} · 건너뜀 {skipped:,}")
        self.status.set(row["status"] + (" · 선택 기록 이어서 실행으로 재개할 수 있습니다." if not row["error"] else " · " + row["error"]))
        self.current.set("선택한 실행 기록의 상태입니다.")
        config = row["config"]
        if config:
            self.record_summary.set(f"{len(config['conditions'])}개 조건 × {config['repetitions']}회 · warmup {config['warmup_steps']} / 측정 {config['measurement_steps']} step · seed {config['seed']}")
        else:
            self.record_summary.set(row["error"])
        lines = [self._attempt_line(attempt) for attempt in row["attempts"]]
        lines += self.project.state["messages"].get(name, [])
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.insert("end", "\n".join(lines) + ("\n" if lines else ""))
        self.log.see("end")
        self.log.configure(state="disabled")
        self.resume_button.state(["disabled"] if row["error"] else ["!disabled"])

    @staticmethod
    def _attempt_line(row):
        return (f"#{row['order']} {LABELS.get(row.get('implementation'), '?')} B={row.get('boid_count', '?')} D={row.get('dummy_count', '?')} "
                f"변경={row.get('dummy_change_percent', '?')}% 반복={row.get('repetition', '?')} 시도={row.get('attempt', '?')}: {row['status']}" +
                (f" · {row['error']}" if row.get("error") else ""))

    def start(self, fresh=True):
        if self.running:
            return
        try:
            if fresh:
                config, resume = self.form_config(), None
            else:
                selected = self.history_table.selection()
                batch.require(selected, "이어서 실행할 기록을 선택하세요.")
                config = self.project.run_config(selected[0])
                resume = self.project.run_folder(selected[0])
            raw = (json.dumps(config, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
            self._persist_project()
            self.active_run = resume.name if resume else None
            self._set_running(True)
            self.notebook.select(1)
            self.status.set("실행 파일과 저장된 결과를 확인하는 중…")
            self.current.set("준비 중")
            self.progress.configure(value=0, maximum=len(config["conditions"]) * config["repetitions"])
            self.progress_text.set(f"0 / {int(self.progress['maximum']):,} · 준비 중")
            self.worker.start(config, raw, self.project.path, fresh=fresh, resume=resume)
        except (OSError, ValueError, KeyError) as error:
            self._set_running(False)
            self._show_error(error)

    def _set_running(self, running):
        self.running = running
        for widget in self.editable:
            widget.state(["disabled"] if running else ["!disabled"])
        self.stop_button.state(["!disabled"] if running else ["disabled"])
        self._update_generator_preview()
        if not running and not self.history_table.selection():
            self.resume_button.state(["disabled"])

    def stop(self):
        if self.running:
            self.worker.stop()
            self.stop_button.state(["disabled"])
            self.status.set("현재 프로세스를 종료하고 결과를 저장하는 중…")

    def _append_log(self, text, persist=True):
        self.log.configure(state="normal")
        self.log.insert("end", text + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")
        if persist:
            key = self.active_run or (self.result_folder.name if self.result_folder else "_project")
            self.project.state["messages"].setdefault(key, []).append(text)

    def handle_event(self, event):
        kind = event["event"]
        if kind == "preparing":
            self.result_folder = Path(event["folder"])
            self.active_run = self.result_folder.name
            self.result_text.set(str(self.result_folder))
            self.status.set("완료된 결과를 검증하고 측정을 준비하는 중…")
            self._refresh_history(self.active_run)
        if kind in ("ready", "progress", "run_finished", "finished"):
            self.progress.configure(maximum=event["total"], value=event["completed"])
            self.progress_text.set(f"{event['completed']:,} / {event['total']:,} "
                f"({event['completed'] / event['total']:.1%}) · 성공 {event['successes']:,} · 실패 {event['failures']:,} · 건너뜀 {event['skipped']:,}")
            if self.active_run:
                self.project.state["view"].setdefault("skipped", {})[self.active_run] = event["skipped"]
                if self.history_table.exists(self.active_run):
                    values = list(self.history_table.item(self.active_run, "values"))
                    values[2:] = [event["completed"], event["total"], event["successes"], event["failures"]]
                    self.history_table.item(self.active_run, values=values)
        if kind == "ready":
            self.status.set("측정 중 · 성공한 실행은 건너뛰고 나머지를 실행합니다.")
        elif kind == "progress":
            task = event["task"]
            self.current.set(f"{LABELS[task['implementation']]} · Boid {task['boid_count']:,} / Dummy {task['dummy_count']:,} "
                f"· 변경 {task['dummy_change_percent']}% · 반복 {task['repetition']} "
                f"· 현재 {event['current_seconds']:.0f}초 / 전체 {event['elapsed_seconds']:.0f}초")
        elif kind == "run_finished":
            self._append_log(self._attempt_line(event["record"]), persist=False)
        elif kind == "stopped":
            self.status.set("중단됨 · 선택 기록 이어서 실행으로 재개할 수 있습니다.")
            self.current.set("중단된 프로세스는 다음 실행에서 처음부터 재시도합니다.")
        elif kind == "finished":
            self.status.set("모든 측정이 완료되었습니다." if event["failures"] == 0 else "일부 측정이 실패했습니다. 기록을 선택해 재시도할 수 있습니다.")
            self.current.set("완료")
        elif kind == "warning":
            self._append_log("알림: " + event["message"])
        elif kind == "error":
            self.status.set("실행 오류 · 실행 기록을 확인하세요.")
            self.current.set(event["message"])
            self._append_log("오류: " + event["message"])
        elif kind == "worker_done":
            previous_status, previous_current = self.status.get(), self.current.get()
            self._set_running(False)
            self._refresh_history(self.active_run)
            if event["code"] == 1 and previous_status.startswith("실행 오류"):
                self.status.set(previous_status)
                self.current.set(previous_current)
        if kind in ("preparing", "stopped", "finished", "worker_done"):
            self._try_save()

    def _poll(self):
        try:
            while True:
                self.handle_event(self.events.get_nowait())
        except queue.Empty:
            pass
        if self.closing and not self.worker.running and not self.running:
            if self._try_save(show_error=True):
                self.root.destroy()
                return
            self.closing = False
        self.poll_id = self.root.after(100, self._poll)

    def _persist_project(self):
        state = self.project.state
        state["settings"] = {key: var.get() for key, var in self.values.items()}
        state["conditions"] = copy.deepcopy(self.conditions)
        state["generator"] = dict(boids=self.boid_list.get(), dummies=self.dummy_list.get(),
            percents=self.percent_list.get(), implementations=[key for key, var in self.implementations.items() if var.get()])
        selection = self.history_table.selection()
        state["view"].update(tab=self.notebook.index("current"), geometry=self.root.geometry(),
            selected_run=selection[0] if selection else None,
            condition_selection=list(self.table.selection()), condition_scroll=self.table.yview()[0])
        self.project.save()
        self.dirty = False
        self._update_title()

    def _try_save(self, show_error=False):
        try:
            self._persist_project()
            return True
        except (OSError, ValueError) as error:
            if show_error:
                self._show_error(error)
            else:
                self._append_log("프로젝트 저장 실패: " + str(error))
            return False

    def _restore_project(self):
        state = self.project.state
        self.loading = True
        try:
            for key, value in state["settings"].items():
                if key in self.values:
                    self.values[key].set(value)
            self.conditions = copy.deepcopy(state["conditions"])
            self._reset_generator()
            generator = state["generator"]
            for key, var in (("boids", self.boid_list), ("dummies", self.dummy_list), ("percents", self.percent_list)):
                if isinstance(generator.get(key), str):
                    var.set(generator[key])
            if isinstance(generator.get("implementations"), list):
                for key, var in self.implementations.items():
                    var.set(key in generator["implementations"])
            self._refresh_table()
            view = state["view"]
            geometry = view.get("geometry", "")
            if isinstance(geometry, str) and re.fullmatch(r"[0-9]+x[0-9]+[+-][0-9]+[+-][0-9]+", geometry):
                self.root.geometry(geometry)
            self.notebook.select(view.get("tab", 0) if view.get("tab") in (0, 1) else 0)
            selected = [item for item in view.get("condition_selection", []) if self.table.exists(item)]
            if selected:
                self.table.selection_set(selected)
            scroll = view.get("condition_scroll", 0)
            if isinstance(scroll, (int, float)) and 0 <= scroll <= 1:
                self.table.yview_moveto(scroll)
            self._refresh_history(view.get("selected_run"))
            if not self.records:
                self._append_log("\n".join(state["messages"].get("_project", [])), persist=False)
            self._update_title()
        finally:
            self.loading = False

    def open_result(self, excel):
        if self.result_folder is None:
            self._show_error("측정 결과 폴더가 아직 없습니다.")
            return
        path = self.result_folder / "results.xlsx" if excel else self.result_folder
        try:
            if not path.exists():
                raise ValueError("아직 결과 파일이 저장되지 않았습니다.")
            os.startfile(str(path))
        except (OSError, ValueError) as error:
            self._show_error(error)

    def close(self):
        if not self._try_save(show_error=True):
            return
        if self.running or self.worker.running:
            self.closing = True
            self.stop()
            self.status.set("측정을 중단하고 프로젝트를 저장한 뒤 창을 닫습니다…")
        else:
            self.root.after_cancel(self.poll_id)
            self.root.destroy()


class ProjectLauncher:
    def __init__(self, root):
        self.root = root
        self.window = None
        root.title("Benchmarker · 프로젝트 선택")
        root.geometry("580x300")
        root.minsize(500, 280)
        self.frame = ttk.Frame(root, padding=30)
        self.frame.pack(fill="both", expand=True)
        ttk.Label(self.frame, text="Boids 실험 프로젝트", font=("맑은 고딕", 17, "bold")).pack(anchor="w", pady=(0, 15))
        ttk.Label(self.frame, text="프로젝트 폴더에 실험 설정과 실행 기록을 함께 보관합니다.\n프로젝트를 생성하거나 기존 폴더를 선택하세요.").pack(anchor="w", pady=(0, 22))
        ttk.Button(self.frame, text="프로젝트 생성", command=lambda: self.choose(True)).pack(fill="x", pady=4)
        ttk.Button(self.frame, text="프로젝트 불러오기", command=lambda: self.choose(False)).pack(fill="x", pady=4)

    def choose(self, create):
        folder = filedialog.askdirectory(parent=self.root, title="새 프로젝트를 저장할 폴더" if create else "프로젝트 폴더 선택")
        if not folder:
            return
        try:
            project = BenchmarkProject.create(folder) if create else BenchmarkProject.open(folder)
            self.enter(project)
        except (OSError, ValueError, KeyError) as error:
            messagebox.showerror("프로젝트 열기 실패", str(error), parent=self.root)

    def enter(self, project):
        self.frame.destroy()
        self.window = BenchmarkerWindow(self.root, project)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", nargs="?", type=Path, help="Optional project directory")
    args = parser.parse_args()
    root = tk.Tk()
    launcher = ProjectLauncher(root)
    if args.project:
        try:
            launcher.enter(BenchmarkProject.open(args.project))
        except (OSError, ValueError, KeyError) as error:
            messagebox.showerror("프로젝트 열기 실패", str(error), parent=root)
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
