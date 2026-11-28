
#include "simulation.h"
#include "lfu_policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_HIT_LATENCY_MS 5
#define KEY_LENGTH 1300

bool run_simulation(
    const ResourceInfo *resources,
    int resource_count,
    const Request *requests,
    int request_count,
    size_t capacity_bytes,
    SimulationPolicy policy,
    const char *trace_filename,
    const char *metrics_filename
)
{
    Cache cache;
    SimulationMetrics metrics;
    CacheVictimSelector select_victim;

    /* Detailed trace will be added next */
    (void)trace_filename;

    /* Validate input */
    if (resources == NULL || requests == NULL ||
        resource_count <= 0 || request_count < 0 ||
        metrics_filename == NULL) {
        return false;
    }

    /* Select replacement policy */
    switch (policy) {
        case SIM_LFU:
            select_victim = lfu_choose_victim;
            break;

        default:
            fprintf(stderr, "Policy not connected yet.\n");
            return false;
    }

    /* Allocate persistent resource keys */
    char (*keys)[KEY_LENGTH] =
        calloc((size_t)resource_count, sizeof *keys);

    if (keys == NULL)
        return false;

    /* Construct a URL for each resource */
    for (int i = 0; i < resource_count; i++) {
        int length = snprintf(
            keys[i],
            KEY_LENGTH,
            "%s://%s%s",
            resources[i].scheme,
            resources[i].host,
            resources[i].path
        );

        if (length < 0 || length >= KEY_LENGTH) {
            free(keys);
            return false;
        }
    }

    /* Initialize cache */
    if (!cache_init(&cache, capacity_bytes)) {
        free(keys);
        return false;
    }

    /* Initialize performance metrics */
    metrics_init(&metrics);

    bool success = true;

    /* Process requests in chronological input order */
    for (int i = 0; i < request_count; i++) {

        const Request *request = &requests[i];

        int index = find_resource(
            resources,
            resource_count,
            request
        );

        if (index < 0 || index >= resource_count) {
            fprintf(stderr,
                    "Resource not found at request %d\n",
                    i + 1);

            success = false;
            break;
        }

        const ResourceInfo *resource = &resources[index];

        if (resource->origin_latency_ms < 0) {
            success = false;
            break;
        }

        /* Check whether resource is cached */
        PolicyEntry *entry = cache_lookup(
            &cache,
            keys[index]
        );

        bool hit = (entry != NULL);

        unsigned response_ms;
        size_t evictions = 0;

        if (hit) {
            response_ms = CACHE_HIT_LATENCY_MS;
        } else {
            response_ms = resource->origin_latency_ms;

            int inserted = cache_insert(
                &cache,
                keys[index],
                resource->size_bytes,
                resource->origin_latency_ms,
                select_victim,
                &evictions
            );

            if (inserted < 0) {
                fprintf(stderr, "Cache insertion failed.\n");
                success = false;
                break;
            }
        }

        /* Record performance statistics */
        if (!metrics_record(
                &metrics,
                request->client_id,
                hit,
                resource->size_bytes,
                response_ms)) {

            success = false;
            break;
        }

        /* Display individual request result */
        printf(
            "%2d | %s | ID: %d | %s | %u ms | Evictions: %zu\n",
            i + 1,
            request->client_id,
            resource->id,
            hit ? "HIT" : "MISS",
            response_ms,
            evictions
        );
    }

    /* Display and export summary statistics */
    if (success) {
        metrics_print(&metrics);

        success = metrics_export_csv(
            &metrics,
            metrics_filename
        );
    }

    /* Release allocated memory */
    metrics_destroy(&metrics);
    cache_destroy(&cache);
    free(keys);

    return success;
}
