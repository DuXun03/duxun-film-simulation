"""Summarize DuXun Film Simulation GPU render logs.

The plugin writes key=value lines to DuXunFilmSim-gpu.log. This tool keeps the
analysis local and deterministic so performance regressions can be checked
without opening Resolve.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
from statistics import median
from typing import Any


LOG_NAME = "DuXunFilmSim-gpu.log"


def default_log_path() -> Path:
    temp_dir = os.environ.get("TEMP") or os.environ.get("TMP") or "."
    return Path(temp_dir) / LOG_NAME


def parse_value(value: str) -> Any:
    if value in {"0", "1"}:
        return int(value)
    try:
        if "." in value:
            return float(value)
        return int(value)
    except ValueError:
        return value


def classify_size(width: int, height: int) -> str:
    pixels = width * height
    if pixels <= 20_000:
        return "thumbnail"
    if pixels <= 600_000:
        return "preview"
    if pixels >= 8_000_000:
        return "full_4k"
    return "full"


def parse_log_line(line: str) -> dict[str, Any] | None:
    fields: dict[str, Any] = {}
    for part in line.strip().split():
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        fields[key] = parse_value(value)
    if not fields.get("phase"):
        return None

    width = int(fields.get("width", 0) or 0)
    height = int(fields.get("height", 0) or 0)
    fields.setdefault("pixels", width * height)
    fields.setdefault("sizeClass", classify_size(width, height))
    elapsed = fields.get("elapsedMs")
    if elapsed is None or elapsed == -1:
        fields["elapsedMs"] = None
    for timing_key in ("setupMs", "kernelMs"):
        value = fields.get(timing_key)
        if value is None or value == -1:
            fields[timing_key] = None
    fields["coldStart"] = 1 if fields.get("coldStart") == 1 else 0
    return fields


def parse_log_lines(lines: list[str]) -> list[dict[str, Any]]:
    return [parsed for line in lines if (parsed := parse_log_line(line)) is not None]


def phase_bucket(phase: str) -> str | None:
    if phase == "cuda_success":
        return "success"
    if phase == "cuda_fallback":
        return "fallback"
    return None


def effect_key(attempt: dict[str, Any]) -> str:
    effects = []
    if attempt.get("doHalation"):
        effects.append("halation")
    if attempt.get("doBloom"):
        effects.append("bloom")
    if attempt.get("doDamage"):
        effects.append("damage")
    if attempt.get("doSpatial"):
        effects.append("spatial")
    return "+".join(effects) if effects else "base"


def percentile(values: list[float], pct: float) -> float | None:
    if not values:
        return None
    if len(values) == 1:
        return values[0]
    ordered = sorted(values)
    index = int(round((len(ordered) - 1) * pct))
    return ordered[index]


def timing_stats(values: list[float]) -> dict[str, Any]:
    if not values:
        return {"timedCount": 0, "avgMs": None, "p50Ms": None, "p95Ms": None, "maxMs": None}
    return {
        "timedCount": len(values),
        "avgMs": round(sum(values) / len(values), 3),
        "p50Ms": round(float(median(values)), 3),
        "p95Ms": round(float(percentile(values, 0.95)), 3),
        "maxMs": round(max(values), 3),
    }


def add_attempt(group: dict[str, Any], bucket: str, attempt: dict[str, Any]) -> None:
    entry = group.setdefault(bucket, {"count": 0, "_elapsed": [], "_setup": [], "_kernel": [], "coldStartCount": 0})
    entry["count"] += 1
    elapsed = attempt.get("elapsedMs")
    if isinstance(elapsed, (int, float)) and elapsed >= 0:
        entry["_elapsed"].append(float(elapsed))
    setup = attempt.get("setupMs")
    if isinstance(setup, (int, float)) and setup >= 0:
        entry["_setup"].append(float(setup))
    kernel = attempt.get("kernelMs")
    if isinstance(kernel, (int, float)) and kernel >= 0:
        entry["_kernel"].append(float(kernel))
    if attempt.get("coldStart") == 1:
        entry["coldStartCount"] += 1


def finalize_group(group: dict[str, Any]) -> dict[str, Any]:
    finalized: dict[str, Any] = {}
    for bucket, entry in sorted(group.items()):
        stats = timing_stats(entry.get("_elapsed", []))
        setup_stats = timing_stats(entry.get("_setup", []))
        kernel_stats = timing_stats(entry.get("_kernel", []))
        finalized[bucket] = {
            "count": entry["count"],
            **stats,
            "setupTimedCount": setup_stats["timedCount"],
            "avgSetupMs": setup_stats["avgMs"],
            "p50SetupMs": setup_stats["p50Ms"],
            "p95SetupMs": setup_stats["p95Ms"],
            "maxSetupMs": setup_stats["maxMs"],
            "kernelTimedCount": kernel_stats["timedCount"],
            "avgKernelMs": kernel_stats["avgMs"],
            "p50KernelMs": kernel_stats["p50Ms"],
            "p95KernelMs": kernel_stats["p95Ms"],
            "maxKernelMs": kernel_stats["maxMs"],
            "coldStartCount": entry.get("coldStartCount", 0),
        }
    return finalized


def summarize_attempts(attempts: list[dict[str, Any]]) -> dict[str, Any]:
    total_success = 0
    total_fallback = 0
    fallback_reasons: dict[str, int] = {}
    by_size: dict[str, dict[str, Any]] = {}
    by_effects: dict[str, dict[str, Any]] = {}

    for attempt in attempts:
        bucket = phase_bucket(str(attempt.get("phase", "")))
        if not bucket:
            continue
        if bucket == "success":
            total_success += 1
        elif bucket == "fallback":
            total_fallback += 1
            reason = str(attempt.get("reason", "unknown") or "unknown")
            fallback_reasons[reason] = fallback_reasons.get(reason, 0) + 1

        add_attempt(by_size.setdefault(str(attempt.get("sizeClass", "unknown")), {}), bucket, attempt)
        add_attempt(by_effects.setdefault(effect_key(attempt), {}), bucket, attempt)

    completed = total_success + total_fallback
    return {
        "total": {
            "completed": completed,
            "success": total_success,
            "fallback": total_fallback,
            "fallbackRate": round(total_fallback / completed, 4) if completed else 0.0,
        },
        "fallbackReasons": dict(sorted(fallback_reasons.items())),
        "bySize": {key: finalize_group(value) for key, value in sorted(by_size.items())},
        "byEffects": {key: finalize_group(value) for key, value in sorted(by_effects.items())},
    }


def format_ms(value: Any) -> str:
    return "-" if value is None else f"{float(value):.3f}"


def format_group(title: str, group: dict[str, Any]) -> list[str]:
    lines = [title]
    if not group:
        return lines + ["  none"]
    for name, buckets in group.items():
        for bucket, stats in buckets.items():
            lines.append(
                "  "
                f"{name} {bucket} count={stats['count']} timed={stats['timedCount']} "
                f"avgMs={format_ms(stats['avgMs'])} p50Ms={format_ms(stats['p50Ms'])} "
                f"p95Ms={format_ms(stats['p95Ms'])} maxMs={format_ms(stats['maxMs'])} "
                f"setupTimed={stats['setupTimedCount']} setupAvgMs={format_ms(stats['avgSetupMs'])} "
                f"kernelTimed={stats['kernelTimedCount']} kernelAvgMs={format_ms(stats['avgKernelMs'])} "
                f"coldStart={stats['coldStartCount']}"
            )
    return lines


def format_summary(summary: dict[str, Any]) -> str:
    total = summary["total"]
    lines = [
        "GPU Render Summary",
        (
            f"completed={total['completed']} success={total['success']} "
            f"fallback={total['fallback']} fallbackRate={total['fallbackRate'] * 100:.1f}%"
        ),
    ]
    lines.extend(format_group("By size", summary["bySize"]))
    lines.extend(format_group("By effects", summary["byEffects"]))
    lines.append("Fallback reasons")
    if summary["fallbackReasons"]:
        for reason, count in summary["fallbackReasons"].items():
            lines.append(f"  {reason}: {count}")
    else:
        lines.append("  none")
    return "\n".join(lines)


def load_attempts(path: Path, since_line: int = 0) -> list[dict[str, Any]]:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    return parse_log_lines(lines[since_line:])


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Summarize DuXun Film Simulation GPU logs.")
    parser.add_argument("log", nargs="?", type=Path, default=default_log_path(), help="Path to DuXunFilmSim-gpu.log")
    parser.add_argument("--json", action="store_true", help="Print machine-readable JSON")
    parser.add_argument("--since-line", type=int, default=0, help="Skip this many existing log lines before analyzing")
    args = parser.parse_args(argv)

    if not args.log.exists():
        raise SystemExit(f"GPU log not found: {args.log}")
    if args.since_line < 0:
        raise SystemExit("--since-line must be >= 0")
    summary = summarize_attempts(load_attempts(args.log, since_line=args.since_line))
    if args.json:
        print(json.dumps(summary, indent=2, sort_keys=True))
    else:
        print(format_summary(summary))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
