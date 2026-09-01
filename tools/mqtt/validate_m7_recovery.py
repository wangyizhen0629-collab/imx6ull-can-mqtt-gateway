#!/usr/bin/env python3
"""校验 M7 QoS 1 重放：原始重复可见，按 device_id+seq 去重后完整有序。"""

import argparse
import json
from pathlib import Path
from typing import Any

from validate_m6_batches import validate_message


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--device-id", required=True)
    parser.add_argument("--expected-first-seq", required=True, type=int)
    parser.add_argument("--expected-last-seq", required=True, type=int)
    parser.add_argument("--require-raw-duplicates", action="store_true")
    return parser.parse_args()


def canonical_record(record: dict[str, Any]) -> str:
    return json.dumps(record, sort_keys=True, separators=(",", ":"))


def main() -> int:
    args = parse_args()
    require(args.expected_first_seq > 0, "expected-first-seq must be positive")
    require(args.expected_last_seq >= args.expected_first_seq,
            "invalid expected sequence range")
    lines = args.input.read_text(encoding="utf-8").splitlines()
    require(bool(lines), "subscriber capture is empty")

    first_seen: list[int] = []
    unique_records: dict[int, str] = {}
    raw_records = 0
    batch_sequences: list[int] = []
    for line_number, line in enumerate(lines, start=1):
        try:
            message = json.loads(line)
            batch_seq, sequences = validate_message(message, args.device_id)
        except (json.JSONDecodeError, ValueError) as error:
            raise ValueError(f"line {line_number}: {error}") from error
        batch_sequences.append(batch_seq)
        for record, sequence in zip(message["records"], sequences):
            raw_records += 1
            encoded = canonical_record(record)
            previous = unique_records.get(sequence)
            if previous is None:
                unique_records[sequence] = encoded
                first_seen.append(sequence)
            else:
                require(previous == encoded,
                        f"conflicting duplicate gateway seq {sequence}")

    expected = list(range(args.expected_first_seq,
                          args.expected_last_seq + 1))
    require(first_seen == expected,
            "deduplicated gateway seq has missing/reordered values")
    raw_duplicates = raw_records - len(unique_records)
    if args.require_raw_duplicates:
        require(raw_duplicates > 0, "expected raw duplicates were not observed")
    summary = {
        "status": "PASS",
        "raw_batches": len(lines),
        "raw_records": raw_records,
        "raw_duplicate_records": raw_duplicates,
        "unique_gateway_seq": len(unique_records),
        "effective_duplicate_records": 0,
        "missing_gateway_seq": 0,
        "first_gateway_seq": first_seen[0],
        "last_gateway_seq": first_seen[-1],
        "unique_batch_seq": len(set(batch_sequences)),
    }
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
