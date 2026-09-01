#!/usr/bin/env python3
"""M7 恢复 validator 的无网络回归测试。"""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from typing import Any


VALIDATOR = Path(__file__).with_name("validate_m7_recovery.py")
DEVICE_ID = "m7-validator-regression"


def make_record(sequence: int, marker: int | None = None) -> dict[str, Any]:
    value = sequence if marker is None else marker
    return {
        "seq": sequence,
        "dlc": 8,
        "data": f"{value & 0xff:02x}" + "00" * 7,
        "decoded_payload": "00" * 32,
    }


def make_message(batch_seq: int, sequences: list[int]) -> dict[str, Any]:
    return {
        "schema": "gateway.telemetry.v1",
        "device_id": DEVICE_ID,
        "batch_seq": batch_seq,
        "record_count": len(sequences),
        "first_seq": sequences[0],
        "last_seq": sequences[-1],
        "records": [make_record(sequence) for sequence in sequences],
    }


class RecoveryValidatorTest(unittest.TestCase):
    def run_validator(
        self, messages: list[dict[str, Any]], *, require_duplicates: bool = False
    ) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory(prefix="m7-validator-") as directory:
            input_path = Path(directory) / "subscriber.jsonl"
            input_path.write_text(
                "\n".join(json.dumps(message) for message in messages) + "\n",
                encoding="utf-8",
            )
            command = [
                sys.executable,
                "-B",
                str(VALIDATOR),
                "--input",
                str(input_path),
                "--device-id",
                DEVICE_ID,
                "--expected-first-seq",
                "1",
                "--expected-last-seq",
                "6",
            ]
            if require_duplicates:
                command.append("--require-raw-duplicates")
            return subprocess.run(command, check=False, capture_output=True,
                                  text=True)

    def test_crash_replay_duplicate_is_deduplicated(self) -> None:
        messages = [
            make_message(1, [1, 2]),
            make_message(2, [3, 4]),
            make_message(2, [3, 4]),
            make_message(3, [5, 6]),
        ]
        result = self.run_validator(messages, require_duplicates=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)
        self.assertEqual(summary["raw_records"], 8)
        self.assertEqual(summary["raw_duplicate_records"], 2)
        self.assertEqual(summary["unique_gateway_seq"], 6)
        self.assertEqual(summary["effective_duplicate_records"], 0)

    def test_full_cursor_reset_replay_passes(self) -> None:
        messages = [make_message(1, [1, 2, 3]), make_message(2, [4, 5, 6]),
                    make_message(1, [1, 2, 3]), make_message(2, [4, 5, 6])]
        result = self.run_validator(messages, require_duplicates=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_missing_sequence_is_rejected(self) -> None:
        result = self.run_validator(
            [make_message(1, [1, 2]), make_message(2, [4, 5, 6])]
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing/reordered", result.stderr)

    def test_conflicting_duplicate_is_rejected(self) -> None:
        messages = [make_message(1, [1, 2, 3]), make_message(2, [4, 5, 6]),
                    make_message(3, [3])]
        messages[-1]["records"][0] = make_record(3, marker=99)
        result = self.run_validator(messages)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("conflicting duplicate", result.stderr)

    def test_duplicate_requirement_is_enforced(self) -> None:
        result = self.run_validator(
            [make_message(1, [1, 2, 3]), make_message(2, [4, 5, 6])],
            require_duplicates=True,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("raw duplicates were not observed", result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
