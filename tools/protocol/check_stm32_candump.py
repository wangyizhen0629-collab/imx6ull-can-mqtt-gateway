#!/usr/bin/env python3
"""核对 STM32 60 秒物理 CAN 抓包、DBC 编码和接口统计差值。"""

from __future__ import annotations

import argparse
import re
import statistics
import sys
from dataclasses import dataclass
from pathlib import Path


CAN_IDS = (0x100, 0x101, 0x102)
EXPECTED_COUNTS = {0x100: 6000, 0x101: 600, 0x102: 60}
NOMINAL_PERIOD_SECONDS = {0x100: 0.010, 0x101: 0.100, 0x102: 1.000}
DRIVE_CYCLE_TICKS = 6000
INITIAL_ODOMETER_TENTH_KM = 1234567
INITIAL_COOLANT_CELSIUS = 20

CANDUMP_PATTERN = re.compile(
    r"^\s*\(([0-9.]+)\)\s+\S+\s+([0-9A-Fa-f]+)\s+"
    r"\[(\d+)\]\s+(.+?)\s*$"
)
CAN_STATE_PATTERN = re.compile(
    r"can state (\S+) \(berr-counter tx (\d+) rx (\d+)\)"
)


@dataclass(frozen=True)
class Frame:
    line_number: int
    timestamp: float
    can_id: int
    dlc: int
    data: bytes


@dataclass(frozen=True)
class VehicleState:
    speed_centi_kph: int
    engine_quarter_rpm: int
    throttle_tenth_percent: int
    gear: int
    battery_millivolt: int
    coolant_celsius: int
    soc_tenth_percent: int
    fault_flags: int
    odometer_tenth_km: int
    door_flags: int
    ignition_state: int


def fail(message: str) -> None:
    raise ValueError(message)


def parse_candump(path: Path) -> dict[int, list[Frame]]:
    frames = {can_id: [] for can_id in CAN_IDS}
    with path.open(encoding="utf-8") as stream:
        for line_number, line in enumerate(stream, 1):
            match = CANDUMP_PATTERN.match(line)
            if match is None:
                fail(f"candump line {line_number}: unsupported format")
            can_id = int(match.group(2), 16)
            if can_id not in frames:
                fail(f"candump line {line_number}: unexpected CAN ID 0x{can_id:x}")
            dlc = int(match.group(3), 10)
            try:
                data = bytes.fromhex(match.group(4))
            except ValueError as error:
                fail(f"candump line {line_number}: invalid payload: {error}")
            frames[can_id].append(
                Frame(line_number, float(match.group(1)), can_id, dlc, data)
            )
    return frames


def checksum(payload: bytes) -> int:
    value = 0
    for byte in payload[:7]:
        value ^= byte
    return value


def pack_u16_le(value: int) -> bytes:
    return bytes((value & 0xFF, (value >> 8) & 0xFF))


def pack_u24_le(value: int) -> bytes:
    return bytes(
        (value & 0xFF, (value >> 8) & 0xFF, (value >> 16) & 0xFF)
    )


def finalize_payload(prefix: bytes, counter: int) -> bytes:
    payload = bytearray(prefix + bytes((counter, 0)))
    payload[7] = checksum(payload)
    return bytes(payload)


def select_gear(speed_centi_kph: int) -> int:
    if speed_centi_kph <= 1200:
        return 1
    if speed_centi_kph <= 2500:
        return 2
    if speed_centi_kph <= 4000:
        return 3
    return 4


def calculate_engine_quarter_rpm(speed_centi_kph: int, gear: int) -> int:
    if speed_centi_kph == 0:
        rpm = 800
    elif gear == 1:
        rpm = 900 + speed_centi_kph * 7 // 5
    elif gear == 2:
        rpm = 900 + speed_centi_kph * 7 // 10
    elif gear == 3:
        rpm = 900 + speed_centi_kph * 9 // 20
    else:
        rpm = 900 + speed_centi_kph // 4
    return rpm * 4


def calculate_fast_state(phase: int) -> tuple[int, int, int, int, int, int]:
    if phase < 300 or phase >= 5800:
        return 0, 0, 0, 0, 1, 0
    if phase < 500 or phase >= 5500:
        return 0, 800 * 4, 0, 0, 0, 2
    if phase < 2000:
        speed = (phase - 500) * 6000 // 1500
        throttle = 320
    elif phase < 4000:
        speed = 6000
        throttle = 160
    else:
        speed = 6000 - (phase - 4000) * 6000 // 1500
        throttle = 0
    gear = select_gear(speed)
    return speed, calculate_engine_quarter_rpm(speed, gear), throttle, gear, 0, 2


def build_expected_states() -> list[VehicleState]:
    states = []
    coolant = INITIAL_COOLANT_CELSIUS
    odometer = INITIAL_ODOMETER_TENTH_KM
    distance_accumulator = 0
    warmup_ticks = 0
    cooldown_ticks = 0

    for phase in range(DRIVE_CYCLE_TICKS):
        speed, engine, throttle, gear, doors, ignition = calculate_fast_state(
            phase
        )
        states.append(
            VehicleState(
                speed,
                engine,
                throttle,
                gear,
                12600 if engine == 0 else 13800,
                coolant,
                800,
                0,
                odometer,
                doors,
                ignition,
            )
        )

        distance_accumulator += speed
        if distance_accumulator >= 3600000:
            distance_accumulator -= 3600000
            odometer = min(odometer + 1, 0xFFFFFF)

        if engine != 0:
            cooldown_ticks = 0
            warmup_ticks += 1
            if warmup_ticks >= 100:
                warmup_ticks = 0
                coolant = min(coolant + 1, 90)
        else:
            warmup_ticks = 0
            cooldown_ticks += 1
            if cooldown_ticks >= 500:
                cooldown_ticks = 0
                coolant = max(coolant - 1, INITIAL_COOLANT_CELSIUS)
    return states


def vehicle_payload(state: VehicleState, counter: int) -> bytes:
    return finalize_payload(
        pack_u16_le(state.speed_centi_kph)
        + pack_u16_le(state.engine_quarter_rpm)
        + bytes((state.throttle_tenth_percent // 4, state.gear)),
        counter,
    )


def power_payload(state: VehicleState, counter: int) -> bytes:
    return finalize_payload(
        pack_u16_le(state.battery_millivolt)
        + bytes((state.coolant_celsius + 40, state.soc_tenth_percent // 4))
        + pack_u16_le(state.fault_flags),
        counter,
    )


def body_payload(state: VehicleState, counter: int) -> bytes:
    return finalize_payload(
        pack_u24_le(state.odometer_tenth_km)
        + bytes((state.door_flags & 0x0F, state.ignition_state & 0x03, 0)),
        counter,
    )


def validate_frames(frames: dict[int, list[Frame]]) -> list[str]:
    states = build_expected_states()
    summaries = []

    for can_id in CAN_IDS:
        message_frames = frames[can_id]
        if len(message_frames) != EXPECTED_COUNTS[can_id]:
            fail(
                f"0x{can_id:03x}: expected {EXPECTED_COUNTS[can_id]} frames, "
                f"got {len(message_frames)}"
            )
        intervals = []
        for index, frame in enumerate(message_frames):
            if frame.dlc != 8 or len(frame.data) != 8:
                fail(f"line {frame.line_number}: expected DLC/data length 8")
            if checksum(frame.data) != frame.data[7]:
                fail(f"line {frame.line_number}: XOR mismatch")
            if frame.data[6] != index & 0xFF:
                fail(
                    f"line {frame.line_number}: counter expected "
                    f"{index & 0xFF}, got {frame.data[6]}"
                )
            if can_id == 0x102 and (
                frame.data[3] & 0xF0
                or frame.data[4] & 0xFC
                or frame.data[5] != 0
            ):
                fail(f"line {frame.line_number}: BodyStatus spare bits not zero")

            if can_id == 0x100:
                expected = vehicle_payload(states[index], index & 0xFF)
            elif can_id == 0x101:
                phase = index * 10 + 9
                expected = power_payload(states[phase], index & 0xFF)
            else:
                phase = index * 100 + 99
                expected = body_payload(states[phase], index & 0xFF)
            if frame.data != expected:
                fail(
                    f"line {frame.line_number}: semantic payload mismatch: "
                    f"expected {expected.hex()}, got {frame.data.hex()}"
                )
            if index > 0:
                intervals.append(frame.timestamp - message_frames[index - 1].timestamp)

        nominal = NOMINAL_PERIOD_SECONDS[can_id]
        minimum = min(intervals)
        maximum = max(intervals)
        mean = statistics.fmean(intervals)
        # 这里只拒绝明显丢周期或突发补发，不把该范围宣称为硬实时容差。
        if minimum < nominal * 0.5 or maximum > nominal * 2.0:
            fail(
                f"0x{can_id:03x}: gross interval anomaly: "
                f"min={minimum:.6f}, max={maximum:.6f}"
            )
        if abs(mean - nominal) > nominal * 0.01:
            fail(
                f"0x{can_id:03x}: mean interval {mean:.6f} differs by over 1%"
            )
        summaries.append(
            f"0x{can_id:03x}: count={len(message_frames)}, "
            f"interval_min={minimum:.6f}, interval_mean={mean:.6f}, "
            f"interval_max={maximum:.6f}"
        )
    return summaries


def next_values(lines: list[str], index: int) -> list[int]:
    for candidate in lines[index + 1 :]:
        if candidate.strip():
            values = []
            for token in candidate.split():
                try:
                    values.append(int(token))
                except ValueError:
                    # 旧版 iproute2 会把 numtxqueues 等属性接在数值行末尾。
                    break
            if not values:
                fail("statistics value row does not start with integers")
            return values
    fail("missing statistics value row")


def parse_can_statistics(path: Path) -> dict[str, int | str]:
    lines = path.read_text(encoding="utf-8").splitlines()
    result: dict[str, int | str] = {}
    for index, line in enumerate(lines):
        state_match = CAN_STATE_PATTERN.search(line)
        if state_match is not None:
            result["state"] = state_match.group(1)
            result["berr_tx"] = int(state_match.group(2))
            result["berr_rx"] = int(state_match.group(3))
        bitrate_match = re.search(r"\bbitrate (\d+)", line)
        if bitrate_match is not None:
            result["bitrate"] = int(bitrate_match.group(1))
        stripped = line.strip()
        if stripped.startswith("re-started "):
            names = stripped.split()
            values = next_values(lines, index)
            if len(names) != len(values):
                fail(f"{path}: CAN state statistics column mismatch")
            result.update(zip(names, values))
        elif stripped.startswith("RX: bytes "):
            names = [f"rx_{name}" for name in stripped[4:].split()]
            values = next_values(lines, index)
            if len(names) != len(values):
                fail(f"{path}: RX statistics column mismatch")
            result.update(zip(names, values))
        elif stripped.startswith("TX: bytes "):
            names = [f"tx_{name}" for name in stripped[4:].split()]
            values = next_values(lines, index)
            if len(names) != len(values):
                fail(f"{path}: TX statistics column mismatch")
            result.update(zip(names, values))
    required = {
        "state",
        "berr_tx",
        "berr_rx",
        "bitrate",
        "re-started",
        "bus-errors",
        "arbit-lost",
        "error-warn",
        "error-pass",
        "bus-off",
        "rx_bytes",
        "rx_packets",
        "rx_errors",
        "rx_dropped",
        "rx_overrun",
    }
    missing = sorted(required - result.keys())
    if missing:
        fail(f"{path}: missing CAN statistics: {', '.join(missing)}")
    return result


def validate_can_statistics(before_path: Path, after_path: Path) -> str:
    before = parse_can_statistics(before_path)
    after = parse_can_statistics(after_path)
    for label, snapshot in (("before", before), ("after", after)):
        if snapshot["state"] != "ERROR-ACTIVE":
            fail(f"{label}: expected ERROR-ACTIVE, got {snapshot['state']}")
        if snapshot["bitrate"] != 500000:
            fail(f"{label}: expected bitrate 500000")
        if snapshot["berr_tx"] != 0 or snapshot["berr_rx"] != 0:
            fail(f"{label}: non-zero CAN berr counter")

    zero_delta_fields = (
        "re-started",
        "bus-errors",
        "arbit-lost",
        "error-warn",
        "error-pass",
        "bus-off",
        "rx_errors",
        "rx_dropped",
        "rx_overrun",
    )
    for field in zero_delta_fields:
        delta = int(after[field]) - int(before[field])
        if delta != 0:
            fail(f"CAN statistics {field} delta expected 0, got {delta}")
    packet_delta = int(after["rx_packets"]) - int(before["rx_packets"])
    byte_delta = int(after["rx_bytes"]) - int(before["rx_bytes"])
    expected_captured = sum(EXPECTED_COUNTS.values())
    if packet_delta < expected_captured:
        fail(
            f"RX packet delta {packet_delta} is smaller than captured "
            f"{expected_captured}"
        )
    if byte_delta != packet_delta * 8:
        fail(
            f"RX byte delta {byte_delta} does not equal "
            f"8 * packet delta {packet_delta}"
        )
    return (
        f"CAN stats: rx_packets_delta={packet_delta}, "
        f"rx_bytes_delta={byte_delta}, outside_capture={packet_delta - expected_captured}, "
        "error_deltas=0"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--candump", required=True, type=Path)
    parser.add_argument("--can-before", required=True, type=Path)
    parser.add_argument("--can-after", required=True, type=Path)
    arguments = parser.parse_args()
    try:
        summaries = validate_frames(parse_candump(arguments.candump))
        statistics_summary = validate_can_statistics(
            arguments.can_before, arguments.can_after
        )
    except (OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    for summary in summaries:
        print(f"PASS: {summary}")
    print(f"PASS: {statistics_summary}")
    print("PASS: all 6660 physical frames match the deterministic DBC scenario")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
