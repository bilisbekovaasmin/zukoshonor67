
#include <stdio.h>
#include <stdlib.h>
#include "data_loader.h"

int main(void)
{
    ResourceInfo *resources = NULL;
    Request *requests = NULL;

    /* Load resources */
    int resource_count = load_resources(
        "resources.csv", &resources
    );

    if (resource_count < 0) {
        printf("Error loading resources.\n");
        return 1;
    }

    /* Load requests */
    int request_count = load_requests(
        "requests_reference.csv", &requests
    );

    if (request_count < 0) {
        printf("Error loading requests.\n");
        free(resources);
        return 1;
    }

    printf("Resources loaded: %d\n", resource_count);
    printf("Requests loaded: %d\n\n", request_count);

    int matched = 0;

    /* Test resource matching */
    for (int i = 0; i < request_count; i++) {

        int index = find_resource(
            resources,
            resource_count,
            &requests[i]
        );

        if (index >= 0) {
            printf(
                "%2d | %s | ID: %d | %zu bytes | %d ms\n",
                i + 1,
                requests[i].client_id,
                resources[index].id,
                resources[index].size_bytes,
                resources[index].origin_latency_ms
            );

            matched++;
        }
        else {
            printf("Request %d: Resource not found\n",
                   i + 1);
        }
    }

    printf("\nMatched requests: %d/%d\n",
           matched, request_count);

    /* Check reference trace */
    int passed = resource_count == 21 &&
                 request_count == 15 &&
                 matched == request_count;

    if (passed)
        printf("TEST PASSED!\n");
    else
        printf("TEST FAILED!\n");

    /* Release allocated memory */
    free(resources);
    free(requests);

    return passed ? 0 : 1;
}
