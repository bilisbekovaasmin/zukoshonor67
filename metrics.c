
#include "metrics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* Initialize all statistics */
void metrics_init(SimulationMetrics *metrics)
{
    memset(metrics, 0, sizeof(*metrics));
}


/* Release allocated memory */
void metrics_destroy(SimulationMetrics *metrics)
{
    free(metrics->clients);
    memset(metrics, 0, sizeof(*metrics));
}


/* Find an existing client or create a new one */
static ClientMetrics *get_client(
    SimulationMetrics *metrics,
    const char *client_id
)
{
    for (size_t i = 0; i < metrics->client_count; i++) {

        if (strcmp(metrics->clients[i].client_id,
                   client_id) == 0) {

            return &metrics->clients[i];
        }
    }

    ClientMetrics *temp = realloc(
        metrics->clients,
        (metrics->client_count + 1)
        * sizeof(ClientMetrics)
    );

    if (temp == NULL)
        return NULL;

    metrics->clients = temp;

    ClientMetrics *client =
        &metrics->clients[metrics->client_count];

    memset(client, 0, sizeof(*client));

    snprintf(client->client_id,
             sizeof(client->client_id),
             "%s", client_id);

    metrics->client_count++;

    return client;
}


/* Update a set of statistics */
static void update_metrics(
    Metrics *metrics,
    bool hit,
    size_t size_bytes,
    double response_time_ms
)
{
    metrics->total_requests++;
    metrics->total_bytes += size_bytes;
    metrics->total_response_time_ms += response_time_ms;

    if (hit) {
        metrics->hits++;
        metrics->hit_bytes += size_bytes;
    }
    else {
        metrics->misses++;
        metrics->origin_bytes += size_bytes;
    }
}


/* Record one request */
bool metrics_record(
    SimulationMetrics *metrics,
    const char *client_id,
    bool hit,
    size_t size_bytes,
    double response_time_ms
)
{
    if (metrics == NULL || client_id == NULL)
        return false;

    if (strlen(client_id) >=
        sizeof(((ClientMetrics *)0)->client_id))
        return false;

    ClientMetrics *client =
        get_client(metrics, client_id);

    if (client == NULL)
        return false;

    /* Update overall statistics */
    update_metrics(
        &metrics->overall,
        hit,
        size_bytes,
        response_time_ms
    );

    /* Update individual client statistics */
    update_metrics(
        &client->stats,
        hit,
        size_bytes,
        response_time_ms
    );

    return true;
}


/* Calculate cache-hit ratio */
double metrics_hit_ratio(const Metrics *metrics)
{
    if (metrics->total_requests == 0)
        return 0.0;

    return 100.0 * metrics->hits
           / metrics->total_requests;
}


/* Calculate byte-hit ratio */
double metrics_byte_hit_ratio(const Metrics *metrics)
{
    if (metrics->total_bytes == 0)
        return 0.0;

    return 100.0 * metrics->hit_bytes
           / metrics->total_bytes;
}


/* Calculate average response time */
double metrics_average_response_time(
    const Metrics *metrics
)
{
    if (metrics->total_requests == 0)
        return 0.0;

    return metrics->total_response_time_ms
           / metrics->total_requests;
}


/* Print one set of statistics */
static void print_stats(
    const char *name,
    const Metrics *metrics
)
{
    printf("\n--- %s ---\n", name);

    printf("Total requests: %zu\n",
           metrics->total_requests);

    printf("Cache hits: %zu\n",
           metrics->hits);

    printf("Cache misses: %zu\n",
           metrics->misses);

    printf("Cache-hit ratio: %.2f%%\n",
           metrics_hit_ratio(metrics));

    printf("Byte-hit ratio: %.2f%%\n",
           metrics_byte_hit_ratio(metrics));

    printf("Average response time: %.2f ms\n",
           metrics_average_response_time(metrics));

    printf("Origin traffic: %" PRIu64 " bytes\n",
           metrics->origin_bytes);
}


/* Print overall and per-client results */
void metrics_print(const SimulationMetrics *metrics)
{
    print_stats("OVERALL", &metrics->overall);

    for (size_t i = 0; i < metrics->client_count; i++) {

        print_stats(
            metrics->clients[i].client_id,
            &metrics->clients[i].stats
        );
    }
}


/* Write one row to the CSV file */
static void write_csv_row(
    FILE *file,
    const char *name,
    const Metrics *metrics
)
{
    /* Quote the name and escape embedded quotes */
    fputc('"', file);

    for (const char *p = name; *p; p++) {
        if (*p == '"')
            fputc('"', file);

        fputc(*p, file);
    }

    fputc('"', file);

    fprintf(
        file,
        ",%zu,%zu,%zu,"
        "%" PRIu64 ","
        "%" PRIu64 ","
        "%" PRIu64 ","
        "%.2f,%.2f,%.2f\n",

        metrics->total_requests,
        metrics->hits,
        metrics->misses,
        metrics->total_bytes,
        metrics->hit_bytes,
        metrics->origin_bytes,
        metrics_hit_ratio(metrics),
        metrics_byte_hit_ratio(metrics),
        metrics_average_response_time(metrics)
    );
}


/* Export all statistics to CSV */
bool metrics_export_csv(
    const SimulationMetrics *metrics,
    const char *filename
)
{
    FILE *file = fopen(filename, "w");

    if (file == NULL)
        return false;

    fprintf(
        file,
        "client_id,total_requests,hits,misses,"
        "total_bytes,hit_bytes,origin_bytes,"
        "hit_ratio_percent,byte_hit_ratio_percent,"
        "average_response_time_ms\n"
    );

    write_csv_row(
        file,
        "OVERALL",
        &metrics->overall
    );

    for (size_t i = 0; i < metrics->client_count; i++) {

        write_csv_row(
            file,
            metrics->clients[i].client_id,
            &metrics->clients[i].stats
        );
    }

    bool success = !ferror(file);

    if (fclose(file) != 0)
        success = false;

    return success;
}
