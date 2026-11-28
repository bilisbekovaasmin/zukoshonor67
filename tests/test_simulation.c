
#include <stdio.h>
#include <stdlib.h>
#include "simulation.h"

int main(void)
{
    ResourceInfo *resources = NULL;
    Request *requests = NULL;

    /* Load resource information */
    int resource_count = load_resources(
        "resources.csv",
        &resources
    );

    /* Load client requests */
    int request_count = load_requests(
        "requests_reference.csv",
        &requests
    );

    /* Check that both files loaded successfully */
    if (resource_count <= 0 || request_count <= 0) {
        fprintf(stderr, "Failed to load input files.\n");

        free(resources);
        free(requests);

        return 1;
    }

    printf("Resources loaded: %d\n", resource_count);
    printf("Requests loaded: %d\n", request_count);
    printf("\n--- LFU SIMULATION ---\n\n");

    /* Run LFU with a 100 KiB cache */
    bool success = run_simulation(
        resources,
        resource_count,
        requests,
        request_count,
        100 * 1024,
        SIM_LFU,
        "lfu_trace.csv",
        "lfu_metrics.csv"
    );
printf("\n--- CUSTOM SIMULATION ---\n\n");

bool custom_success = run_simulation(
    resources,
    resource_count,
    requests,
    request_count,
    100 * 1024,
    SIM_CUSTOM,
    "custom_trace.csv",
    "custom_metrics.csv"
);

success = success && custom_success;
    /* Release loaded data */
    free(resources);
    free(requests);

    if (success)
        printf("\nSIMULATION COMPLETED!\n");
    else
        printf("\nSIMULATION FAILED!\n");

    return success ? 0 : 1;
}
