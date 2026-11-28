
#ifndef METRICS_H
#define METRICS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* Performance statistics */
typedef struct {
    size_t total_requests;
    size_t hits;
    size_t misses;

    uint64_t total_bytes;
    uint64_t hit_bytes;
    uint64_t origin_bytes;

    double total_response_time_ms;
} Metrics;

/* Statistics for an individual client */
typedef struct {
    char client_id[64];
    Metrics stats;
} ClientMetrics;

/* Overall and individual client statistics */
typedef struct {
    Metrics overall;

    ClientMetrics *clients;
    size_t client_count;
} SimulationMetrics;

/* Initialization and cleanup */
void metrics_init(SimulationMetrics *metrics);

void metrics_destroy(SimulationMetrics *metrics);

/* Record a processed request */
bool metrics_record(
    SimulationMetrics *metrics,
    const char *client_id,
    bool hit,
    size_t size_bytes,
    double response_time_ms
);

/* Performance calculations */
double metrics_hit_ratio(const Metrics *metrics);

double metrics_byte_hit_ratio(const Metrics *metrics);

double metrics_average_response_time(
    const Metrics *metrics
);

/* Display results */
void metrics_print(const SimulationMetrics *metrics);

/* Export results to CSV */
bool metrics_export_csv(
    const SimulationMetrics *metrics,
    const char *filename
);

#endif
