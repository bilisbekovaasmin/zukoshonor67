# 12-Minute Presentation Plan

The assignment asks each of four team members to speak for about three
minutes. Use this as a rehearsal script. Replace any wording that does not
match what the speaker personally implemented.

## 0:00–3:00 — Malak: cache structure

“Our project is a trace-driven web-cache simulator. It reads a list of
resources and a chronological list of requests. It does not contact real
servers or store website contents. It keeps resource information in one
shared cache so requests from different clients can benefit from earlier
requests.

“My part is the cache structure and its operations. A cache entry stores a
resource key, its size, origin latency, access frequency, last-access time,
and insertion time. The cache also tracks its capacity and used bytes. When a
request arrives, the simulator looks for its key. A hit uses the fixed
5-millisecond cache latency. A miss uses the origin latency from the metadata.

“When a missed resource fits individually but there is not enough free space,
the cache asks the selected policy for an entry index. It removes entries
until there is enough room, then adds the new one. A resource larger than the
whole cache is not stored. Keeping the cache operations separate from victim
selection lets us use the same cache with FIFO, LRU, LFU, and our custom
policy.”

Show cache.h and the insertion/lookup path in cache.c.

## 3:00–6:00 — Mia: FIFO and LRU

“FIFO and LRU use different definitions of the resource that should leave.
FIFO removes the resource that entered the cache first. A hit does not change
its insertion time. LRU removes the resource whose most recent use happened
longest ago. Each hit updates its last-access time.

“The policies only choose an index. The cache is responsible for removing
the selected entry and adjusting used space. This keeps the policy rule
small and lets all policies share the same insertion and removal code.

“We checked FIFO and LRU against the instructor's 15-request reference trace
at 100 KiB. The regression test compares every row, including hits, misses,
evictions, cache contents, response time, and cumulative origin traffic.”

Show policies.c and the passing reference test output.

## 6:00–9:00 — Yasmin: LFU and custom policy

“LFU removes the resource with the lowest access frequency since insertion.
When frequencies tie, it removes the least recently used resource. A new
resource starts at frequency one, and its count restarts if it is later
inserted again.

“Our custom policy gives each resource a retention score. The score combines
origin latency per KiB, frequency, recency, and an approximate measure of
how many clients have used it. A higher score means the policy is more
reluctant to evict the resource. The score favors small resources that are
costly to retrieve, popular, recent, or shared by several clients.

“This policy is a trade-off, not a guaranteed winner for every measurement.
For example, counting request hits gives a small image and a large file equal
weight, while byte-hit ratio gives more weight to large files.”

Show lfu_policy.c, custom_policy.c, and the custom-policy unit test.

## 9:00–12:00 — Alina: inputs, metrics, and findings

“The data loader reads resources.csv and a request trace. It matches each
request to metadata using scheme, host, and path. Requests are processed in
file order. The metrics module calculates global and per-client hit ratio,
byte-hit ratio, average response time, origin traffic, evictions, oversized
requests, and average cache utilization.

“We tested the reference trace first. LFU produced four hits out of fifteen,
an 84.33-millisecond average response time, and 362 KiB of origin traffic,
matching the expected result.

“We then ran 72 comparisons: four policies, three workloads, and six cache
capacities. At 5 MiB, the custom policy had the highest request hit ratio
and lowest average response time in all three workloads. In the mixed
workload, FIFO had lower origin traffic and a higher byte-hit ratio than
custom. This shows why we compare several measurements instead of choosing
one winner from hit ratio alone.

“Increasing capacity from 5 MiB to 10 MiB made the policies converge on
these traces. The two very large video resources still exceeded 10 MiB, so
their requests continued to go to the origin.”

Live demo: run the 100 KiB reference command, show the matching CSV output,
then show results/experiment_summary.csv and the charts.

## Commands for the live demonstration

    make
    make test
    ./cache_sim simulate resources.csv requests_reference.csv 102400 lfu lfu_trace.csv lfu_metrics.csv
    python3 run_experiments.py

The team must record the presentation, check that all four members speak,
upload it to YouTube, and submit the video link.
