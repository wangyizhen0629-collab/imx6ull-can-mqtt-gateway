#!/usr/bin/env python3
"""生成或核对M10的三个Keil编译期流量profile target。"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


TARGETS = (
    ("M10_111", "gateway_m10_111", "111"),
    ("M10_500", "gateway_m10_500", "500"),
    ("M10_1000", "gateway_m10_1000", "1000"),
)
TARGET_PATTERN = re.compile(r"    <Target>.*?    </Target>", re.DOTALL)


def require_single(text: str, old: str, new: str) -> str:
    if text.count(old) != 1:
        raise ValueError(f"expected exactly one occurrence: {old}")
    return text.replace(old, new, 1)


def canonical_base(block: str) -> str:
    name_match = re.search(r"<TargetName>([^<]+)</TargetName>", block)
    output_match = re.search(r"<OutputName>([^<]+)</OutputName>", block)
    define_match = re.search(
        r"<Define>USE_HAL_DRIVER,STM32F103xB"
        r"(?:,ECU_TRAFFIC_PROFILE=(?:111|500|1000))?</Define>",
        block,
    )
    directory_match = re.search(
        r"<OutputDirectory>([^<]+)</OutputDirectory>", block
    )
    if not all((name_match, output_match, define_match, directory_match)):
        raise ValueError("Keil target is missing an expected identity field")

    block = require_single(
        block, name_match.group(0), "<TargetName>@TARGET@</TargetName>"
    )
    block = require_single(
        block,
        directory_match.group(0),
        "<OutputDirectory>@TARGET@\\</OutputDirectory>",
    )
    block = require_single(
        block, output_match.group(0), "<OutputName>@OUTPUT@</OutputName>"
    )
    block = require_single(
        block,
        define_match.group(0),
        "<Define>USE_HAL_DRIVER,STM32F103xB,"
        "ECU_TRAFFIC_PROFILE=@RATE@</Define>",
    )
    return block


def render_target(base: str, target: str, output: str, rate: str) -> str:
    return (
        base.replace("@TARGET@", target)
        .replace("@OUTPUT@", output)
        .replace("@RATE@", rate)
    )


def expected_project(text: str) -> str:
    blocks = TARGET_PATTERN.findall(text)
    if len(blocks) not in {1, 3}:
        raise ValueError(f"expected one or three Keil targets, got {len(blocks)}")
    base = canonical_base(blocks[0])
    rendered = "\n".join(
        render_target(base, target, output, rate)
        for target, output, rate in TARGETS
    )
    return text[: text.index(blocks[0])] + rendered + text[
        text.index(blocks[-1]) + len(blocks[-1]) :
    ]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project", required=True, type=Path)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--write", action="store_true")
    action.add_argument("--check", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    original_bytes = args.project.read_bytes()
    text = original_bytes.decode("utf-8")
    expected = expected_project(text)
    expected_bytes = expected.encode("utf-8")

    if args.check:
        if original_bytes != expected_bytes:
            raise ValueError("Keil M10 target definitions are not canonical")
        print("PASS: canonical Keil targets M10_111/M10_500/M10_1000")
        return 0

    if original_bytes == expected_bytes:
        print("PASS: Keil M10 targets already generated")
        return 0
    args.project.write_bytes(expected_bytes)
    print("PASS: generated Keil targets M10_111/M10_500/M10_1000")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, UnicodeError, ValueError) as error:
        print(f"FAIL: {error}", file=__import__("sys").stderr)
        raise SystemExit(1)
