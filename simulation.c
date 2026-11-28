
#include "simulation.h"
#include "lfu_policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_HIT_LATENCY_MS 5
#define KEY_LENGTH 1300

/* Convert resource IDs to reference trace labels */
static void resource_label(int id, char *label, size_t size)
{
    switch (id) {
        case 1:
            snprintf(label, size, "A");
            break;

        case 6:
            snprintf(label, size, "B");
            break;

        case 7:
            snprintf(label, size, "C");
            break;

        case 12:
            snprintf(label, size, "D");
            break;

        case 18:
            snprintf(label, size, "E");
            break;

        default:
            snprintf(label, size, "%d", id);
            break;
    }
}

/* Information needed for detailed eviction tracking */
static const ResourceInfo *trace_resources = NULL;
static char (*trace_keys)[KEY_LENGTH] = NULL;
static int trace_resource_count = 0;

static int *evicted_ids = NULL;
static size_t evicted_count = 0;

/* Find the numeric resource ID using its cache key */
static int resource_id_from_key(const char *key)
{
    for (int i = 0; i < trace_resource_count; i++) {
        if (strcmp(trace_keys[i], key) == 0) {
            return trace_resources[i].id;
        }
    }

    return -1;
}

/* Select an LFU victim and record its resource ID */
static size_t tracked_lfu_choose_victim(
    const PolicyEntry entries[],
    size_t count)
{
    size_t victim = lfu_choose_victim(entries, count);

    if (victim < count &&
        evicted_ids != NULL &&
        evicted_count < (size_t)trace_resource_count) {

        evicted_ids[evicted_count++] =
            resource_id_from_key(entries[victim].key);
    }

    return victim;
}

/* Write one request result to the detailed CSV trace */
static void write_trace_row(
    FILE *file,
    int step,
    const ResourceInfo *resource,
    const Request *request,
    bool hit,
    const Cache *cache,
    unsigned response_ms,
    size_t cumulative_origin_bytes,
    size_t cumulative_hits)
{
    char label[32];

    resource_label(resource->id, label, sizeof label);

    /* Request information */
    fprintf(file, "%d,%s,%s,%s,",
            step,
            label,
            request->client_id,
            hit ? "HIT" : "MISS");

    /* Evicted resources in eviction order */
    if (evicted_count == 0) {
        fprintf(file, "-");
    } else {
        if (evicted_count > 1)
            fprintf(file, "\"");

        for (size_t i = 0; i < evicted_count; i++) {
            resource_label(
                evicted_ids[i], label, sizeof label
            );

            fprintf(file, "%s%s",
                    i == 0 ? "" : ",", label);
        }

        if (evicted_count > 1)
            fprintf(file, "\"");
    }

    fprintf(file, ",");

    /* Cache contents in insertion order */
    if (cache->count == 0) {
        fprintf(file, "-");
    } else {
        if (cache->count > 1)
            fprintf(file, "\"");

        for (size_t i = 0; i < cache->count; i++) {
            int id = resource_id_from_key(
                cache->entries[i].key
            );

            resource_label(id, label, sizeof label);

            fprintf(file, "%s%s",
                    i == 0 ? "" : ",", label);
        }

        if (cache->count > 1)
            fprintf(file, "\"");
    }

    /* Capacity, latency and cumulative statistics */
    fprintf(file, ",%.10g,%u,%zu,%zu\n",
            cache->used_bytes / 1024.0,
            response_ms,
            cumulative_origin_bytes,
            cumulative_hits);
}

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

    /* Validate input */
    if (resources == NULL || requests == NULL ||
        resource_count <= 0 || request_count < 0 ||
        metrics_filename == NULL) {
        return false;
    }

    /* Select replacement policy */
    switch (policy) {
        case SIM_LFU:
    select_victim = tracked_lfu_choose_victim;
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
    
/* Initialize eviction tracking */
trace_resources = resources;
trace_keys = keys;
trace_resource_count = resource_count;

evicted_ids = calloc(
    (size_t)resource_count,
    sizeof *evicted_ids
);

if (evicted_ids == NULL) {
    metrics_destroy(&metrics);
    cache_destroy(&cache);
    free(keys);

    trace_resources = NULL;
    trace_keys = NULL;
    trace_resource_count = 0;

    return false;
}

evicted_count = 0;
    
/* Open detailed trace CSV */
FILE *trace_file = NULL;

if (trace_filename != NULL) {
    trace_file = fopen(trace_filename, "w");

    if (trace_file == NULL) {
        fprintf(stderr, "Failed to open trace CSV.\n");
        success = false;
    }
}

if (trace_file != NULL) {
    fprintf(trace_file,
        "step,resource_id,client_id,result,"
        "evicted,cache_contents_oldest_first,"
        "cache_used_KiB,response_ms,"
        "cumulative_origin_bytes,cumulative_hits\n"
    );
}

/* Running totals for the detailed trace */
size_t cumulative_origin_bytes = 0;
size_t cumulative_hits = 0;

    /* Process requests in chronological input order */
    for (int i = 0; success && i < request_count; i++) {

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
        evicted_count = 0;

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

/* Update cumulative trace statistics */
if (hit) {
    cumulative_hits++;
} else {
    cumulative_origin_bytes += resource->size_bytes;
}

/* Export the current request */
if (trace_file != NULL) {
    write_trace_row(
        trace_file,
        i + 1,
        resource,
        request,
        hit,
        &cache,
        response_ms,
        cumulative_origin_bytes,
        cumulative_hits
    );
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

/* Close the detailed trace file */
if (trace_file != NULL) {
    if (ferror(trace_file))
        success = false;

    if (fclose(trace_file) != 0)
        success = false;
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
free(evicted_ids);

evicted_ids = NULL;
evicted_count = 0;
trace_resources = NULL;
trace_keys = NULL;
trace_resource_count = 0;

metrics_destroy(&metrics);
cache_destroy(&cache);
free(keys);

return success;
    
}
