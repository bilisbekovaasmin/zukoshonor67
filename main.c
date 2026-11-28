#include "data_loader.h"
#include "simulation.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *program) {
    fprintf(stderr,
        "Usage:\n"
        "  %s simulate <resources.csv> <requests.csv> <capacity_bytes>\n"
        "      <fifo|lru|lfu|custom> <trace.csv> <metrics.csv>\n"
        "  %s --help\n\n"
        "Example:\n"
        "  %s simulate resources.csv requests_reference.csv 102400 lfu\n"
        "      lfu_trace.csv lfu_metrics.csv\n",
        program, program, program);
}

static int parse_policy(const char *name, SimulationPolicy *policy) {
    if (strcmp(name, "fifo") == 0) *policy = SIM_FIFO;
    else if (strcmp(name, "lru") == 0) *policy = SIM_LRU;
    else if (strcmp(name, "lfu") == 0) *policy = SIM_LFU;
    else if (strcmp(name, "custom") == 0) *policy = SIM_CUSTOM;
    else return 0;
    return 1;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (argc != 8 || strcmp(argv[1], "simulate") != 0) {
        print_usage(argv[0]);
        return 2;
    }

    errno = 0;
    char *end = NULL;
    unsigned long long parsed_capacity = strtoull(argv[4], &end, 10);
    if (argv[4][0] == '-' || errno != 0 || end == argv[4] || *end != '\0' ||
        parsed_capacity == 0 || parsed_capacity > (unsigned long long)SIZE_MAX) {
        fprintf(stderr, "Invalid cache capacity in bytes: %s\n", argv[4]);
        return 2;
    }

    SimulationPolicy policy;
    if (!parse_policy(argv[5], &policy)) {
        fprintf(stderr, "Unknown policy: %s\n", argv[5]);
        print_usage(argv[0]);
        return 2;
    }

    ResourceInfo *resources = NULL;
    Request *requests = NULL;
    int resource_count = load_resources(argv[2], &resources);
    if (resource_count < 0) {
        fprintf(stderr, "Could not load resource file: %s\n", argv[2]);
        return 1;
    }

    int request_count = load_requests(argv[3], &requests);
    if (request_count < 0) {
        fprintf(stderr, "Could not load request file: %s\n", argv[3]);
        free(resources);
        return 1;
    }

    bool success = run_simulation(
        resources,
        resource_count,
        requests,
        request_count,
        (size_t)parsed_capacity,
        policy,
        strcmp(argv[6], "-") == 0 ? NULL : argv[6],
        argv[7]
    );

    free(resources);
    free(requests);
    return success ? 0 : 1;
}
