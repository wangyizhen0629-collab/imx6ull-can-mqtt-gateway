#!/usr/bin/env python3
"""用同一组黄金向量核对 DBC 位布局、缩放和协议校验规则。"""

import argparse
import csv
from dataclasses import dataclass
from decimal import Decimal
from pathlib import Path
import re
import sys


MESSAGE_RE = re.compile(r"^BO_\s+(\d+)\s+(\w+):\s+(\d+)\s+(\w+)\s*$")
SIGNAL_RE = re.compile(
    r'^\s*SG_\s+(\w+)\s*:\s*(\d+)\|(\d+)@([01])([+-])\s*'
    r'\(([^,]+),([^\)]+)\)\s*\[([^|]+)\|([^\]]+)\]\s*"([^"]*)"'
)


@dataclass(frozen=True)
class Signal:
    start: int
    length: int
    byte_order: int
    signed: bool
    factor: Decimal
    offset: Decimal


@dataclass
class Message:
    name: str
    dlc: int
    transmitter: str
    signals: dict[str, Signal]


EXPECTED_MESSAGES = {
    0x100: ("VehicleDynamics", {
        "VehicleSpeed": ("vehicle_speed_centi_kph", Decimal("100")),
        "EngineSpeed": ("engine_speed_quarter_rpm", Decimal("4")),
        "ThrottlePosition": ("throttle_tenth_percent", Decimal("10")),
        "Gear": ("gear", Decimal("1")),
        "RollingCounter": ("rolling_counter", Decimal("1")),
        "Checksum": ("checksum", Decimal("1")),
    }),
    0x101: ("PowerStatus", {
        "BatteryVoltage": ("battery_millivolt", Decimal("1000")),
        "CoolantTemperature": ("coolant_celsius", Decimal("1")),
        "StateOfCharge": ("soc_tenth_percent", Decimal("10")),
        "FaultFlags": ("fault_flags", Decimal("1")),
        "RollingCounter": ("rolling_counter", Decimal("1")),
        "Checksum": ("checksum", Decimal("1")),
    }),
    0x102: ("BodyStatus", {
        "Odometer": ("odometer_tenth_km", Decimal("10")),
        "DoorFlags": ("door_flags", Decimal("1")),
        "IgnitionState": ("ignition_state", Decimal("1")),
        "RollingCounter": ("rolling_counter", Decimal("1")),
        "Checksum": ("checksum", Decimal("1")),
    }),
}


def parse_dbc(path: Path) -> dict[int, Message]:
    messages: dict[int, Message] = {}
    current: Message | None = None
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        message_match = MESSAGE_RE.match(line)
        if message_match:
            can_id = int(message_match.group(1))
            current = Message(message_match.group(2), int(message_match.group(3)),
                              message_match.group(4), {})
            if can_id in messages:
                raise ValueError(f"{path}:{line_number}: duplicate CAN ID {can_id}")
            messages[can_id] = current
            continue
        signal_match = SIGNAL_RE.match(line)
        if signal_match:
            if current is None:
                raise ValueError(f"{path}:{line_number}: signal before message")
            name = signal_match.group(1)
            if name in current.signals:
                raise ValueError(f"{path}:{line_number}: duplicate signal {name}")
            current.signals[name] = Signal(
                start=int(signal_match.group(2)),
                length=int(signal_match.group(3)),
                byte_order=int(signal_match.group(4)),
                signed=signal_match.group(5) == "-",
                factor=Decimal(signal_match.group(6)),
                offset=Decimal(signal_match.group(7)),
            )
    return messages


def extract(signal: Signal, payload: bytes) -> Decimal:
    if signal.byte_order != 1:
        raise ValueError("M4 checker only accepts explicitly reviewed Intel signals")
    if signal.start + signal.length > len(payload) * 8:
        raise ValueError("signal exceeds DLC")
    raw_frame = int.from_bytes(payload, byteorder="little", signed=False)
    raw = (raw_frame >> signal.start) & ((1 << signal.length) - 1)
    if signal.signed and raw & (1 << (signal.length - 1)):
        raw -= 1 << signal.length
    return Decimal(raw) * signal.factor + signal.offset


def validate_schema(messages: dict[int, Message]) -> None:
    if set(messages) != set(EXPECTED_MESSAGES):
        raise ValueError(f"unexpected DBC IDs: {sorted(messages)}")
    for can_id, (expected_name, expected_signals) in EXPECTED_MESSAGES.items():
        message = messages[can_id]
        if message.name != expected_name or message.dlc != 8:
            raise ValueError(f"0x{can_id:03x}: unexpected name or DLC")
        if message.transmitter != "STM32_ECU":
            raise ValueError(f"0x{can_id:03x}: unexpected transmitter")
        if set(message.signals) != set(expected_signals):
            raise ValueError(f"0x{can_id:03x}: unexpected signal set")


def validate_vectors(messages: dict[int, Message], path: Path) -> int:
    count = 0
    with path.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            count += 1
            name = row["name"]
            result = row["result"]
            can_id = int(row["can_id"], 0)
            dlc = int(row["dlc"], 10)
            payload = bytes.fromhex(row["data_hex"])
            if len(payload) != 8:
                raise ValueError(f"{name}: data_hex must contain 8 bytes")
            if result == "ID":
                if can_id in messages:
                    raise ValueError(f"{name}: ID case unexpectedly exists in DBC")
                continue
            if can_id not in messages:
                raise ValueError(f"{name}: successful/error case has unknown ID")
            message = messages[can_id]
            if result == "DLC":
                if dlc == message.dlc:
                    raise ValueError(f"{name}: DLC case is not invalid")
                continue
            if dlc != message.dlc:
                raise ValueError(f"{name}: unexpected DLC")
            xor_value = 0
            for value in payload[:7]:
                xor_value ^= value
            checksum_valid = xor_value == payload[7]
            if result == "CHECKSUM":
                if checksum_valid:
                    raise ValueError(f"{name}: checksum case is not invalid")
                continue
            if result != "OK" or not checksum_valid:
                raise ValueError(f"{name}: invalid result or checksum")

            expected_mapping = EXPECTED_MESSAGES[can_id][1]
            used_columns = set()
            for signal_name, signal in message.signals.items():
                column, storage_scale = expected_mapping[signal_name]
                actual = extract(signal, payload) * storage_scale
                expected = Decimal(row[column])
                if actual != expected:
                    raise ValueError(
                        f"{name}: {signal_name} expected {expected}, got {actual}"
                    )
                used_columns.add(column)
            for column, value in row.items():
                if column in {"name", "result", "can_id", "dlc", "data_hex"}:
                    continue
                if column not in used_columns and value != "NA":
                    raise ValueError(f"{name}: unrelated column {column} must be NA")
    if count == 0:
        raise ValueError("golden vector file is empty")
    return count


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dbc", required=True, type=Path)
    parser.add_argument("--vectors", required=True, type=Path)
    arguments = parser.parse_args()
    try:
        messages = parse_dbc(arguments.dbc)
        validate_schema(messages)
        count = validate_vectors(messages, arguments.vectors)
    except (KeyError, OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print(f"PASS: validated {len(messages)} DBC messages and {count} golden vectors")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
