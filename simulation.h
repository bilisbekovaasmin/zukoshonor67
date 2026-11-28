
#ifndef SIMULATION_H
#define SIMULATION_H

#include <stddef.h>
#include <stdbool.h>
#include "cache.h"
#include "data_loader.h"
#include "metrics.h"

/* Available cache replacement policies */
typedef enum {
    SIM_FIFO,
    SIM_LRU,
    SIM_LFU,
    SIM_CUSTOM
} SimulationPolicy;

/* Run the cache simulation and export results */
bool run_simulation(
    const ResourceInfo *resources,
    int resource_count,
    const Request *requests,
    int request_count,
    size_t capacity_bytes,
    SimulationPolicy policy,
    const char *trace_filename,
    const char *metrics_filename
);

#endif
