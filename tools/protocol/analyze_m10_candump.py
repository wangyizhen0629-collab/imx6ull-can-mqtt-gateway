#!/usr/bin/env python3
"""分析M10真实candump的profile、计数、counter、XOR和CAN统计。"""

from __future__ import annotations

import argparse
import json
import math
import re
import statistics
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from check_stm32_candump import parse_can_statistics


CAN_IDS = (0x100, 0x101, 0x102)
CAN_ERR_FLAG = 0x20000000
CAN_RTR_FLAG = 0x40000000
CAN_EFF_FLAG = 0x80000000
PRETTY_PATTERN = re.compile(
    r"^\s*\(([0-9.]+)\)\s+\S+\s+([0-9A-Fa-f]+)\s+"
    r"\[(\d+)\]\s+(.+?)\s*$"
)
LOG_PATTERN = re.compile(
    r"^\s*\(([0-9.]+)\)\s+\S+\s+([0-9A-Fa-f]+)#([0-9A-Fa-f]*)\s*$"
)


@dataclass(frozen=True)
class Frame:
    line_number: int
    timestamp: float
    raw_can_id: int
    can_id: int
    dlc: int
    data: bytes


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def parse_frames(path: Path) -> list[Frame]:
    frames = []
    with path.open(encoding="utf-8") as stream:
        for line_number, line in enumerate(stream, 1):
            pretty = PRETTY_PATTERN.match(line)
            compact = LOG_PATTERN.match(line)
            if pretty is not None:
                timestamp = float(pretty.group(1))
                raw_can_id = int(pretty.group(2), 16)
                dlc = int(pretty.group(3), 10)
                data_text = pretty.group(4)
            elif compact is not None:
                timestamp = float(compact.group(1))
                raw_can_id = int(compact.group(2), 16)
                data_text = compact.group(3)
                require(len(data_text) % 2 == 0,
                        f"line {line_number}: odd compact payload length")
                dlc = len(data_text) // 2
            else:
                raise ValueError(
                    f"candump line {line_number}: unsupported format"
                )
            try:
                data = bytes.fromhex(data_text)
            except ValueError as error:
                raise ValueError(
                    f"candump line {line_number}: invalid payload: {error}"
                ) from error
            can_id = raw_can_id & 0x7FF
            frames.append(
                Frame(line_number, timestamp, raw_can_id, can_id, dlc, data)
            )
    require(bool(frames), "candump contains no frames")
    return frames


def load_profile(path: Path, profile_name: str) -> dict[str, Any]:
    document = json.loads(path.read_text(encoding="utf-8"))
    require(document.get("schema") == "gateway.m10.stm32-profiles.v1",
            "invalid M10 profile schema")
    profiles = document.get("profiles")
    require(type(profiles) is dict and profile_name in profiles,
            f"unknown M10 profile: {profile_name}")
    profile = profiles[profile_name]
    require(type(profile) is dict, "profile must be an object")
    counts = profile.get("frames_per_superframe")
    require(type(counts) is dict and set(counts) ==
            {"0x100", "0x101", "0x102"},
            "profile must define exactly three CAN IDs")
    require(sum(counts.values()) * 1000 ==
            profile["target_rate_fps"] * profile["superframe_ms"],
            "profile frame counts do not reconcile with target rate")
    return profile


def checksum(payload: bytes) -> int:
    value = 0
    for byte in payload[:7]:
        value ^= byte
    return value


def stress_pattern() -> list[int]:
    pattern = []
    for slot in range(100):
        if slot == 99:
            pattern.append(0x102)
        elif (slot + 1) % 10 == 0:
            pattern.append(0x101)
        else:
            pattern.append(0x100)
    require(pattern.count(0x100) == 90, "internal 0x100 schedule error")
    require(pattern.count(0x101) == 9, "internal 0x101 schedule error")
    require(pattern.count(0x102) == 1, "internal 0x102 schedule error")
    return pattern


def validate_stress_sequence(frames: list[Frame]) -> int:
    pattern = stress_pattern()
    candidates = [
        start for start, can_id in enumerate(pattern)
        if can_id == frames[0].can_id
    ]
    for start in candidates:
        if all(
            frame.can_id == pattern[(start + index) % len(pattern)]
            for index, frame in enumerate(frames)
        ):
            return start
    raise ValueError("stress CAN ID sequence is not a contiguous 100-slot pattern")


def observed_rate(frames: list[Frame]) -> float:
    require(len(frames) >= 2, "at least two frames are required for rate")
    duration = frames[-1].timestamp - frames[0].timestamp
    require(duration > 0.0, "frame timestamps are not increasing")
    return (len(frames) - 1) / duration


def validate_capture(
    all_frames: list[Frame], profile: dict[str, Any],
    minimum_duration: float, tolerance_percent: float,
) -> dict[str, Any]:
    error_frames = [
        frame for frame in all_frames if frame.raw_can_id & CAN_ERR_FLAG
    ]
    data_frames = [
        frame for frame in all_frames
        if not frame.raw_can_id & CAN_ERR_FLAG
    ]
    require(not error_frames,
            f"candump contains {len(error_frames)} CAN error frames")
    require(bool(data_frames), "candump contains no data frames")

    unexpected = sorted({
        frame.can_id for frame in data_frames
        if frame.can_id not in CAN_IDS
        or frame.raw_can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG)
    })
    require(not unexpected,
            "unexpected/extended/RTR CAN IDs: " +
            ", ".join(f"0x{can_id:x}" for can_id in unexpected))

    by_id = {can_id: [] for can_id in CAN_IDS}
    counter_gaps = {can_id: 0 for can_id in CAN_IDS}
    for frame in data_frames:
        require(frame.dlc == 8 and len(frame.data) == 8,
                f"line {frame.line_number}: expected DLC/data length 8")
        require(checksum(frame.data) == frame.data[7],
                f"line {frame.line_number}: XOR mismatch")
        if frame.can_id == 0x102:
            require(not (frame.data[3] & 0xF0 or frame.data[4] & 0xFC
                         or frame.data[5] != 0),
                    f"line {frame.line_number}: BodyStatus spare bits not zero")
        message_frames = by_id[frame.can_id]
        if message_frames:
            expected_counter = (message_frames[-1].data[6] + 1) & 0xFF
            if frame.data[6] != expected_counter:
                counter_gaps[frame.can_id] += (
                    frame.data[6] - expected_counter
                ) & 0xFF
        message_frames.append(frame)

    for can_id in CAN_IDS:
        require(len(by_id[can_id]) >= 2,
                f"0x{can_id:03x}: fewer than two captured frames")
        require(counter_gaps[can_id] == 0,
                f"0x{can_id:03x}: rolling counter gap")

    duration = data_frames[-1].timestamp - data_frames[0].timestamp
    require(duration >= minimum_duration,
            f"observed duration {duration:.6f}s is below {minimum_duration}s")
    target_rate = profile["target_rate_fps"]
    total_rate = observed_rate(data_frames)
    total_intervals = [
        right.timestamp - left.timestamp
        for left, right in zip(data_frames, data_frames[1:])
    ]
    require(min(total_intervals) >= 0.0,
            "aggregate candump timestamps are not monotonic")
    tolerance = tolerance_percent / 100.0
    require(abs(total_rate - target_rate) <= target_rate * tolerance,
            f"total rate {total_rate:.6f} differs from {target_rate} by over "
            f"{tolerance_percent}%")
    if profile["scheduler"] == "fixed-slot-superframe":
        nominal_slot = profile["slot_period_ms"] / 1000.0
        require(min(total_intervals) >= nominal_slot * 0.25,
                "stress capture contains a burst interval below 25% of one slot")
        require(max(total_intervals) <= nominal_slot * 2.5,
                "stress capture contains an interval above 2.5 slots")

    counts_contract = profile["frames_per_superframe"]
    superframe_seconds = profile["superframe_ms"] / 1000.0
    per_id: dict[str, Any] = {}
    for can_id in CAN_IDS:
        key = f"0x{can_id:03x}"
        expected_rate = counts_contract[key] / superframe_seconds
        actual_rate = observed_rate(by_id[can_id])
        require(abs(actual_rate - expected_rate) <= expected_rate * tolerance,
                f"{key}: rate {actual_rate:.6f} differs from "
                f"{expected_rate:.6f} by over {tolerance_percent}%")
        intervals = [
            right.timestamp - left.timestamp
            for left, right in zip(by_id[can_id], by_id[can_id][1:])
        ]
        require(min(intervals) > 0.0,
                f"{key}: non-increasing timestamp")
        expected_period = 1.0 / expected_rate
        require(min(intervals) >= expected_period * 0.4,
                f"{key}: gross interval is below 40% of nominal")
        require(max(intervals) <= expected_period * 2.2,
                f"{key}: gross interval is above 220% of nominal")
        per_id[key] = {
            "frames": len(by_id[can_id]),
            "counter_gaps": 0,
            "expected_rate_fps": expected_rate,
            "observed_rate_fps": actual_rate,
            "interval_seconds": {
                "minimum": min(intervals),
                "mean": statistics.fmean(intervals),
                "maximum": max(intervals),
            },
        }

    sequence_start = None
    if profile["scheduler"] == "fixed-slot-superframe":
        sequence_start = validate_stress_sequence(data_frames)

    return {
        "frames_total": len(data_frames),
        "frames_by_id": {
            f"0x{can_id:03x}": len(by_id[can_id]) for can_id in CAN_IDS
        },
        "counter_gaps_by_id": {
            f"0x{can_id:03x}": counter_gaps[can_id] for can_id in CAN_IDS
        },
        "error_frame_count": len(error_frames),
        "observed_duration_seconds": duration,
        "observed_rate_fps": total_rate,
        "aggregate_interval_seconds": {
            "minimum": min(total_intervals),
            "mean": statistics.fmean(total_intervals),
            "maximum": max(total_intervals),
        },
        "per_id": per_id,
        "stress_sequence_start_slot": sequence_start,
    }


def validate_can_statistics(
    before_path: Path, after_path: Path, captured_frames: int
) -> dict[str, Any]:
    before = parse_can_statistics(before_path)
    after = parse_can_statistics(after_path)
    for label, snapshot in (("before", before), ("after", after)):
        require(snapshot["state"] == "ERROR-ACTIVE",
                f"{label}: expected ERROR-ACTIVE")
        require(snapshot["bitrate"] == 500000,
                f"{label}: expected bitrate 500000")
        require(snapshot["berr_tx"] == 0 and snapshot["berr_rx"] == 0,
                f"{label}: non-zero CAN berr counter")

    zero_delta_fields = (
        "re-started", "bus-errors", "arbit-lost", "error-warn",
        "error-pass", "bus-off", "rx_errors", "rx_dropped", "rx_overrun",
    )
    deltas = {
        field: int(after[field]) - int(before[field])
        for field in zero_delta_fields
    }
    require(all(value == 0 for value in deltas.values()),
            "CAN error/statistics delta is non-zero")
    packet_delta = int(after["rx_packets"]) - int(before["rx_packets"])
    byte_delta = int(after["rx_bytes"]) - int(before["rx_bytes"])
    require(packet_delta >= captured_frames,
            "RX packet delta is smaller than captured frames")
    require(byte_delta == packet_delta * 8,
            "RX byte delta does not equal 8 * packet delta")
    return {
        "rx_packets_delta": packet_delta,
        "rx_bytes_delta": byte_delta,
        "outside_capture_frames": packet_delta - captured_frames,
        "error_deltas": deltas,
    }


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument("--candump", required=True, type=Path)
    parser.add_argument("--can-before", required=True, type=Path)
    parser.add_argument("--can-after", required=True, type=Path)
    parser.add_argument("--profile", required=True, choices=("111", "500", "1000"))
    parser.add_argument(
        "--profile-spec", type=Path,
        default=repo_root / "protocol" / "m10_traffic_profiles.json",
    )
    parser.add_argument("--minimum-duration-seconds", required=True, type=float)
    parser.add_argument("--rate-tolerance-percent", type=float, default=2.0)
    parser.add_argument("--summary-json", required=True, type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    require(math.isfinite(args.minimum_duration_seconds)
            and args.minimum_duration_seconds > 0.0,
            "minimum duration must be positive")
    require(math.isfinite(args.rate_tolerance_percent)
            and 0.0 < args.rate_tolerance_percent <= 10.0,
            "rate tolerance must be in (0, 10]")
    require(not args.summary_json.exists(),
            f"refusing to overwrite {args.summary_json}")
    profile = load_profile(args.profile_spec, args.profile)
    capture = validate_capture(
        parse_frames(args.candump), profile,
        args.minimum_duration_seconds, args.rate_tolerance_percent,
    )
    can_stats = validate_can_statistics(
        args.can_before, args.can_after, capture["frames_total"]
    )
    summary = {
        "schema": "gateway.m10.candump-analysis.v1",
        "status": "PASS",
        "profile": args.profile,
        "target_rate_fps": profile["target_rate_fps"],
        "minimum_duration_seconds": args.minimum_duration_seconds,
        "rate_tolerance_percent": args.rate_tolerance_percent,
        "capture": capture,
        "can_statistics": can_stats,
    }
    args.summary_json.write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
