import csv
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {
    "fifo": {"hits": 3, "origin_bytes": 421888, "average_ms": 95.33},
    "lru": {"hits": 3, "origin_bytes": 403456, "average_ms": 92.33},
    "lfu": {"hits": 4, "origin_bytes": 370688, "average_ms": 84.33},
}


def read_csv(path):
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def main():
    with tempfile.TemporaryDirectory(prefix="cache-reference-") as temporary:
        output_dir = Path(temporary)
        for policy, expected_summary in EXPECTED.items():
            trace = output_dir / f"{policy}_trace.csv"
            metrics = output_dir / f"{policy}_metrics.csv"
            subprocess.run(
                [
                    str(ROOT / "cache_sim"),
                    "simulate",
                    str(ROOT / "resources.csv"),
                    str(ROOT / "requests_reference.csv"),
                    "102400",
                    policy,
                    str(trace),
                    str(metrics),
                ],
                check=True,
                stdout=subprocess.DEVNULL,
            )

            expected_rows = read_csv(ROOT / f"expected_{policy}.csv")
            actual_rows = read_csv(trace)
            if actual_rows != expected_rows:
                for index, (actual, expected) in enumerate(
                    zip(actual_rows, expected_rows), start=1
                ):
                    if actual != expected:
                        raise AssertionError(
                            f"{policy} trace differs at request {index}:\n"
                            f"expected {expected}\nactual   {actual}"
                        )
                raise AssertionError(
                    f"{policy} trace row count differs: "
                    f"{len(actual_rows)} != {len(expected_rows)}"
                )

            overall = read_csv(metrics)[0]
            assert int(overall["total_requests"]) == 15
            assert int(overall["hits"]) == expected_summary["hits"]
            assert int(overall["origin_bytes"]) == expected_summary["origin_bytes"]
            expected_evictions = sum(
                0 if row["evicted"] == "-" else len(row["evicted"].split(","))
                for row in expected_rows
            )
            expected_utilization = sum(
                float(row["cache_used_KiB"]) / 100.0 * 100.0
                for row in expected_rows
            ) / len(expected_rows)
            assert int(overall["evictions"]) == expected_evictions
            assert int(overall["oversized_requests"]) == 0
            assert abs(
                float(overall["average_cache_utilization_percent"])
                - expected_utilization
            ) < 0.01
            assert abs(
                float(overall["average_response_time_ms"])
                - expected_summary["average_ms"]
            ) < 0.01

            print(f"{policy.upper()}: reference trace and summary pass")


if __name__ == "__main__":
    main()
