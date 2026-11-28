#!/usr/bin/env python3
"""Run the required cache-policy comparisons and create CSV/SVG results."""

import csv
import subprocess
import sys
from collections import Counter
from pathlib import Path
from xml.sax.saxutils import escape


ROOT = Path(__file__).resolve().parent
WORKLOADS = {
    "sequential": ROOT / "requests_sequential.csv",
    "repetitive": ROOT / "requests_repetitive.csv",
    "mixed": ROOT / "requests_mixed.csv",
}
POLICIES = ["fifo", "lru", "lfu", "custom"]
CAPACITIES = [
    ("100 KiB", 100 * 1024),
    ("500 KiB", 500 * 1024),
    ("1 MiB", 1024 * 1024),
    ("2 MiB", 2 * 1024 * 1024),
    ("5 MiB", 5 * 1024 * 1024),
    ("10 MiB", 10 * 1024 * 1024),
]
COLORS = {
    "fifo": "#2563eb",
    "lru": "#16a34a",
    "lfu": "#ea580c",
    "custom": "#9333ea",
}


def read_rows(path):
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def write_rows(path, rows, fields):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def resource_sizes():
    resources = read_rows(ROOT / "resources.csv")
    return {
        (r["scheme"], r["host"], r["path"]): int(r["size_bytes"])
        for r in resources
    }


def characterize(sizes):
    result = []
    for workload, path in WORKLOADS.items():
        requests = read_rows(path)
        counts = Counter()
        clients = set()
        total_bytes = 0
        oversized_counts = {label: 0 for label, _ in CAPACITIES}
        for request in requests:
            key = (request["scheme"], request["host"], request["path"])
            if key not in sizes:
                raise ValueError(f"Resource not found in metadata: {key}")
            size = sizes[key]
            counts[key] += 1
            clients.add(request["client_id"])
            total_bytes += size
            for label, capacity in CAPACITIES:
                if size > capacity:
                    oversized_counts[label] += 1

        top = "; ".join(
            f"{host}{path} ({count})"
            for (_, host, path), count in counts.most_common(5)
        )
        row = {
            "workload": workload,
            "request_count": len(requests),
            "distinct_resources": len(counts),
            "distinct_clients": len(clients),
            "total_requested_bytes": total_bytes,
            "top_5_resources": top,
        }
        for label, _ in CAPACITIES:
            row[f"oversized_at_{label.replace(' ', '_')}"] = oversized_counts[label]
        result.append(row)
    return result


def resource_distribution(sizes):
    distribution = []
    for workload, path in WORKLOADS.items():
        requests = read_rows(path)
        counts = Counter(
            (row["scheme"], row["host"], row["path"])
            for row in requests
        )
        total = len(requests)
        for (scheme, host, path), count in counts.most_common():
            size = sizes[(scheme, host, path)]
            distribution.append({
                "workload": workload,
                "resource": f"{scheme}://{host}{path}",
                "request_count": count,
                "request_share_percent": f"{100.0 * count / total:.2f}",
                "resource_size_bytes": size,
                "requested_bytes_for_resource": count * size,
            })
    return distribution


def run_all(output_dir):
    binary = ROOT / "cache_sim"
    if not binary.exists():
        raise RuntimeError("Build cache_sim first with: make")
    sizes = resource_sizes()
    run_dir = output_dir / "run_metrics"
    run_dir.mkdir(parents=True, exist_ok=True)

    summary = []
    mixed_clients = []
    for workload, request_path in WORKLOADS.items():
        for capacity_label, capacity in CAPACITIES:
            for policy in POLICIES:
                metrics_path = run_dir / (
                    f"{workload}_{capacity_label.replace(' ', '')}_{policy}.csv"
                )
                completed = subprocess.run(
                    [
                        str(binary), "simulate",
                        str(ROOT / "resources.csv"),
                        str(request_path),
                        str(capacity),
                        policy,
                        "-",  # No detailed 3,000-row trace needed for each experiment.
                        str(metrics_path),
                    ],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.PIPE,
                    text=True,
                )
                if completed.returncode != 0:
                    raise RuntimeError(
                        f"{workload}/{capacity_label}/{policy} failed:\n"
                        f"{completed.stderr}"
                    )

                rows = read_rows(metrics_path)
                overall = {
                    key: value for key, value in rows[0].items()
                    if key != "client_id"
                }
                summary.append({
                    "workload": workload,
                    "capacity_label": capacity_label,
                    "capacity_bytes": capacity,
                    "policy": policy,
                    **overall,
                })
                if workload == "mixed" and capacity == 5 * 1024 * 1024:
                    for client_row in rows[1:]:
                        mixed_clients.append({
                            "capacity_label": capacity_label,
                            "policy": policy,
                            **client_row,
                        })

    summary_fields = [
        "workload", "capacity_label", "capacity_bytes", "policy",
        "total_requests", "hits", "misses", "total_bytes", "hit_bytes",
        "origin_bytes", "hit_ratio_percent", "byte_hit_ratio_percent",
        "average_response_time_ms", "evictions", "oversized_requests",
        "average_cache_utilization_percent",
    ]
    write_rows(output_dir / "experiment_summary.csv", summary, summary_fields)
    write_rows(
        output_dir / "mixed_clients_5MiB.csv",
        mixed_clients,
        ["capacity_label", "policy", "client_id", "total_requests", "hits",
         "misses", "total_bytes", "hit_bytes", "origin_bytes",
         "hit_ratio_percent", "byte_hit_ratio_percent",
         "average_response_time_ms", "evictions", "oversized_requests",
         "average_cache_utilization_percent"],
    )
    workload_rows = characterize(sizes)
    workload_fields = list(workload_rows[0].keys())
    write_rows(output_dir / "workload_characterization.csv",
               workload_rows, workload_fields)
    distribution = resource_distribution(sizes)
    write_rows(
        output_dir / "resource_frequency.csv",
        distribution,
        ["workload", "resource", "request_count",
         "request_share_percent", "resource_size_bytes",
         "requested_bytes_for_resource"],
    )
    return summary, mixed_clients, distribution


def svg_text(x, y, value, size=14, anchor="middle", color="#1f2937",
             weight="normal", rotate=None):
    transform = f' transform="rotate({rotate} {x} {y})"' if rotate else ""
    return (
        f'<text x="{x}" y="{y}" text-anchor="{anchor}" '
        f'font-family="Arial, sans-serif" font-size="{size}" '
        f'font-weight="{weight}" fill="{color}"{transform}>'
        f'{escape(str(value))}</text>'
    )


def chart_hit_ratio(summary, destination):
    width, height = 960, 970
    left, right = 92, 920
    panel_h = 270
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        svg_text(width / 2, 32, "Cache hit ratio by cache capacity", 22, weight="bold"),
    ]
    labels = [label for label, _ in CAPACITIES]
    for panel, workload in enumerate(WORKLOADS):
        top = 72 + panel * 292
        bottom = top + 220
        x_pos = [left + i * (right - left) / (len(labels) - 1)
                 for i in range(len(labels))]
        parts.append(svg_text(width / 2, top - 12, workload.title(), 17, weight="bold"))
        for tick in range(0, 101, 20):
            y = bottom - tick * 2
            parts.append(f'<line x1="{left}" y1="{y}" x2="{right}" y2="{y}" '
                         'stroke="#d1d5db" stroke-width="1"/>')
            parts.append(svg_text(left - 12, y + 5, tick, 12, anchor="end"))
        for x, label in zip(x_pos, labels):
            parts.append(svg_text(x, bottom + 22, label, 12))
        for policy in POLICIES:
            values = [
                float(next(r for r in summary
                           if r["workload"] == workload
                           and r["capacity_label"] == label
                           and r["policy"] == policy)["hit_ratio_percent"])
                for label in labels
            ]
            points = [
                (x_pos[i], bottom - values[i] * 2)
                for i in range(len(values))
            ]
            point_string = " ".join(f"{x},{y}" for x, y in points)
            parts.append(
                f'<polyline points="{point_string}" fill="none" '
                f'stroke="{COLORS[policy]}" stroke-width="3"/>'
            )
            for x, y in points:
                parts.append(f'<circle cx="{x}" cy="{y}" r="4" fill="{COLORS[policy]}"/>')
        if panel == 0:
            for i, policy in enumerate(POLICIES):
                x = 190 + i * 160
                parts.append(f'<line x1="{x}" y1="51" x2="{x+22}" y2="51" '
                             f'stroke="{COLORS[policy]}" stroke-width="3"/>')
                parts.append(svg_text(x + 30, 56, policy.upper(), 12, anchor="start"))
    parts.append(svg_text(28, height / 2, "Hit ratio (%)", 14, rotate=-90))
    parts.append("</svg>")
    destination.write_text("\n".join(parts), encoding="utf-8")


def chart_response_time(summary, destination):
    width, height = 960, 560
    left, right, top, bottom = 100, 930, 70, 455
    rows = [r for r in summary if int(r["capacity_bytes"]) == 5 * 1024 * 1024]
    max_value = max(float(r["average_response_time_ms"]) for r in rows)
    y_max = max(50, int((max_value + 49) // 50) * 50)
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        svg_text(width/2, 32, "Average response time at 5 MiB", 22, weight="bold"),
    ]
    for tick in range(0, y_max + 1, 50):
        y = bottom - tick / y_max * (bottom - top)
        parts.append(f'<line x1="{left}" y1="{y}" x2="{right}" y2="{y}" '
                     'stroke="#d1d5db"/>')
        parts.append(svg_text(left - 12, y + 5, tick, 12, anchor="end"))
    workload_names = list(WORKLOADS)
    group_width = (right - left) / len(workload_names)
    bar_width = 30
    for g, workload in enumerate(workload_names):
        center = left + group_width * (g + 0.5)
        parts.append(svg_text(center, bottom + 27, workload.title(), 14))
        for p, policy in enumerate(POLICIES):
            row = next(r for r in rows
                       if r["workload"] == workload and r["policy"] == policy)
            value = float(row["average_response_time_ms"])
            x = center + (p - 1.5) * (bar_width + 5) - bar_width / 2
            h = value / y_max * (bottom - top)
            y = bottom - h
            parts.append(f'<rect x="{x}" y="{y}" width="{bar_width}" height="{h}" '
                         f'fill="{COLORS[policy]}"/>')
            parts.append(svg_text(x + bar_width/2, y - 6, f"{value:.1f}", 10))
    for i, policy in enumerate(POLICIES):
        x = 220 + i * 160
        parts.append(f'<rect x="{x}" y="48" width="15" height="15" fill="{COLORS[policy]}"/>')
        parts.append(svg_text(x + 21, 61, policy.upper(), 12, anchor="start"))
    parts.append(svg_text(28, height/2, "Response time (ms)", 14, rotate=-90))
    parts.append("</svg>")
    destination.write_text("\n".join(parts), encoding="utf-8")


def chart_mixed_clients(clients, destination):
    width, height = 1120, 600
    left, right, top, bottom = 90, 1080, 80, 500
    max_value = 100
    names = sorted({row["client_id"] for row in clients})
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        svg_text(width/2, 34, "Mixed workload: per-client hit ratio at 5 MiB",
                 21, weight="bold"),
    ]
    for tick in range(0, 101, 20):
        y = bottom - tick / max_value * (bottom - top)
        parts.append(f'<line x1="{left}" y1="{y}" x2="{right}" y2="{y}" '
                     'stroke="#d1d5db"/>')
        parts.append(svg_text(left - 12, y + 5, tick, 12, anchor="end"))
    group_width = (right - left) / len(names)
    bar_width = 14
    for g, name in enumerate(names):
        center = left + group_width * (g + 0.5)
        parts.append(svg_text(center, bottom + 24, name, 11))
        for p, policy in enumerate(POLICIES):
            row = next(r for r in clients
                       if r["client_id"] == name and r["policy"] == policy)
            value = float(row["hit_ratio_percent"])
            x = center + (p - 1.5) * (bar_width + 3) - bar_width / 2
            h = value / max_value * (bottom - top)
            parts.append(f'<rect x="{x}" y="{bottom-h}" width="{bar_width}" '
                         f'height="{h}" fill="{COLORS[policy]}"/>')
    for i, policy in enumerate(POLICIES):
        x = 350 + i * 140
        parts.append(f'<rect x="{x}" y="52" width="15" height="15" fill="{COLORS[policy]}"/>')
        parts.append(svg_text(x + 21, 65, policy.upper(), 12, anchor="start"))
    parts.append(svg_text(28, height/2, "Hit ratio (%)", 14, rotate=-90))
    parts.append("</svg>")
    destination.write_text("\n".join(parts), encoding="utf-8")


def chart_workload_profiles(distribution, destination):
    width, height = 960, 560
    left, right, top, bottom = 100, 930, 80, 455
    workload_names = list(WORKLOADS)
    rank_values = {}
    for workload in workload_names:
        rows = [r for r in distribution if r["workload"] == workload]
        rows.sort(key=lambda row: int(row["request_count"]), reverse=True)
        rank_values[workload] = [
            int(row["request_count"]) for row in rows[:5]
        ]

    y_max_value = max(max(values) for values in rank_values.values())
    y_max = max(100, ((y_max_value + 99) // 100) * 100)
    colors = {
        "sequential": "#2563eb",
        "repetitive": "#ea580c",
        "mixed": "#16a34a",
    }
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        svg_text(width/2, 34, "Top-resource request counts by workload",
                 21, weight="bold"),
    ]
    tick_step = 100
    for tick in range(0, y_max + 1, tick_step):
        y = bottom - tick / y_max * (bottom - top)
        parts.append(f'<line x1="{left}" y1="{y}" x2="{right}" y2="{y}" '
                     'stroke="#d1d5db"/>')
        parts.append(svg_text(left - 12, y + 5, tick, 12, anchor="end"))
    group_width = (right - left) / 5
    bar_width = 24
    for rank in range(5):
        center = left + group_width * (rank + 0.5)
        parts.append(svg_text(center, bottom + 24, f"Rank {rank + 1}", 12))
        for w, workload in enumerate(workload_names):
            value = rank_values[workload][rank]
            x = center + (w - 1) * (bar_width + 4) - bar_width / 2
            bar_height = value / y_max * (bottom - top)
            parts.append(
                f'<rect x="{x}" y="{bottom-bar_height}" width="{bar_width}" '
                f'height="{bar_height}" fill="{colors[workload]}"/>'
            )
            parts.append(svg_text(x + bar_width/2, bottom-bar_height-5,
                                  value, 10))
    for i, workload in enumerate(workload_names):
        x = 320 + i * 150
        parts.append(f'<rect x="{x}" y="52" width="15" height="15" '
                     f'fill="{colors[workload]}"/>')
        parts.append(svg_text(x + 21, 65, workload.title(), 12, anchor="start"))
    parts.append(svg_text(28, height/2, "Requests", 14, rotate=-90))
    parts.append("</svg>")
    destination.write_text("\n".join(parts), encoding="utf-8")


def main():
    output_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "results"
    output_dir.mkdir(parents=True, exist_ok=True)
    summary, clients, distribution = run_all(output_dir)
    chart_hit_ratio(summary, output_dir / "hit_ratio_vs_capacity.svg")
    chart_response_time(summary, output_dir / "response_time_5MiB.svg")
    chart_mixed_clients(clients, output_dir / "mixed_client_hit_ratio_5MiB.svg")
    chart_workload_profiles(
        distribution, output_dir / "workload_profiles.svg"
    )
    print(f"Experiments complete. Results saved in: {output_dir}")


if __name__ == "__main__":
    main()
