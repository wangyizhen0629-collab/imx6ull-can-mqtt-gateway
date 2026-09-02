#!/usr/bin/env python3
"""M10 /proc采集与报告工具的无网络回归。"""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


DIRECTORY = Path(__file__).resolve().parent
COLLECTOR = DIRECTORY / "collect_proc_metrics.sh"
REPORTER = DIRECTORY / "report_proc_metrics.py"
BUSYBOX = shutil.which("busybox")


class ProcMetricsTest(unittest.TestCase):
    def test_collect_current_process_and_report(self) -> None:
        self.assertIsNotNone(BUSYBOX, "BusyBox is required for the M10 collector test")
        with tempfile.TemporaryDirectory(prefix="m10-proc-") as directory:
            root = Path(directory)
            csv_path = root / "proc.csv"
            pid_path = root / "gatewayd.pid"
            pid_path.write_text(f"{os.getpid()}\n", encoding="ascii")
            expected_exe = str(Path(f"/proc/{os.getpid()}/exe").resolve())
            collect = subprocess.run(
                [BUSYBOX, "ash", str(COLLECTOR), "--output", str(csv_path),
                 "--pid-file", str(pid_path), "--expected-exe", expected_exe,
                 "--samples", "2", "--interval-sec", "1"],
                check=False, capture_output=True, text=True, timeout=5,
            )
            self.assertEqual(collect.returncode, 0, collect.stderr)
            summary_path = root / "summary.json"
            report_path = root / "report.md"
            report = subprocess.run(
                [sys.executable, "-B", str(REPORTER), "--input", str(csv_path),
                 "--summary-json", str(summary_path), "--report-md",
                 str(report_path)],
                check=False, capture_output=True, text=True,
            )
            self.assertEqual(report.returncode, 0, report.stderr)
            summary = json.loads(summary_path.read_text(encoding="utf-8"))
            self.assertEqual(summary["status"], "PASS")
            self.assertEqual(summary["ok_samples"], 2)
            self.assertEqual(summary["missing_samples"], 0)
            self.assertEqual(summary["pid_changes"], 0)
            self.assertEqual(summary["cpu_percent_total_capacity"]["valid_intervals"], 1)
            self.assertTrue(report_path.read_text(encoding="utf-8").startswith("# /proc指标报告"))

    def test_collector_refuses_existing_output(self) -> None:
        self.assertIsNotNone(BUSYBOX, "BusyBox is required for the M10 collector test")
        with tempfile.TemporaryDirectory(prefix="m10-proc-existing-") as directory:
            output = Path(directory) / "proc.csv"
            output.write_text("keep\n", encoding="utf-8")
            result = subprocess.run(
                [BUSYBOX, "ash", str(COLLECTOR), "--output", str(output),
                 "--pid", str(os.getpid()), "--samples", "1"],
                check=False, capture_output=True, text=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(output.read_text(encoding="utf-8"), "keep\n")
            self.assertIn("refusing to overwrite", result.stderr)

    def test_missing_pid_is_visible(self) -> None:
        self.assertIsNotNone(BUSYBOX, "BusyBox is required for the M10 collector test")
        with tempfile.TemporaryDirectory(prefix="m10-proc-missing-") as directory:
            output = Path(directory) / "proc.csv"
            result = subprocess.run(
                [BUSYBOX, "ash", str(COLLECTOR), "--output", str(output),
                 "--pid", "99999999", "--samples", "1"],
                check=False, capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn(",MISSING\n", output.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
