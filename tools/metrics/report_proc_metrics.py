#!/usr/bin/env python3
"""校验M10原始/proc CSV并生成可复核的CPU/RSS汇总。"""

import argparse
import csv
import json
import math
from pathlib import Path
from typing import Any


FIELDS = [
    "sample_index",
    "epoch_s",
    "uptime_s",
    "pid",
    "starttime_ticks",
    "proc_ticks",
    "total_ticks",
    "vmrss_kb",
    "vmhwm_kb",
    "linux_state",
    "sample_status",
]
VALID_STATUSES = {"OK", "MISSING", "IDENTITY_MISMATCH", "READ_ERROR"}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def nearest_rank(values: list[float], percentile: float) -> float:
    require(bool(values), "cannot calculate percentile for empty values")
    ordered = sorted(values)
    rank = max(1, math.ceil(percentile * len(ordered)))
    return ordered[rank - 1]


def rounded(value: float) -> float:
    return round(value, 6)


def parse_int(row: dict[str, str], field: str, line: int) -> int:
    try:
        return int(row[field])
    except ValueError as error:
        raise ValueError(f"line {line}: {field} must be an integer") from error


def parse_float(row: dict[str, str], field: str, line: int) -> float:
    try:
        value = float(row[field])
    except ValueError as error:
        raise ValueError(f"line {line}: {field} must be numeric") from error
    require(math.isfinite(value), f"line {line}: {field} must be finite")
    return value


def load_metrics(path: Path) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    with path.open("r", encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        require(reader.fieldnames == FIELDS, "unexpected proc metrics CSV header")
        raw_rows = list(reader)
    require(bool(raw_rows), "proc metrics CSV has no samples")

    rows: list[dict[str, Any]] = []
    previous_index = 0
    previous_uptime = -1.0
    status_counts = {status: 0 for status in sorted(VALID_STATUSES)}
    for line, raw in enumerate(raw_rows, start=2):
        status = raw["sample_status"]
        require(status in VALID_STATUSES,
                f"line {line}: invalid sample_status {status!r}")
        index = parse_int(raw, "sample_index", line)
        epoch_s = parse_int(raw, "epoch_s", line)
        uptime_s = parse_float(raw, "uptime_s", line)
        require(uptime_s >= 0.0, f"line {line}: uptime_s must be non-negative")
        require(index == previous_index + 1,
                f"line {line}: sample_index is not contiguous")
        require(uptime_s >= previous_uptime,
                f"line {line}: uptime_s moved backwards")
        previous_index = index
        previous_uptime = uptime_s
        status_counts[status] += 1

        parsed: dict[str, Any] = {
            "sample_index": index,
            "epoch_s": epoch_s,
            "uptime_s": uptime_s,
            "sample_status": status,
        }
        if status in {"OK", "IDENTITY_MISMATCH"}:
            for field in ("pid", "starttime_ticks", "proc_ticks", "total_ticks",
                          "vmrss_kb", "vmhwm_kb"):
                parsed[field] = parse_int(raw, field, line)
                require(parsed[field] >= 0,
                        f"line {line}: {field} must be non-negative")
            require(bool(raw["linux_state"]),
                    f"line {line}: linux_state is empty")
            parsed["linux_state"] = raw["linux_state"]
        rows.append(parsed)

    valid = [row for row in rows if row["sample_status"] == "OK"]
    require(len(valid) >= 2, "at least two OK samples are required")
    instances: list[tuple[int, int]] = []
    cpu_values: list[float] = []
    for current in valid:
        identity = (current["pid"], current["starttime_ticks"])
        if not instances or identity != instances[-1]:
            instances.append(identity)
    for previous, current in zip(valid, valid[1:]):
        if (previous["pid"], previous["starttime_ticks"]) != \
           (current["pid"], current["starttime_ticks"]):
            continue
        process_delta = current["proc_ticks"] - previous["proc_ticks"]
        total_delta = current["total_ticks"] - previous["total_ticks"]
        require(process_delta >= 0, "process CPU ticks moved backwards")
        require(total_delta > 0, "total CPU ticks did not increase")
        cpu_values.append(100.0 * process_delta / total_delta)
    require(bool(cpu_values), "no same-process CPU intervals are available")

    rss_values = [float(row["vmrss_kb"]) for row in valid]
    hwm_values = [float(row["vmhwm_kb"]) for row in valid]
    interval_values = [
        current["uptime_s"] - previous["uptime_s"]
        for previous, current in zip(rows, rows[1:])
    ]
    require(all(value > 0.0 for value in interval_values),
            "sample uptime intervals must be positive")

    def aggregate(values: list[float]) -> dict[str, float]:
        return {
            "average": rounded(sum(values) / len(values)),
            "p95_nearest_rank": rounded(nearest_rank(values, 0.95)),
            "maximum": rounded(max(values)),
        }

    summary = {
        "status": "PASS",
        "total_samples": len(rows),
        "ok_samples": len(valid),
        "missing_samples": status_counts["MISSING"],
        "identity_mismatch_samples": status_counts["IDENTITY_MISMATCH"],
        "read_error_samples": status_counts["READ_ERROR"],
        "unique_process_instances": len(instances),
        "pid_changes": max(0, len(instances) - 1),
        "observed_duration_seconds": rounded(
            rows[-1]["uptime_s"] - rows[0]["uptime_s"]
        ),
        "sample_interval_seconds": aggregate(interval_values),
        "cpu_percent_total_capacity": {
            **aggregate(cpu_values),
            "valid_intervals": len(cpu_values),
            "method": "100 * delta(proc utime+stime) / delta(sum /proc/stat cpu ticks)",
        },
        "vmrss_kb": aggregate(rss_values),
        "vmhwm_kb": aggregate(hwm_values),
    }
    return rows, summary


def render_markdown(input_path: Path, summary: dict[str, Any]) -> str:
    cpu = summary["cpu_percent_total_capacity"]
    rss = summary["vmrss_kb"]
    hwm = summary["vmhwm_kb"]
    interval = summary["sample_interval_seconds"]
    return f"""# /proc指标报告

- 输入：`{input_path}`
- 样本：{summary['ok_samples']}/{summary['total_samples']}个OK
- 观察时长：{summary['observed_duration_seconds']}秒（按`/proc/uptime`）
- 采样间隔秒平均/P95/最大：{interval['average']}/{interval['p95_nearest_rank']}/{interval['maximum']}
- 进程实例/PID变化：{summary['unique_process_instances']}/{summary['pid_changes']}
- 缺失/身份不符/读取错误：{summary['missing_samples']}/{summary['identity_mismatch_samples']}/{summary['read_error_samples']}
- CPU（总系统容量口径）平均/P95/最大：{cpu['average']}/{cpu['p95_nearest_rank']}/{cpu['maximum']}%
- VmRSS KiB平均/P95/最大：{rss['average']}/{rss['p95_nearest_rank']}/{rss['maximum']}
- VmHWM KiB平均/P95/最大：{hwm['average']}/{hwm['p95_nearest_rank']}/{hwm['maximum']}

CPU按相邻同一PID和starttime样本的`utime+stime`增量除以`/proc/stat`总tick增量计算；
P95采用nearest-rank。该报告只汇总输入证据，不自行证明目标环境、负载或运行时长。
"""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--summary-json", required=True, type=Path)
    parser.add_argument("--report-md", required=True, type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    require(not args.summary_json.exists(),
            f"refusing to overwrite {args.summary_json}")
    require(not args.report_md.exists(), f"refusing to overwrite {args.report_md}")
    _, summary = load_metrics(args.input)
    args.summary_json.write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    args.report_md.write_text(render_markdown(args.input, summary),
                              encoding="utf-8")
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(f"ERROR: {error}", file=__import__("sys").stderr)
        raise SystemExit(1)
