#!/usr/bin/env python3

import argparse
import csv
import json
import os
import subprocess
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402


SCHEDULERS = ("round-robin", "proportional-fair")
DISPLAY_NAMES = {
    "round-robin": "Round Robin",
    "proportional-fair": "Proportional Fair",
}
EXPECTED_RESULT_FILES = ("summary.json", "per_ue.csv", "per_slot.csv")


class ComparisonError(RuntimeError):
    pass


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run and compare the simulator's two scheduling algorithms."
    )
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    return parser.parse_args()


def validate_inputs(executable: Path, config: Path) -> None:
    if not executable.is_file():
        raise ComparisonError(f"simulator executable does not exist: {executable}")
    if not os.access(executable, os.X_OK):
        raise ComparisonError(f"simulator file is not executable: {executable}")
    if not config.is_file():
        raise ComparisonError(f"configuration file does not exist: {config}")


def run_simulator(
    executable: Path, config: Path, scheduler: str, output_directory: Path
) -> dict:
    output_directory.mkdir(parents=True, exist_ok=True)
    for filename in EXPECTED_RESULT_FILES:
        (output_directory / filename).unlink(missing_ok=True)

    command = [
        str(executable),
        "--config",
        str(config),
        "--scheduler",
        scheduler,
        "--output-dir",
        str(output_directory),
    ]
    completed = subprocess.run(command, capture_output=True, text=True, check=False)
    if completed.returncode != 0:
        raise ComparisonError(
            f"simulator failed for {scheduler} with exit code {completed.returncode}\n"
            f"standard output:\n{completed.stdout}\n"
            f"standard error:\n{completed.stderr}"
        )

    for filename in EXPECTED_RESULT_FILES:
        result_path = output_directory / filename
        if not result_path.is_file():
            raise ComparisonError(
                f"simulator did not create expected result file: {result_path}"
            )

    summary_path = output_directory / "summary.json"
    try:
        with summary_path.open(encoding="utf-8") as summary_file:
            summary = json.load(summary_file)
    except (OSError, json.JSONDecodeError) as error:
        raise ComparisonError(f"could not read {summary_path}: {error}") from error

    if summary.get("scheduler_name") != scheduler:
        raise ComparisonError(
            f"{summary_path} reports scheduler {summary.get('scheduler_name')!r}, "
            f"expected {scheduler!r}"
        )
    return summary


def completed_packet_metrics(summary: dict) -> tuple[int, float]:
    completed_packets = sum(ue["completed_packets"] for ue in summary["per_ue"])
    if completed_packets == 0:
        return 0, 0.0

    total_latency = sum(
        ue["completed_packets"] * ue["average_completed_packet_latency_slots"]
        for ue in summary["per_ue"]
    )
    return completed_packets, total_latency / completed_packets


def sorted_ue_ids(summaries: dict[str, dict]) -> list[int]:
    ue_id_sets = [
        {ue["ue_id"] for ue in summary["per_ue"]} for summary in summaries.values()
    ]
    if not ue_id_sets or any(ue_ids != ue_id_sets[0] for ue_ids in ue_id_sets[1:]):
        raise ComparisonError("scheduler summaries contain different UE IDs")
    return sorted(ue_id_sets[0])


def summary_row(summary: dict, ue_ids: list[int]) -> dict:
    completed_packets, average_latency = completed_packet_metrics(summary)
    per_ue = {ue["ue_id"]: ue for ue in summary["per_ue"]}
    row = {
        "scheduler": summary["scheduler_name"],
        "total_transmitted_bytes": summary["total_transmitted_bytes"],
        "total_dropped_bytes": summary["total_dropped_bytes"],
        "total_completed_packets": completed_packets,
        "average_completed_packet_latency_slots": average_latency,
        "resource_block_utilization": summary["resource_block_utilization"],
        "jains_fairness_index": summary["jains_fairness_index"],
    }
    for ue_id in ue_ids:
        row[f"ue_{ue_id}_average_throughput_bytes_per_slot"] = per_ue[ue_id][
            "average_throughput_bytes_per_slot"
        ]
    return row


def write_comparison_csv(
    output_path: Path, summaries: dict[str, dict], ue_ids: list[int]
) -> None:
    rows = [summary_row(summaries[scheduler], ue_ids) for scheduler in SCHEDULERS]
    fieldnames = list(rows[0].keys())
    with output_path.open("w", encoding="utf-8", newline="") as output_file:
        writer = csv.DictWriter(
            output_file, fieldnames=fieldnames, lineterminator="\n"
        )
        writer.writeheader()
        writer.writerows(rows)


def create_chart(output_path: Path, summaries: dict[str, dict], ue_ids: list[int]) -> None:
    labels = [DISPLAY_NAMES[scheduler] for scheduler in SCHEDULERS]
    colors = ("#4C78A8", "#F58518")
    figure, axes = plt.subplots(2, 2, figsize=(12, 8))
    figure.suptitle("Congested Scenario: Round Robin vs Proportional Fair", fontsize=14)

    width = 0.35
    transmitted = [summaries[name]["total_transmitted_bytes"] for name in SCHEDULERS]
    dropped = [summaries[name]["total_dropped_bytes"] for name in SCHEDULERS]
    traffic_positions = [0, 1]
    for index, scheduler in enumerate(SCHEDULERS):
        offset = -width / 2 if index == 0 else width / 2
        axes[0, 0].bar(
            [position + offset for position in traffic_positions],
            [transmitted[index], dropped[index]],
            width,
            label=DISPLAY_NAMES[scheduler],
            color=colors[index],
        )
    axes[0, 0].set_xticks(traffic_positions, ["Transmitted", "Dropped"])
    axes[0, 0].set_ylabel("Bytes")
    axes[0, 0].set_title("Traffic outcome")
    axes[0, 0].set_ylim(bottom=0)
    axes[0, 0].ticklabel_format(axis="y", style="plain")
    axes[0, 0].legend()

    ue_positions = list(range(len(ue_ids)))
    for index, scheduler in enumerate(SCHEDULERS):
        throughput_by_ue = {
            ue["ue_id"]: ue["average_throughput_bytes_per_slot"]
            for ue in summaries[scheduler]["per_ue"]
        }
        offset = -width / 2 if index == 0 else width / 2
        axes[0, 1].bar(
            [position + offset for position in ue_positions],
            [throughput_by_ue[ue_id] for ue_id in ue_ids],
            width,
            label=DISPLAY_NAMES[scheduler],
            color=colors[index],
        )
    axes[0, 1].set_xticks(ue_positions, [f"UE {ue_id}" for ue_id in ue_ids])
    axes[0, 1].set_ylabel("Average throughput (bytes/slot)")
    axes[0, 1].set_title("Per-UE throughput")
    axes[0, 1].set_ylim(bottom=0)
    axes[0, 1].legend()

    metric_positions = [0, 1]
    for index, scheduler in enumerate(SCHEDULERS):
        offset = -width / 2 if index == 0 else width / 2
        axes[1, 0].bar(
            [position + offset for position in metric_positions],
            [
                summaries[scheduler]["jains_fairness_index"],
                summaries[scheduler]["resource_block_utilization"],
            ],
            width,
            label=DISPLAY_NAMES[scheduler],
            color=colors[index],
        )
    axes[1, 0].set_xticks(metric_positions, ["Jain fairness", "RB utilization"])
    axes[1, 0].set_ylabel("Ratio")
    axes[1, 0].set_title("Fairness and utilization")
    axes[1, 0].set_ylim(0, 1)
    axes[1, 0].legend()

    average_latencies = [completed_packet_metrics(summaries[name])[1] for name in SCHEDULERS]
    axes[1, 1].bar(labels, average_latencies, color=colors)
    axes[1, 1].set_ylabel("Slots")
    axes[1, 1].set_title("Completion-weighted average latency")
    axes[1, 1].set_ylim(bottom=0)

    figure.tight_layout(rect=(0, 0, 1, 0.95))
    figure.savefig(output_path, dpi=150)
    plt.close(figure)


def print_comparison_table(summaries: dict[str, dict]) -> None:
    header = (
        f"{'Scheduler':<20} {'Tx bytes':>10} {'Dropped':>10} {'Completed':>10} "
        f"{'Avg latency':>12} {'Fairness':>10} {'Utilization':>12}"
    )
    print(header)
    print("-" * len(header))
    for scheduler in SCHEDULERS:
        summary = summaries[scheduler]
        completed_packets, average_latency = completed_packet_metrics(summary)
        print(
            f"{DISPLAY_NAMES[scheduler]:<20} "
            f"{summary['total_transmitted_bytes']:>10,d} "
            f"{summary['total_dropped_bytes']:>10,d} "
            f"{completed_packets:>10,d} "
            f"{average_latency:>12.3f} "
            f"{summary['jains_fairness_index']:>10.4f} "
            f"{summary['resource_block_utilization']:>12.4f}"
        )


def main() -> int:
    arguments = parse_arguments()
    executable = arguments.executable.expanduser().resolve()
    config = arguments.config.expanduser().resolve()
    output_directory = arguments.output_dir.expanduser().resolve()

    try:
        validate_inputs(executable, config)
        output_directory.mkdir(parents=True, exist_ok=True)
        summaries = {
            scheduler: run_simulator(
                executable, config, scheduler, output_directory / scheduler
            )
            for scheduler in SCHEDULERS
        }
        ue_ids = sorted_ue_ids(summaries)
        comparison_path = output_directory / "comparison.csv"
        chart_path = output_directory / "scheduler_comparison.png"
        write_comparison_csv(comparison_path, summaries, ue_ids)
        create_chart(chart_path, summaries, ue_ids)
        print_comparison_table(summaries)
        print(f"\nComparison CSV: {comparison_path}")
        print(f"Comparison chart: {chart_path}")
        return 0
    except (ComparisonError, KeyError, OSError, TypeError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
