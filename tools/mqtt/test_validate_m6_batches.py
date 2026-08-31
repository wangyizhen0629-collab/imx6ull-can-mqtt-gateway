#!/usr/bin/env python3
"""M6 batch validator 的无网络回归测试。"""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from typing import Any


VALIDATOR = Path(__file__).with_name("validate_m6_batches.py")
DEVICE_ID = "m6-validator-regression"


def make_record(sequence: Any, *, dlc: Any = 8) -> dict[str, Any]:
    return {
        "seq": sequence,
        "dlc": dlc,
        "data": "00" * 8,
        "decoded_payload": "00" * 32,
    }


def make_message(batch_seq: Any, sequences: list[Any]) -> dict[str, Any]:
    return {
        "schema": "gateway.telemetry.v1",
        "device_id": DEVICE_ID,
        "batch_seq": batch_seq,
        "record_count": len(sequences),
        "first_seq": sequences[0],
        "last_seq": sequences[-1],
        "records": [make_record(sequence) for sequence in sequences],
    }


class ValidatorRegressionTest(unittest.TestCase):
    def run_validator(self, messages: list[dict[str, Any]],
                      expected_batches: int) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory(prefix="m6-validator-") as directory:
            input_path = Path(directory) / "subscriber.jsonl"
            payload = "\n".join(json.dumps(message) for message in messages) + "\n"
            input_path.write_text(payload, encoding="utf-8")
            return subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(VALIDATOR),
                    "--input",
                    str(input_path),
                    "--device-id",
                    DEVICE_ID,
                    "--expected-batches",
                    str(expected_batches),
                ],
                check=False,
                capture_output=True,
                text=True,
            )

    def assert_rejected(self, messages: list[dict[str, Any]],
                        expected_error: str) -> None:
        result = self.run_validator(messages, len(messages))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(expected_error, result.stderr)

    def test_two_multirecord_batches_pass(self) -> None:
        result = self.run_validator(
            [make_message(1, [1, 2]), make_message(2, [3, 4, 5])], 2
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)
        self.assertEqual(summary["raw_batches"], 2)
        self.assertEqual(summary["unique_gateway_seq"], 5)
        self.assertEqual(summary["last_gateway_seq"], 5)

    def test_cross_batch_gap_is_rejected(self) -> None:
        self.assert_rejected(
            [make_message(1, [1, 2]), make_message(2, [4, 5])],
            "gateway seq has missing/duplicate/reordered values",
        )

    def test_cross_batch_duplicate_is_rejected(self) -> None:
        self.assert_rejected(
            [make_message(1, [1, 2]), make_message(2, [2, 3])],
            "gateway seq has missing/duplicate/reordered values",
        )

    def test_boolean_batch_seq_is_rejected(self) -> None:
        self.assert_rejected([make_message(True, [1])], "invalid batch_seq")

    def test_boolean_record_seq_is_rejected(self) -> None:
        message = make_message(1, [1])
        message["records"][0]["seq"] = True
        self.assert_rejected([message], "invalid record seq")

    def test_boolean_record_count_is_rejected(self) -> None:
        message = make_message(1, [1])
        message["record_count"] = True
        self.assert_rejected([message], "invalid record_count")

    def test_boolean_first_and_last_seq_are_rejected(self) -> None:
        first_message = make_message(1, [1])
        first_message["first_seq"] = True
        self.assert_rejected([first_message], "invalid first_seq")

        last_message = make_message(1, [1])
        last_message["last_seq"] = True
        self.assert_rejected([last_message], "invalid last_seq")

    def test_non_integer_dlc_is_rejected(self) -> None:
        message = make_message(1, [1])
        message["records"][0]["dlc"] = 8.0
        self.assert_rejected([message], "DLC mismatch")


if __name__ == "__main__":
    unittest.main(verbosity=2)
