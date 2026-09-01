#!/usr/bin/env python3
"""显式执行的 M7 loopback Broker/重连/SIGKILL 恢复门禁。"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import time
from typing import IO


DEVICE_ID = "m7-recovery-gateway"
TOPIC = "test/m7/recovery"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--broker", required=True, type=Path)
    parser.add_argument("--subscriber", required=True, type=Path)
    parser.add_argument("--driver", required=True, type=Path)
    parser.add_argument("--validator", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--port", type=int, default=21883)
    return parser.parse_args()


def wait_for_broker(port: int) -> None:
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                return
        except OSError:
            time.sleep(0.05)
    raise RuntimeError("Broker readiness timeout")


def stop_process(process: subprocess.Popen[bytes] | None) -> None:
    if process is None or process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=3)


def start_broker(args: argparse.Namespace, phase: int) -> tuple[
        subprocess.Popen[bytes], IO[bytes]]:
    log = (args.work_dir / f"broker-phase{phase}.log").open("wb")
    process = subprocess.Popen(
        [str(args.broker), "-p", str(args.port), "-v"],
        stdout=log, stderr=subprocess.STDOUT,
    )
    wait_for_broker(args.port)
    return process, log


def start_subscriber(args: argparse.Namespace, phase: int) -> tuple[
        subprocess.Popen[bytes], IO[bytes]]:
    capture = (args.work_dir / f"subscriber-phase{phase}.jsonl").open("wb")
    process = subprocess.Popen(
        [str(args.subscriber), "-h", "127.0.0.1", "-p", str(args.port),
         "-t", TOPIC, "-q", "1"], stdout=capture,
        stderr=subprocess.PIPE,
    )
    time.sleep(0.2)
    return process, capture


def driver_command(args: argparse.Namespace, append_count: int,
                   mode: str) -> list[str]:
    return [str(args.driver), str(args.work_dir / "spool.data"),
            str(args.port), DEVICE_ID, TOPIC, str(append_count), mode]


def run_driver(args: argparse.Namespace, phase: int,
               append_count: int) -> None:
    with (args.work_dir / f"driver-phase{phase}.stdout").open("wb") as output, \
         (args.work_dir / f"driver-phase{phase}.stderr").open("wb") as error:
        result = subprocess.run(
            driver_command(args, append_count, "drain"), stdout=output,
            stderr=error, timeout=25, check=False,
        )
    if result.returncode != 0:
        raise RuntimeError(f"driver phase {phase} failed: {result.returncode}")


def run_online_phase(args: argparse.Namespace, phase: int,
                     append_count: int) -> None:
    broker = subscriber = None
    broker_log = subscriber_log = None
    try:
        broker, broker_log = start_broker(args, phase)
        subscriber, subscriber_log = start_subscriber(args, phase)
        run_driver(args, phase, append_count)
        time.sleep(0.2)
    finally:
        stop_process(subscriber)
        stop_process(broker)
        if subscriber_log is not None:
            subscriber_log.close()
        if broker_log is not None:
            broker_log.close()


def run_disconnect_recovery_phase(args: argparse.Namespace) -> None:
    output = (args.work_dir / "driver-phase2.stdout").open("wb")
    error = (args.work_dir / "driver-phase2.stderr").open("wb")
    driver = subprocess.Popen(driver_command(args, 4, "drain"),
                              stdout=output, stderr=error)
    broker = subscriber = None
    broker_log = subscriber_log = None
    try:
        time.sleep(0.5)
        if driver.poll() is not None:
            raise RuntimeError("offline reconnect driver exited early")
        driver.send_signal(signal.SIGSTOP)
        broker, broker_log = start_broker(args, 2)
        subscriber, subscriber_log = start_subscriber(args, 2)
        driver.send_signal(signal.SIGCONT)
        if driver.wait(timeout=25) != 0:
            raise RuntimeError("offline reconnect driver failed")
        time.sleep(0.2)
    finally:
        if driver.poll() is None:
            driver.send_signal(signal.SIGCONT)
        stop_process(driver)
        stop_process(subscriber)
        stop_process(broker)
        output.close()
        error.close()
        if subscriber_log is not None:
            subscriber_log.close()
        if broker_log is not None:
            broker_log.close()


def run_sigkill_phase(args: argparse.Namespace) -> None:
    with (args.work_dir / "driver-phase3-killed.stdout").open("wb") as output, \
         (args.work_dir / "driver-phase3-killed.stderr").open("wb") as error:
        driver = subprocess.Popen(driver_command(args, 4, "hold"),
                                  stdout=output, stderr=error)
        time.sleep(0.5)
        if driver.poll() is not None:
            raise RuntimeError("SIGKILL target exited before injection")
        driver.send_signal(signal.SIGKILL)
        returncode = driver.wait(timeout=3)
    if returncode != -signal.SIGKILL:
        raise RuntimeError(f"unexpected kill return code {returncode}")
    (args.work_dir / "sigkill-result.json").write_text(
        json.dumps({"signal": "SIGKILL", "returncode": returncode,
                    "status": "PASS"}, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    run_online_phase(args, 3, 0)


def corrupt_state_for_safe_replay(work_dir: Path) -> None:
    state_path = work_dir / "spool.state"
    with state_path.open("r+b") as state:
        state.seek(8)
        original = state.read(1)
        if len(original) != 1:
            raise RuntimeError("state file too short")
        state.seek(8)
        state.write(bytes([original[0] ^ 0xFF]))
        state.flush()
        os.fsync(state.fileno())
    (work_dir / "state-corruption.json").write_text(
        json.dumps({"offset": 8, "operation": "xor-ff",
                    "purpose": "deterministic safe replay",
                    "status": "INJECTED"}, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def combine_and_validate(args: argparse.Namespace) -> None:
    combined = args.work_dir / "subscriber-combined.jsonl"
    with combined.open("wb") as destination:
        for phase in range(1, 5):
            destination.write(
                (args.work_dir / f"subscriber-phase{phase}.jsonl").read_bytes()
            )
    validation = subprocess.run(
        ["python3", "-B", str(args.validator), "--input", str(combined),
         "--device-id", DEVICE_ID, "--expected-first-seq", "1",
         "--expected-last-seq", "14", "--require-raw-duplicates"],
        capture_output=True, text=True, check=False,
    )
    (args.work_dir / "validator.stdout").write_text(validation.stdout,
                                                    encoding="utf-8")
    (args.work_dir / "validator.stderr").write_text(validation.stderr,
                                                    encoding="utf-8")
    if validation.returncode != 0:
        raise RuntimeError(f"validator failed: {validation.stderr.strip()}")


def write_manifest(work_dir: Path) -> None:
    entries: list[str] = []
    for path in sorted(work_dir.iterdir()):
        if path.is_file() and path.name != "manifest.sha256":
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            entries.append(f"{digest}  {path.name}")
    (work_dir / "manifest.sha256").write_text("\n".join(entries) + "\n",
                                               encoding="utf-8")


def main() -> int:
    args = parse_args()
    if args.work_dir.exists() or not 1 <= args.port <= 65535:
        raise ValueError("work-dir must not exist and port must be valid")
    args.work_dir.mkdir(parents=True)
    try:
        run_online_phase(args, 1, 6)
        run_disconnect_recovery_phase(args)
        run_sigkill_phase(args)
        corrupt_state_for_safe_replay(args.work_dir)
        run_online_phase(args, 4, 0)
        combine_and_validate(args)
        (args.work_dir / "result.json").write_text(
            json.dumps({"status": "PASS", "expected_first_seq": 1,
                        "expected_last_seq": 14,
                        "broker_disconnect_recovery": "PASS",
                        "sigkill_recovery": "PASS",
                        "state_reset_replay": "PASS"}, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    finally:
        write_manifest(args.work_dir)
    print((args.work_dir / "result.json").read_text(encoding="utf-8"), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
