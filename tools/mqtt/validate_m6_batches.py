#!/usr/bin/env python3
"""校验 M6 mosquitto_sub 捕获的 JSON batch、batch_seq 和 gateway seq。"""

import argparse
import json
from pathlib import Path
from typing import Any


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--device-id", required=True)
    parser.add_argument("--expected-batches", required=True, type=int)
    return parser.parse_args()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def is_json_integer(value: Any) -> bool:
    # bool 是 int 的子类，但 JSON true/false 不能充当协议整数。
    return type(value) is int


def validate_message(message: Any, device_id: str) -> tuple[int, list[int]]:
    require(isinstance(message, dict), "payload is not a JSON object")
    require(message.get("schema") == "gateway.telemetry.v1", "schema mismatch")
    require(message.get("device_id") == device_id, "device_id mismatch")
    batch_seq = message.get("batch_seq")
    records = message.get("records")
    record_count = message.get("record_count")
    first_seq = message.get("first_seq")
    last_seq = message.get("last_seq")
    require(is_json_integer(batch_seq) and batch_seq > 0, "invalid batch_seq")
    require(isinstance(records, list) and records, "records must be non-empty")
    require(is_json_integer(record_count), "invalid record_count")
    require(record_count == len(records), "record_count mismatch")
    require(is_json_integer(first_seq), "invalid first_seq")
    require(is_json_integer(last_seq), "invalid last_seq")

    sequences: list[int] = []
    for record in records:
        require(isinstance(record, dict), "record is not an object")
        sequence = record.get("seq")
        dlc = record.get("dlc")
        require(is_json_integer(sequence) and sequence > 0, "invalid record seq")
        require(is_json_integer(dlc) and dlc == 8, "DLC mismatch")
        require(isinstance(record.get("data"), str) and len(record["data"]) == 16,
                "CAN data hex length mismatch")
        require(isinstance(record.get("decoded_payload"), str)
                and len(record["decoded_payload"]) == 64,
                "decoded payload hex length mismatch")
        sequences.append(sequence)
    require(all(left < right for left, right in zip(sequences, sequences[1:])),
            "record seq is not strictly increasing inside batch")
    require(first_seq == sequences[0], "first_seq mismatch")
    require(last_seq == sequences[-1], "last_seq mismatch")
    return batch_seq, sequences


def main() -> int:
    args = parse_args()
    require(args.expected_batches > 0, "expected-batches must be positive")
    lines = args.input.read_text(encoding="utf-8").splitlines()
    require(len(lines) == args.expected_batches,
            f"raw batch count {len(lines)} != {args.expected_batches}")

    batch_sequences: list[int] = []
    gateway_sequences: list[int] = []
    for line_number, line in enumerate(lines, start=1):
        try:
            message = json.loads(line)
            batch_seq, record_sequences = validate_message(message, args.device_id)
        except (json.JSONDecodeError, ValueError) as error:
            raise ValueError(f"line {line_number}: {error}") from error
        batch_sequences.append(batch_seq)
        gateway_sequences.extend(record_sequences)

    expected_batch_sequences = list(range(1, args.expected_batches + 1))
    require(batch_sequences == expected_batch_sequences,
            "batch_seq has missing/duplicate/reordered values")
    expected_gateway_sequences = list(range(1, gateway_sequences[-1] + 1))
    require(gateway_sequences == expected_gateway_sequences,
            "gateway seq has missing/duplicate/reordered values")
    summary = {
        "status": "PASS",
        "raw_batches": len(lines),
        "unique_batch_seq": len(set(batch_sequences)),
        "unique_gateway_seq": len(set(gateway_sequences)),
        "missing_batch_seq": 0,
        "missing_gateway_seq": 0,
        "duplicate_batch_seq": 0,
        "duplicate_gateway_seq": 0,
        "first_batch_seq": batch_sequences[0],
        "last_batch_seq": batch_sequences[-1],
        "first_gateway_seq": gateway_sequences[0],
        "last_gateway_seq": gateway_sequences[-1],
    }
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
