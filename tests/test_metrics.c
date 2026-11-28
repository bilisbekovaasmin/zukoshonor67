
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "metrics.h"

/* Compare decimal values */
static bool almost_equal(double a, double b)
{
    return fabs(a - b) < 0.0001;
}

int main(void)
{
    SimulationMetrics metrics;

    metrics_init(&metrics);

    /* Simulate four requests */
    if (!metrics_record(&metrics, "student-01",
                        false, 32768, 125) ||
        !metrics_record(&metrics, "student-02",
                        false, 43008, 90) ||
        !metrics_record(&metrics, "student-01",
                        true, 32768, 5) ||
        !metrics_record(&metrics, "student-02",
                        true, 14336, 5)) {

        printf("Error recording requests.\n");
        metrics_destroy(&metrics);
        return 1;
    }

    /* Display statistics */
    metrics_print(&metrics);

    /* Check overall statistics */
    bool passed =
        metrics.overall.total_requests == 4 &&
        metrics.overall.hits == 2 &&
        metrics.overall.misses == 2 &&
        metrics.overall.total_bytes == 122880 &&
        metrics.overall.hit_bytes == 47104 &&
        metrics.overall.origin_bytes == 75776 &&
        almost_equal(
            metrics_hit_ratio(&metrics.overall),
            50.0
        ) &&
        almost_equal(
            metrics_byte_hit_ratio(&metrics.overall),
            38.333333
        ) &&
        almost_equal(
            metrics_average_response_time(
                &metrics.overall
            ),
            56.25
        );

    /* Check individual client statistics */
    passed = passed &&
        metrics.client_count == 2 &&
        metrics.clients[0].stats.total_requests == 2 &&
        metrics.clients[1].stats.total_requests == 2 &&
        metrics.clients[0].stats.hits == 1 &&
        metrics.clients[1].stats.hits == 1 &&
        metrics.clients[0].stats.origin_bytes == 32768 &&
        metrics.clients[1].stats.origin_bytes == 43008 &&
        almost_equal(
            metrics_average_response_time(
                &metrics.clients[0].stats
            ),
            65.0
        ) &&
        almost_equal(
            metrics_average_response_time(
                &metrics.clients[1].stats
            ),
            47.5
        );

    /* Test CSV export */
    bool exported = metrics_export_csv(
        &metrics,
        "test_metrics_results.csv"
    );

    if (exported)
        printf("\nCSV exported successfully.\n");
    else
        printf("\nCSV export failed.\n");

    passed = passed && exported;

    if (passed)
        printf("\nTEST PASSED!\n");
    else
        printf("\nTEST FAILED!\n");

    metrics_destroy(&metrics);

    return passed ? 0 : 1;
}
