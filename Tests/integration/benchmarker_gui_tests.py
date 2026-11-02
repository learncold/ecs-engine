"""Settings round trips and real Tk controller lifecycle checks."""

import copy
import json
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest
import uuid
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
try:
    import tkinter as tk
    import benchmarker_gui as gui
except ImportError:
    tk = gui = None


@unittest.skipUnless(gui, "GUI requires the optional Python tkinter module")
class SettingsTests(unittest.TestCase):
    def test_combinations_include_ratios_and_do_not_repeat_zero_dummy_conditions(self):
        rows = gui.generate_conditions("512,1024,512", "0,B,4B", "0,10,100", gui.IMPLEMENTATIONS)
        self.assertEqual(len(rows), 42)
        self.assertEqual(len({tuple(row.values()) for row in rows}), 42)
        self.assertTrue(any(row["boid_count"] == 1024 and row["dummy_count"] == 4096 for row in rows))
        self.assertTrue(all(row["dummy_change_percent"] == 0 for row in rows if row["dummy_count"] == 0))

    def test_invalid_combinations_do_not_silently_change_input(self):
        for args in (("", "0", "0", ["oop"]), ("1.5", "B", "10", ["oop"]),
                     ("100", "-1", "0", ["oop"]), ("100", "B", "101", ["oop"]),
                     ("100", "B", "0", []), (str(2**32), "0", "0", ["oop"]),
                     ("100", "foo", "0", ["oop"])):
            with self.subTest(args=args), self.assertRaises(ValueError):
                gui.generate_conditions(*args)

    def test_save_to_other_directory_preserves_resolved_paths_and_all_settings(self):
        original, _ = gui.batch.load_config(ROOT / "benchmarks/smoke.json")
        original.update(order_seed=2026, timeout_seconds=90)
        before = copy.deepcopy(original)
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp) / "측정 조건 with spaces"
            folder.mkdir()
            path = folder / "experiment.json"
            gui.save_settings(path, original)
            restored, _ = gui.batch.load_config(path)
            self.assertEqual(restored, original)
            self.assertEqual(original, before)
            bad = copy.deepcopy(original)
            bad["measurement_steps"] = 0
            with self.assertRaises(ValueError):
                gui.save_settings(path, bad)
            self.assertEqual(gui.batch.load_config(path)[0], original)

    def test_form_optional_fields_and_numeric_validation(self):
        config, _ = gui.batch.load_config(ROOT / "benchmarks/smoke.json")
        values = {key: str(config.get(key, "")) for key in ("executable", "output_directory") + tuple(k for k, _ in gui.SETTINGS)}
        restored = gui.config_from_form(values, config["conditions"])
        self.assertEqual(restored, config)
        for key, value in (("measurement_steps", "0"), ("dt", "nan"), ("dt", "bad"), ("repetitions", "1.5"), ("executable", "")):
            changed = dict(values, **{key: value})
            with self.subTest(key=key), self.assertRaises(ValueError):
                gui.config_from_form(changed, config["conditions"])



@unittest.skipUnless(gui, "GUI requires tkinter")
class ProjectTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="benchmark project ")
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name) / "내 프로젝트"
        self.project = gui.BenchmarkProject.create(self.folder)

    def test_create_open_and_existing_project_is_never_overwritten(self):
        self.assertTrue((self.folder / "project.json").exists())
        self.assertTrue((self.folder / "Result").is_dir())
        before = self.project.path.read_bytes()
        with self.assertRaises(ValueError):
            gui.BenchmarkProject.create(self.folder)
        self.assertEqual(self.project.path.read_bytes(), before)
        self.assertEqual(gui.BenchmarkProject.open(self.folder).state, self.project.state)

    def test_invalid_numeric_draft_is_restored_but_result_paths_cannot_escape_project(self):
        self.project.state["settings"]["dt"] = "unfinished input"
        self.project.save()
        self.assertEqual(gui.BenchmarkProject.open(self.folder).state["settings"]["dt"], "unfinished input")
        for name in ("..", "../outside", str(self.folder.resolve()), ""):
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.project.run_folder(name)

    def test_history_reads_partial_journal_without_changing_it(self):
        config, _ = gui.batch.load_config(ROOT / "benchmarks/smoke.json")
        config["output_directory"] = str(self.project.results)
        folder = self.project.results / "first-run"
        folder.mkdir()
        gui.batch.write_json(folder / "manifest.json", dict(resume_version=1, resolved_config=config))
        journal = folder / "attempts.jsonl"
        journal.write_bytes(b'{"order":1,"status":"success"}\n{"order":')
        before = journal.read_bytes()
        record = self.project.history()[0]
        self.assertEqual(record["successes"], 1)
        self.assertEqual(record["status"], "중단됨")
        self.assertEqual(journal.read_bytes(), before)
        broken = self.project.results / "broken-run"
        broken.mkdir()
        (broken / "manifest.json").write_text("truncated")
        self.assertEqual(len(self.project.history()), 2)
        self.assertTrue(any(row["error"] for row in self.project.history()))


@unittest.skipUnless(gui, "GUI requires tkinter")
class WindowTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="benchmarker GUI ")
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.project = gui.BenchmarkProject.create(self.folder / "project")
        self.root = tk.Tk()
        self.root.withdraw()
        self.window = gui.BenchmarkerWindow(self.root, self.project)
        self.window.load_path(ROOT / "benchmarks/smoke.json")

    def tearDown(self):
        if self.window.worker.running:
            self.window.worker.stop()
            self.window.worker.thread.join(timeout=10)
        try:
            if self.root.winfo_exists():
                self.window.close()
        except tk.TclError:
            pass

    def reopen(self):
        self.window.close()
        self.root = tk.Tk()
        self.root.withdraw()
        self.window = gui.BenchmarkerWindow(self.root, gui.BenchmarkProject.open(self.project.directory))

    def pump_until(self, predicate, timeout=10):
        deadline = time.monotonic() + timeout
        while not predicate() and time.monotonic() < deadline:
            self.root.update()
            time.sleep(0.01)
        self.assertTrue(predicate(), "GUI did not reach expected state")

    def fake_record(self, name, seed=42):
        config = self.window.form_config()
        config["seed"] = seed
        folder = self.project.results / name
        folder.mkdir()
        gui.batch.write_json(folder / "manifest.json", dict(resume_version=1, resolved_config=config))
        return folder, config

    def test_exactly_two_tabs_and_imported_output_is_project_result(self):
        labels = [self.window.notebook.tab(tab, "text") for tab in self.window.notebook.tabs()]
        self.assertEqual(labels, ["실험 설정", "실행"])
        self.assertNotIn("output_directory", self.window.values)
        self.assertEqual(self.window.form_config()["output_directory"], str(self.project.results))

    def test_project_close_restores_editor_generator_tab_and_invalid_draft(self):
        self.window.values["dt"].set("not finished")
        self.window.boid_list.set("300,600")
        self.window.dummy_list.set("4B")
        self.window.percent_list.set("25")
        self.window.implementations["oop"].set(False)
        self.window.notebook.select(1)
        self.reopen()
        self.assertEqual(self.window.values["dt"].get(), "not finished")
        self.assertEqual(self.window.boid_list.get(), "300,600")
        self.assertEqual(self.window.dummy_list.get(), "4B")
        self.assertEqual(self.window.percent_list.get(), "25")
        self.assertFalse(self.window.implementations["oop"].get())
        self.assertEqual(self.window.notebook.index("current"), 1)
        self.assertEqual(len(self.window.conditions), 6)
        self.assertFalse((self.folder / "session.json").exists())

    def test_import_export_settings_and_cancel_keep_edits(self):
        path = self.folder / "내 측정 조건.json"
        self.window.values["measurement_steps"].set("321")
        with patch.object(gui.filedialog, "asksaveasfilename", return_value=str(path)):
            self.assertTrue(self.window.save_dialog())
        self.assertEqual(gui.batch.load_config(path)[0]["measurement_steps"], 321)
        self.window.values["measurement_steps"].set("999")
        with patch.object(gui.messagebox, "askyesnocancel", return_value=None), \
                patch.object(gui.filedialog, "askopenfilename") as dialog:
            self.window.load_dialog()
            dialog.assert_not_called()
        with patch.object(gui.messagebox, "askyesnocancel", return_value=False), \
                patch.object(gui.filedialog, "askopenfilename", return_value=str(path)):
            self.window.load_dialog()
        self.assertEqual(self.window.form_config()["measurement_steps"], 321)

    def test_import_clears_generator_and_preview_counts_only_new_conditions(self):
        self.assertEqual(self.window.boid_list.get(), "")
        self.assertIn("disabled", self.window.add_button.state())
        original = self.window.form_config()
        self.window.boid_list.set("16")
        self.window.dummy_list.set("12")
        self.window.percent_list.set("0,40")
        self.assertIn("새 조건 0개", self.window.generator_preview.get())
        self.assertIn("중복 6개", self.window.generator_preview.get())
        self.window.percent_list.set("0,40,100")
        self.assertIn("새 조건 3개", self.window.generator_preview.get())
        self.assertEqual(self.window.form_config(), original)
        self.window.add_button.invoke()
        self.assertEqual(len(self.window.conditions), 9)
        self.window.load_path(ROOT / "benchmarks/smoke.json")
        self.assertEqual(self.window.boid_list.get(), "")
        self.assertEqual(len(self.window.conditions), 6)

    def test_combinations_edit_delete_and_total_count(self):
        self.window.clear_conditions()
        self.window.boid_list.set("1024,2048")
        self.window.dummy_list.set("0,B")
        self.window.percent_list.set("0,10")
        self.window.add_combinations()
        self.window.add_combinations()
        self.assertEqual(len(self.window.conditions), 18)
        self.window.table.selection_set("0", "1")
        self.window.delete_selected()
        self.assertEqual(len(self.window.conditions), 16)
        self.window.table.selection_set("0")
        self.window.edit_selected()
        dialog = next(w for w in self.root.winfo_children() if isinstance(w, tk.Toplevel))
        entries = [w for w in dialog.winfo_children() if isinstance(w, gui.ttk.Entry) and not isinstance(w, gui.ttk.Combobox)]
        entries[0].delete(0, "end")
        entries[0].insert(0, "333")
        next(w for w in dialog.winfo_children() if isinstance(w, gui.ttk.Button)).invoke()
        self.assertEqual(self.window.conditions[0]["boid_count"], 333)

    def test_invalid_settings_prevent_new_launch_but_can_be_saved(self):
        self.window.values["dt"].set("invalid")
        with patch.object(gui.messagebox, "showerror") as error, patch.object(self.window.worker, "start") as launch:
            self.window.start()
            launch.assert_not_called()
            error.assert_called_once()
        self.assertFalse(self.window.running)
        self.assertTrue(self.window.save_project())

    def test_selected_run_resume_uses_its_snapshot_without_changing_editor(self):
        folder, config = self.fake_record("previous-run")
        self.window._refresh_history(folder.name)
        self.window.values["seed"].set("123")
        self.window.values["dt"].set("invalid draft")
        with patch.object(self.window.worker, "start") as start:
            self.window.start(fresh=False)
            args, kwargs = start.call_args
            self.assertEqual(args[0], config)
            self.assertEqual(kwargs["resume"], folder)
            self.assertFalse(kwargs["fresh"])
        self.window._set_running(False)
        self.assertEqual(self.window.values["seed"].get(), "123")
        self.assertEqual(self.window.values["dt"].get(), "invalid draft")

    def test_history_selection_messages_and_progress_are_restored_per_project(self):
        first, _ = self.fake_record("first-run")
        second, _ = self.fake_record("second-run", 123)
        (first / "attempts.jsonl").write_text('{"order":1,"status":"success","implementation":"oop","boid_count":16,"dummy_count":12,"dummy_change_percent":0,"repetition":1,"attempt":1,"error":""}\n')
        self.window._refresh_history(first.name)
        self.window._append_log("a warning")
        self.window.notebook.select(1)
        self.reopen()
        self.assertEqual(len(self.window.records), 2)
        self.assertEqual(self.window.history_table.selection(), (first.name,))
        self.assertIn("1 / 12", self.window.progress_text.get())
        self.assertIn("a warning", self.window.log.get("1.0", "end"))
        self.assertEqual(self.window.form_config()["seed"], 42)

    def test_project_save_failure_keeps_window_open(self):
        with patch.object(self.project, "save", side_effect=PermissionError("read only")), \
                patch.object(gui.messagebox, "showerror") as error:
            self.window.close()
            error.assert_called_once()
        self.assertTrue(self.root.winfo_exists())

    def test_launcher_has_no_main_screen_until_project_selected(self):
        self.window.close()
        self.root = tk.Tk()
        self.root.withdraw()
        launcher = gui.ProjectLauncher(self.root)
        self.assertIsNone(launcher.window)
        with patch.object(gui.filedialog, "askdirectory", return_value=str(self.project.directory)):
            launcher.choose(False)
        self.window = launcher.window
        self.assertIsNotNone(self.window)
        self.assertEqual(self.window.project.directory, self.project.directory)


def integration(app, output):
    """Project close during measurement, reopen, resume and independent histories."""
    output = output / uuid.uuid4().hex
    project = gui.BenchmarkProject.create(output)
    config, _ = gui.batch.load_config(ROOT / "benchmarks/smoke.json")
    config.update(executable=str(app.resolve()), output_directory=str(project.results),
                  repetitions=1, warmup_steps=0, measurement_steps=100,
                  conditions=[dict(implementation="archetype", boid_count=0, dummy_count=10, dummy_change_percent=0),
                              dict(implementation="oop", boid_count=2048, dummy_count=10, dummy_change_percent=0)])
    root = tk.Tk()
    root.withdraw()
    window = gui.BenchmarkerWindow(root, project)
    window.apply_config(config)
    original_handler = window.handle_event

    def handle(event):
        original_handler(event)
        if event["event"] == "progress" and event["task"]["order"] == 2:
            window.values["seed"].set("99")
            window.boid_list.set("777")
            window.close()
    window.handle_event = handle

    def pump(predicate, timeout=60):
        deadline = time.monotonic() + timeout
        while not predicate() and time.monotonic() < deadline:
            root.update()
            time.sleep(0.01)
        assert predicate(), "GUI integration timeout"

    try:
        window.start()
        pump(lambda: not window.running and not window.worker.running)
        folder = window.result_folder
        attempts = gui.batch.read_attempts(folder / "attempts.jsonl")
        assert [row["status"] for row in attempts] == ["success", "interrupted"]
        first_csv = Path(attempts[0]["csv_path"])
        saved_csv = first_csv.read_bytes()
        assert (folder / "results.xlsx").exists()
        root = tk.Tk()
        root.withdraw()
        window = gui.BenchmarkerWindow(root, gui.BenchmarkProject.open(output))
        assert window.form_config()["seed"] == 99
        assert window.boid_list.get() == "777"
        assert window.notebook.index("current") == 1
        assert window.history_table.selection() == (folder.name,)
        window.start(fresh=False)
        pump(lambda: not window.running)
        attempts = gui.batch.read_attempts(folder / "attempts.jsonl")
        assert [row["status"] for row in attempts] == ["success", "interrupted", "success"]
        assert attempts[-1]["order"] == 2 and attempts[-1]["attempt"] == 2
        assert attempts[-1]["seed"] == 42, "Resume used edited settings instead of its original snapshot"
        assert window.form_config()["seed"] == 99
        assert first_csv.read_bytes() == saved_csv
        assert "건너뜀 1" in window.progress_text.get()
        assert json.loads((folder / "summary.json").read_text())["successes"] == 2
        window.start()
        pump(lambda: not window.running)
        assert len(window.records) == 2
        assert window.result_folder != folder
        assert first_csv.read_bytes() == saved_csv
        new_folder = window.result_folder
        window.close()
        root = tk.Tk()
        root.withdraw()
        window = gui.BenchmarkerWindow(root, gui.BenchmarkProject.open(output))
        assert len(window.records) == 2
        assert window.result_folder == new_folder
        window._refresh_history(folder.name)
        journal = (folder / "attempts.jsonl").read_bytes()
        window.start(fresh=False)
        pump(lambda: not window.running)
        assert (folder / "attempts.jsonl").read_bytes() == journal
        assert len(list(project.results.iterdir())) == 2
        print("Project GUI acceptance: close/reopen/resume, preserved drafts, independent histories verified")
    finally:
        if window.worker.running:
            window.worker.stop()
            window.worker.thread.join(timeout=10)
        try:
            if root.winfo_exists():
                window.close()
        except tk.TclError:
            pass


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--integration":
        if gui is None:
            print("GUI acceptance skipped: tkinter unavailable")
        else:
            integration(Path(sys.argv[2]), Path(sys.argv[3]))
    else:
        unittest.main()
