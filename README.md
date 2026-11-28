# Shared Web Cache Simulator

A trace-driven C simulator for comparing FIFO, LRU, LFU, and a custom
replacement policy. It reads resource metadata and chronological HTTP request
traces; it does not contact origin servers or store real response bodies.

## Build and run

    make
    make test

Run one simulation with:

    ./cache_sim simulate resources.csv requests_reference.csv 102400 lfu \
      lfu_trace.csv lfu_metrics.csv

The command arguments are: resource file, request trace, cache capacity in
bytes, policy (fifo, lru, lfu, or custom), detailed trace output, and summary
metrics output. Use - in place of the detailed trace filename to skip
per-request output.

Run the full experiment suite with:

    python3 run_experiments.py

The script evaluates all four policies on the sequential, repetitive, and
mixed traces at six capacities. It creates CSV results, full resource
frequency distributions, workload characterization, and SVG charts under
results/.

See HOWTO.md for complete commands and output descriptions.

## Simulator assumptions

- The cache starts empty and processes requests in file order.
- Cache-hit latency is fixed at 5 ms.
- Miss latency and resource size come from resources.csv.
- Resources remain unchanged and all requests are treated as cacheable.
- A resource larger than the cache is fetched on every request but not stored.
- LFU uses least-recently-used as its frequency tie-breaker.
- Cache capacity is measured in bytes; KiB means 1,024 bytes.

## Custom policy

The custom policy assigns a retention score using origin latency per KiB,
access frequency, recent use, and an approximate count of distinct clients.
The lowest score is evicted. Its score is a design choice for this project,
not a universal measure of resource value.

## Main files

- cache.c / cache.h: shared cache storage, lookup, insertion, and eviction.
- policies.c / policies.h: FIFO and LRU victim selection.
- lfu_policy.c / lfu_policy.h: LFU victim selection and access bookkeeping.
- custom_policy.c / custom_policy.h: custom score and victim selection.
- data_loader.c / data_loader.h: CSV input and resource matching.
- simulation.c / simulation.h: request-by-request simulation.
- metrics.c / metrics.h: global and per-client statistics.
- main.c: command-line entry point.
- run_experiments.py: workload analysis, policy/capacity runs, and charts.
- tests/: unit tests and reference-trace regression checks.
- REPORT.pdf / REPORT.docx: four-page project report.
- PRESENTATION_SCRIPT.md: timed four-person video script.
