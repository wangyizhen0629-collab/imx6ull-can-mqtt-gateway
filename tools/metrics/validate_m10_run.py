#!/usr/bin/env python3
"""把M10场景摘要和/proc原始样本校验为可追溯门禁报告。"""

import argparse
import json
import math
from pathlib import Path
from typing import Any

from report_proc_metrics import load_metrics, nearest_rank, require


SCENARIOS = {
    "stress_500": {"minimum_duration": 1800, "target_rate": 500},
    "stress_1000": {"minimum_duration": 1800, "target_rate": 1000},
    "broker_interruptions": {"minimum_duration": 6000, "target_rate": 111},
    "baseline_24h": {"minimum_duration": 86400, "target_rate": 111},
}


def integer(value: Any, name: str, minimum: int = 0) -> int:
    require(type(value) is int, f"{name} must be an integer")
    require(value >= minimum, f"{name} must be >= {minimum}")
    return value


def number(value: Any, name: str, minimum: float = 0.0) -> float:
    require(type(value) in {int, float} and math.isfinite(float(value)),
            f"{name} must be a finite number")
    require(float(value) >= minimum, f"{name} must be >= {minimum}")
    return float(value)


def object_field(value: dict[str, Any], name: str) -> dict[str, Any]:
    field = value.get(name)
    require(type(field) is dict, f"{name} must be an object")
    return field


def validate_run(run: dict[str, Any], proc: dict[str, Any]) -> dict[str, Any]:
    require(run.get("schema") == "gateway.m10.run.v1", "invalid M10 schema")
    require(type(run.get("run_id")) is str and bool(run["run_id"]),
            "run_id must be a non-empty string")
    scenario = run.get("scenario")
    require(scenario in SCENARIOS, "invalid M10 scenario")
    require(run.get("environment") == "imx6ull-physical",
            "M10 gate requires environment=imx6ull-physical")
    rule = SCENARIOS[scenario]
    duration = integer(run.get("duration_seconds"), "duration_seconds", 1)
    require(duration >= rule["minimum_duration"],
            f"{scenario} requires at least {rule['minimum_duration']} seconds")
    require(integer(run.get("target_rate_fps"), "target_rate_fps", 1) ==
            rule["target_rate"], f"{scenario} target_rate_fps mismatch")

    can = object_field(run, "can")
    queue = object_field(run, "queue")
    mqtt = object_field(run, "mqtt")
    spool = object_field(run, "spool")
    process = object_field(run, "process")
    frames_total = integer(can.get("frames_total"), "can.frames_total", 1)
    require(frames_total >= run["target_rate_fps"] * duration,
            "CAN frames_total is below target_rate_fps * duration_seconds")
    frames_by_id = object_field(can, "frames_by_id")
    gaps_by_id = object_field(can, "counter_gaps_by_id")
    expected_ids = {"0x100", "0x101", "0x102"}
    require(set(frames_by_id) == expected_ids,
            "can.frames_by_id must contain exactly 0x100/0x101/0x102")
    require(set(gaps_by_id) == expected_ids,
            "can.counter_gaps_by_id must contain exactly 0x100/0x101/0x102")
    counted_frames = sum(
        integer(frames_by_id[can_id], f"can.frames_by_id.{can_id}", 1)
        for can_id in sorted(expected_ids)
    )
    require(counted_frames == frames_total,
            "per-ID CAN frame counts do not equal frames_total")
    for can_id in sorted(expected_ids):
        require(integer(gaps_by_id[can_id],
                        f"can.counter_gaps_by_id.{can_id}") == 0,
                f"CAN counter gaps for {can_id} must be zero")
    for field in ("error_frame_count", "rx_error_delta", "rx_dropped_delta",
                  "rx_over_errors_delta"):
        require(integer(can.get(field), f"can.{field}") == 0,
                f"can.{field} must be zero")
    require(integer(queue.get("drops"), "queue.drops") == 0,
            "queue drops must be zero")
    integer(mqtt.get("raw_batches"), "mqtt.raw_batches", 1)
    unique_records = integer(mqtt.get("unique_records"),
                             "mqtt.unique_records", 1)
    require(unique_records == frames_total,
            "MQTT unique_records must equal drained CAN frames_total")
    require(integer(mqtt.get("missing_records"), "mqtt.missing_records") == 0,
            "MQTT missing records must be zero")
    integer(mqtt.get("raw_duplicate_records"), "mqtt.raw_duplicate_records")
    require(integer(mqtt.get("effective_duplicate_records"),
                    "mqtt.effective_duplicate_records") == 0,
            "effective MQTT duplicates must be zero")
    reconnects = integer(mqtt.get("reconnects"), "mqtt.reconnects")
    integer(spool.get("max_pending_records"), "spool.max_pending_records")
    require(integer(process.get("exit_count"), "process.exit_count") == 0,
            "gatewayd exit count must be zero")

    interruptions = run.get("broker_interruptions")
    require(type(interruptions) is list, "broker_interruptions must be an array")
    latencies: list[float] = []
    if scenario == "broker_interruptions":
        require(len(interruptions) == 20,
                "broker_interruptions requires exactly 20 cycles")
        require(reconnects >= 20, "broker_interruptions requires >=20 reconnects")
        for expected_cycle, cycle in enumerate(interruptions, start=1):
            require(type(cycle) is dict, "interruption cycle must be an object")
            require(integer(cycle.get("cycle"), "interruption.cycle", 1) ==
                    expected_cycle, "interruption cycles must be contiguous 1..20")
            number(cycle.get("offline_seconds"), "interruption.offline_seconds",
                   300.0)
            latencies.append(number(cycle.get("reconnect_latency_ms"),
                                    "interruption.reconnect_latency_ms"))
    else:
        require(len(interruptions) == 0,
                "non-interruption scenarios must not claim interruption cycles")

    require(proc["missing_samples"] == 0, "proc metrics contain missing samples")
    require(proc["identity_mismatch_samples"] == 0,
            "proc metrics contain identity mismatches")
    require(proc["read_error_samples"] == 0,
            "proc metrics contain read errors")
    require(proc["pid_changes"] == 0, "proc metrics observed a process change")
    require(proc["observed_duration_seconds"] >= duration,
            "proc metrics do not cover the full scenario duration")
    require(proc["total_samples"] >= duration + 1,
            "one-second proc sampling requires at least duration+1 samples")
    require(proc["sample_interval_seconds"]["maximum"] <= 2.5,
            "proc sampling contains an interval greater than 2.5 seconds")

    latency_summary = None
    if latencies:
        latency_summary = {
            "p50_ms_nearest_rank": nearest_rank(latencies, 0.50),
            "p95_ms_nearest_rank": nearest_rank(latencies, 0.95),
            "maximum_ms": max(latencies),
        }
    return {
        "status": "PASS",
        "run_id": run["run_id"],
        "scenario": scenario,
        "environment": run["environment"],
        "duration_seconds": duration,
        "target_rate_fps": run["target_rate_fps"],
        "functional_counters": {
            "can": can,
            "queue": queue,
            "mqtt": mqtt,
            "spool": spool,
            "process": process,
        },
        "proc_metrics": proc,
        "reconnect_latency": latency_summary,
    }


def render_markdown(summary: dict[str, Any]) -> str:
    proc = summary["proc_metrics"]
    cpu = proc["cpu_percent_total_capacity"]
    rss = proc["vmrss_kb"]
    lines = [
        "# M10场景门禁报告",
        "",
        f"- 结果：{summary['status']}",
        f"- run_id：`{summary['run_id']}`",
        f"- 场景：`{summary['scenario']}`",
        f"- 环境：`{summary['environment']}`",
        f"- 持续时间/目标速率：{summary['duration_seconds']}秒 / {summary['target_rate_fps']}帧/s",
        f"- /proc样本：{proc['ok_samples']}/{proc['total_samples']}个OK，PID变化{proc['pid_changes']}",
        f"- CPU总系统容量口径平均/P95/最大：{cpu['average']}/{cpu['p95_nearest_rank']}/{cpu['maximum']}%",
        f"- VmRSS KiB平均/P95/最大：{rss['average']}/{rss['p95_nearest_rank']}/{rss['maximum']}",
    ]
    if summary["reconnect_latency"] is not None:
        latency = summary["reconnect_latency"]
        lines.append(
            "- 重连/补传首条时延ms P50/P95/最大："
            f"{latency['p50_ms_nearest_rank']}/{latency['p95_ms_nearest_rank']}/{latency['maximum_ms']}"
        )
    lines.extend([
        "",
        "该PASS只适用于输入summary绑定的单个真实场景；不同压力档、20轮断网和24小时基准",
        "必须分别使用唯一run并各自通过，不能互相替代。",
        "",
    ])
    return "\n".join(lines)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-summary", required=True, type=Path)
    parser.add_argument("--proc-metrics", required=True, type=Path)
    parser.add_argument("--output-json", required=True, type=Path)
    parser.add_argument("--report-md", required=True, type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    require(not args.output_json.exists(), f"refusing to overwrite {args.output_json}")
    require(not args.report_md.exists(), f"refusing to overwrite {args.report_md}")
    run = json.loads(args.run_summary.read_text(encoding="utf-8"))
    require(type(run) is dict, "run summary must be a JSON object")
    _, proc = load_metrics(args.proc_metrics)
    summary = validate_run(run, proc)
    args.output_json.write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    args.report_md.write_text(render_markdown(summary), encoding="utf-8")
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"ERROR: {error}", file=__import__("sys").stderr)
        raise SystemExit(1)
