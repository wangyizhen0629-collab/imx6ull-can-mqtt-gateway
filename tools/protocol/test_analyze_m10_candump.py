#!/usr/bin/env python3
"""M10 candump分析器和STM32 profile合同的无硬件回归。"""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parents[1]
ANALYZER = TOOLS_DIR / "analyze_m10_candump.py"
LEGACY_ANALYZER = TOOLS_DIR / "check_stm32_candump.py"
PROFILE_SPEC = REPO_ROOT / "protocol" / "m10_traffic_profiles.json"
KEIL_PROJECT = (
    REPO_ROOT / "stm32" / "firmware" / "imx6ull-can-mqtt-gateway"
    / "MDK-ARM" / "imx6ull-can-mqtt-gateway.uvprojx"
)
PROFILE_HEADER = (
    REPO_ROOT / "stm32" / "firmware" / "imx6ull-can-mqtt-gateway"
    / "Core" / "Inc" / "ecu_traffic_profile.h"
)
TARGET_GENERATOR = (
    REPO_ROOT / "tools" / "stm32" / "generate_m10_keil_targets.py"
)

sys.path.insert(0, str(TOOLS_DIR))
import check_stm32_candump as legacy  # noqa: E402


def payload(counter: int) -> bytes:
    data = bytearray(8)
    data[6] = counter & 0xFF
    data[7] = data[6]
    return bytes(data)


def candump_line(timestamp: float, can_id: int, data: bytes) -> str:
    return f"({timestamp:.6f}) can0 {can_id:03X}#{data.hex().upper()}"


def stress_frames(profile: int, superframes: int = 20) -> list[str]:
    slot_seconds = {500: 0.002, 1000: 0.001}[profile]
    counters = {0x100: 0, 0x101: 0, 0x102: 0}
    lines = []
    for absolute_slot in range(superframes * 100):
        slot = absolute_slot % 100
        if slot == 99:
            can_id = 0x102
        elif (slot + 1) % 10 == 0:
            can_id = 0x101
        else:
            can_id = 0x100
        lines.append(candump_line(
            absolute_slot * slot_seconds, can_id, payload(counters[can_id])
        ))
        counters[can_id] = (counters[can_id] + 1) & 0xFF
    return lines


def renumber_counters(lines: list[str]) -> list[str]:
    counters = {0x100: 0, 0x101: 0, 0x102: 0}
    result = []
    for line in lines:
        prefix, encoded = line.split("#", 1)
        can_id = int(prefix.rsplit(" ", 1)[1], 16)
        data = bytearray.fromhex(encoded)
        data[6] = counters[can_id]
        data[7] = data[6]
        counters[can_id] = (counters[can_id] + 1) & 0xFF
        result.append(prefix + "#" + data.hex().upper())
    return result


def baseline_frames(duration_seconds: int = 20) -> list[str]:
    counters = {0x100: 0, 0x101: 0, 0x102: 0}
    events = []
    for millisecond in range(1, duration_seconds * 1000 + 1):
        for can_id, period in ((0x100, 10), (0x101, 100), (0x102, 1000)):
            if millisecond % period == 0:
                events.append(candump_line(
                    millisecond / 1000.0, can_id, payload(counters[can_id])
                ))
                counters[can_id] = (counters[can_id] + 1) & 0xFF
    return events


def can_snapshot(packets: int, bytes_count: int) -> str:
    return "\n".join((
        "can state ERROR-ACTIVE (berr-counter tx 0 rx 0)",
        "bitrate 500000 sample-point 0.866",
        "re-started bus-errors arbit-lost error-warn error-pass bus-off",
        "0 0 0 0 0 0",
        "RX: bytes packets errors dropped overrun mcast",
        f"{bytes_count} {packets} 0 0 0 0",
        "TX: bytes packets errors dropped carrier collsns",
        "0 0 0 0 0 0",
        "",
    ))


class M10CandumpAnalyzerTest(unittest.TestCase):
    def setUp(self) -> None:
        self.directory = tempfile.TemporaryDirectory(prefix="m10-candump-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def run_analyzer(
        self, profile: int, lines: list[str], minimum_duration: float
    ) -> subprocess.CompletedProcess[str]:
        candump = self.root / f"candump-{profile}-{len(list(self.root.iterdir()))}.log"
        before = self.root / f"before-{profile}-{len(list(self.root.iterdir()))}.txt"
        after = self.root / f"after-{profile}-{len(list(self.root.iterdir()))}.txt"
        output = self.root / f"summary-{profile}-{len(list(self.root.iterdir()))}.json"
        candump.write_text("\n".join(lines) + "\n", encoding="utf-8")
        before.write_text(can_snapshot(0, 0), encoding="utf-8")
        after.write_text(
            can_snapshot(len(lines), len(lines) * 8), encoding="utf-8"
        )
        return subprocess.run(
            [
                sys.executable, "-B", str(ANALYZER),
                "--candump", str(candump),
                "--can-before", str(before),
                "--can-after", str(after),
                "--profile", str(profile),
                "--profile-spec", str(PROFILE_SPEC),
                "--minimum-duration-seconds", str(minimum_duration),
                "--summary-json", str(output),
            ],
            check=False, capture_output=True, text=True,
        )

    def test_all_profiles_pass(self) -> None:
        cases = (
            (111, baseline_frames(), 19.0),
            (500, stress_frames(500), 3.0),
            (1000, stress_frames(1000), 1.5),
        )
        for profile, lines, minimum_duration in cases:
            with self.subTest(profile=profile):
                result = self.run_analyzer(profile, lines, minimum_duration)
                self.assertEqual(result.returncode, 0, result.stderr)
                summary = json.loads(result.stdout)
                self.assertEqual(summary["status"], "PASS")
                self.assertEqual(summary["target_rate_fps"], profile)

    def test_counter_gap_is_rejected(self) -> None:
        lines = stress_frames(500)
        fields = lines[50].split("#")
        data = bytearray.fromhex(fields[1])
        data[6] = (data[6] + 1) & 0xFF
        data[7] = data[6]
        lines[50] = fields[0] + "#" + data.hex().upper()
        result = self.run_analyzer(500, lines, 3.0)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("counter gap", result.stderr)

    def test_checksum_error_is_rejected(self) -> None:
        lines = stress_frames(1000)
        lines[25] = lines[25][:-2] + "FF"
        result = self.run_analyzer(1000, lines, 1.5)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("XOR mismatch", result.stderr)

    def test_missing_schedule_slot_is_rejected(self) -> None:
        lines = stress_frames(1000)
        del lines[98]
        lines = renumber_counters(lines)
        result = self.run_analyzer(1000, lines, 1.5)
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(
            "gross interval" in result.stderr or
            "100-slot pattern" in result.stderr,
            result.stderr,
        )

    def test_short_capture_is_rejected(self) -> None:
        result = self.run_analyzer(500, stress_frames(500), 5.0)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("observed duration", result.stderr)

    def test_profile_source_and_keil_targets_are_consistent(self) -> None:
        profiles = json.loads(PROFILE_SPEC.read_text(encoding="utf-8"))["profiles"]
        self.assertEqual(
            [profiles[key]["target_rate_fps"] for key in ("111", "500", "1000")],
            [111, 500, 1000],
        )
        self.assertEqual(
            profiles["500"]["frames_per_superframe"],
            {"0x100": 90, "0x101": 9, "0x102": 1},
        )
        project = KEIL_PROJECT.read_text(encoding="utf-8")
        header = PROFILE_HEADER.read_text(encoding="utf-8")
        for profile in ("111", "500", "1000"):
            self.assertEqual(project.count(f"<TargetName>M10_{profile}</TargetName>"), 1)
            self.assertEqual(project.count(f"ECU_TRAFFIC_PROFILE={profile}</Define>"), 1)
            self.assertIn(f"ECU_TRAFFIC_PROFILE_{profile}", header)
        check = subprocess.run(
            [sys.executable, "-B", str(TARGET_GENERATOR),
             "--project", str(KEIL_PROJECT), "--check"],
            check=False, capture_output=True, text=True,
        )
        self.assertEqual(check.returncode, 0, check.stderr)

    def test_legacy_m4_contract_still_passes(self) -> None:
        states = legacy.build_expected_states()
        events = []
        for index in range(6000):
            events.append((
                (index + 1) * 0.01, 0x100,
                legacy.vehicle_payload(states[index], index & 0xFF),
            ))
        for index in range(600):
            phase = index * 10 + 9
            events.append((
                (index + 1) * 0.1, 0x101,
                legacy.power_payload(states[phase], index & 0xFF),
            ))
        for index in range(60):
            phase = index * 100 + 99
            events.append((
                (index + 1) * 1.0, 0x102,
                legacy.body_payload(states[phase], index & 0xFF),
            ))
        events.sort(key=lambda item: (item[0], item[1]))
        candump = self.root / "legacy.log"
        before = self.root / "legacy-before.txt"
        after = self.root / "legacy-after.txt"
        candump.write_text(
            "\n".join(
                f"({timestamp:.6f}) can0 {can_id:03X} [8] "
                + " ".join(f"{byte:02X}" for byte in data)
                for timestamp, can_id, data in events
            ) + "\n",
            encoding="utf-8",
        )
        before.write_text(can_snapshot(0, 0), encoding="utf-8")
        after.write_text(can_snapshot(6660, 6660 * 8), encoding="utf-8")
        result = subprocess.run(
            [sys.executable, "-B", str(LEGACY_ANALYZER),
             "--candump", str(candump), "--can-before", str(before),
             "--can-after", str(after)],
            check=False, capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("all 6660 physical frames", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
