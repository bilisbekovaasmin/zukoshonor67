# How to Build and Run

## Requirements

- A C11 compiler such as clang or GCC.
- make.
- Python 3.8 or newer for the experiment runner.
- No third-party Python packages are required.

## Build

From the project directory:

    make

This creates the cache_sim executable. To remove the executable, object files,
and test programs:

    make clean

## Run one policy

The command format is:

    ./cache_sim simulate RESOURCE_CSV REQUESTS_CSV CAPACITY_BYTES POLICY TRACE_CSV METRICS_CSV

Example: validate LFU with the 100 KiB reference configuration:

    ./cache_sim simulate resources.csv requests_reference.csv 102400 lfu lfu_trace.csv lfu_metrics.csv

Available policies are fifo, lru, lfu, and custom. Capacity is entered in
bytes. Use 102400 for 100 KiB and 5242880 for 5 MiB.

Use a dash for TRACE_CSV to skip the detailed per-request trace:

    ./cache_sim simulate resources.csv requests_reference.csv 102400 fifo - fifo_metrics.csv

The detailed trace records each request, its hit/miss result, evictions,
cache contents, cache usage, response time, cumulative origin traffic, and
cumulative hits. The metrics CSV includes an overall row and one row per
client.

## Validate the reference trace

Run all unit tests and the FIFO/LRU/LFU reference comparison:

    make test

The regression test compares each generated request row with the supplied
expected_fifo.csv, expected_lru.csv, and expected_lfu.csv files.

## Run the workload experiments

Build the program, then run:

    python3 run_experiments.py

The script uses resources.csv and the three request workloads. It evaluates
all four policies at 100 KiB, 500 KiB, 1 MiB, 2 MiB, 5 MiB, and 10 MiB.
Results are saved under results/:

- experiment_summary.csv: one overall row per workload, capacity, and policy.
- mixed_clients_5MiB.csv: per-client results for the mixed workload at 5 MiB.
- workload_characterization.csv: trace size, clients, resources, data volume,
  top five resources, and oversized-request counts by capacity.
- resource_frequency.csv: request counts and shares for every resource in each
  workload.
- run_metrics/: full overall and per-client CSVs for each experiment.
- SVG charts: hit ratio versus capacity, response time at 5 MiB, and mixed
  workload per-client hit ratios, plus a top-resource workload comparison.

## Output definitions

- Hit ratio: cache hits divided by total requests.
- Byte-hit ratio: bytes served from cache divided by total requested bytes.
- Average response time: mean of 5 ms for hits and origin latency for misses.
- Origin traffic: sum of resource bytes for all misses.
- Evictions: resources removed to make room for new entries.
- Oversized requests: requests for resources larger than the cache capacity.
- Average cache utilization: mean occupied-cache percentage after each request.

The simulator processes requests sequentially, starts with an empty cache,
uses a fixed 5 ms hit latency, and ignores expiration and Cache-Control rules.

## Report and presentation

REPORT.pdf and REPORT.docx contain the four-page technical report.
PRESENTATION_SCRIPT.md provides a 12-minute outline split among four speakers.
The group must record the video and upload it to YouTube before submission.

To regenerate the editable report, install the optional document dependencies
and run:

    python3 -m pip install -r requirements-report.txt
    python3 tools/create_report.py

The script recreates REPORT.docx and its embedded chart images. Open the DOCX
in Word or LibreOffice to export a PDF.
