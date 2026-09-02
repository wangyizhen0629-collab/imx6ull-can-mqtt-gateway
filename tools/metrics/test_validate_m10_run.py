#!/usr/bin/env python3
"""M10场景门禁校验器的无硬件回归。"""

import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from typing import Any


VALIDATOR = Path(__file__).with_name("validate_m10_run.py")
FIELDS = [
    "sample_index", "epoch_s", "uptime_s", "pid", "starttime_ticks",
    "proc_ticks", "total_ticks", "vmrss_kb", "vmhwm_kb", "linux_state",
    "sample_status",
]


def base_summary(scenario: str) -> dict[str, Any]:
    rule = {
        "stress_500": (1800, 500),
        "stress_1000": (1800, 1000),
        "broker_interruptions": (6000, 111),
        "baseline_24h": (86400, 111),
    }[scenario]
    interruptions = []
    reconnects = 0
    if scenario == "broker_interruptions":
        interruptions = [
            {"cycle": cycle, "offline_seconds": 300,
             "reconnect_latency_ms": 100 + cycle}
            for cycle in range(1, 21)
        ]
        reconnects = 20
    frames_total = rule[0] * rule[1]
    frames_100 = frames_total * 100 // 111
    frames_101 = frames_total * 10 // 111
    frames_102 = frames_total - frames_100 - frames_101
    return {
        "schema": "gateway.m10.run.v1",
        "run_id": f"test-{scenario}",
        "scenario": scenario,
        "environment": "imx6ull-physical",
        "duration_seconds": rule[0],
        "target_rate_fps": rule[1],
        "can": {
            "frames_total": frames_total,
            "frames_by_id": {
                "0x100": frames_100,
                "0x101": frames_101,
                "0x102": frames_102,
            },
            "counter_gaps_by_id": {"0x100": 0, "0x101": 0, "0x102": 0},
            "error_frame_count": 0,
            "rx_error_delta": 0,
            "rx_dropped_delta": 0,
            "rx_over_errors_delta": 0,
        },
        "queue": {"drops": 0},
        "mqtt": {"raw_batches": 10, "unique_records": frames_total,
                 "missing_records": 0, "raw_duplicate_records": 2,
                 "effective_duplicate_records": 0, "reconnects": reconnects},
        "spool": {"max_pending_records": 100},
        "process": {"exit_count": 0},
        "broker_interruptions": interruptions,
    }


class M10ValidatorTest(unittest.TestCase):
    def write_metrics(self, path: Path, duration: int = 2,
                      status: str = "OK") -> None:
        with path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=FIELDS)
            writer.writeheader()
            for index in range(1, duration + 2):
                writer.writerow({
                    "sample_index": index, "epoch_s": 1000 + index,
                    "uptime_s": 2000 + index, "pid": 42,
                    "starttime_ticks": 100, "proc_ticks": index * 10,
                    "total_ticks": index * 100, "vmrss_kb": 200 + index,
                    "vmhwm_kb": 300, "linux_state": "S",
                    "sample_status": status,
                })

    def run_validator(self, summary: dict[str, Any], status: str = "OK",
                      metrics_duration: int = 2) -> subprocess.CompletedProcess[str]:
        directory = tempfile.TemporaryDirectory(prefix="m10-validator-")
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        run_path = root / "run.json"
        metrics_path = root / "proc.csv"
        run_path.write_text(json.dumps(summary), encoding="utf-8")
        self.write_metrics(metrics_path, duration=metrics_duration, status=status)
        return subprocess.run(
            [sys.executable, "-B", str(VALIDATOR), "--run-summary", str(run_path),
             "--proc-metrics", str(metrics_path), "--output-json",
             str(root / "result.json"), "--report-md", str(root / "report.md")],
            check=False, capture_output=True, text=True,
        )

    def test_all_four_scenario_contracts_pass(self) -> None:
        for scenario in ("stress_500", "stress_1000",
                         "broker_interruptions", "baseline_24h"):
            with self.subTest(scenario=scenario):
                summary = base_summary(scenario)
                result = self.run_validator(
                    summary, metrics_duration=summary["duration_seconds"]
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(json.loads(result.stdout)["status"], "PASS")

    def test_host_environment_cannot_claim_gate(self) -> None:
        summary = base_summary("stress_500")
        summary["environment"] = "ubuntu-host"
        result = self.run_validator(summary)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("imx6ull-physical", result.stderr)

    def test_short_duration_is_rejected(self) -> None:
        summary = base_summary("baseline_24h")
        summary["duration_seconds"] = 86399
        result = self.run_validator(summary)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("86400", result.stderr)

    def test_interruption_count_is_exact(self) -> None:
        summary = base_summary("broker_interruptions")
        summary["broker_interruptions"].pop()
        result = self.run_validator(summary)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("exactly 20", result.stderr)

    def test_drop_is_rejected(self) -> None:
        summary = base_summary("stress_1000")
        summary["queue"]["drops"] = 1
        result = self.run_validator(summary)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("queue drops", result.stderr)

    def test_rate_claim_requires_enough_can_frames(self) -> None:
        summary = base_summary("stress_500")
        summary["can"]["frames_total"] -= 1
        summary["can"]["frames_by_id"]["0x100"] -= 1
        summary["mqtt"]["unique_records"] -= 1
        result = self.run_validator(summary)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("target_rate_fps", result.stderr)

    def test_per_id_counts_must_reconcile(self) -> None:
        summary = base_summary("stress_500")
        summary["can"]["frames_by_id"]["0x100"] -= 1
        result = self.run_validator(summary)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("per-ID", result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
