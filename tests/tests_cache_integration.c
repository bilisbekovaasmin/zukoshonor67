/*Tests LFU and BFV through the team's shared CAche structure*/

#include "cache.h" //Provides the Cache structure and function prototypes
#include "cache_policies.h" //Provides the LFU and BFV policy structures

#include <assert.h> //Provides the assert macro for runtime assertions
#include <stdio.h>  //Provides the printf function for outputting information

#define KIB 1024U //Defines a kibibyte as 1024 bytes
#define CACHE_CAPACITY (100U * KIB) //Defines the cache's maximum capacity as 100 kibibytes

/*Verifies that the real cache calls LFU correctly */

static void test_lfu_integration(void) {
    Cache cache;
    size_t evictions;

    assert(cache_init(&cache, CACHE_CAPACITY));

    /*Fill the cache with A, B, and C: 88 KiB total*/
    assert (cache_insert(&cache, "A", 32 * KIB, 125, "student1", &LFU_CACHE_POLICY, &evictions)==1);
    assert (cache_insert(&cache, "B", 42 * KIB, 90, "student2", &LFU_CACHE_POLICY, &evictions)==1);
    assert (cache_insert(&cache, "C", 14 * KIB, 80, "student3", &LFU_CACHE_POLICY, &evictions)==1);
    /* Give A and B one additional hit each. */
    assert(cache_lookup(
        &cache, "A", "student1", &LFU_CACHE_POLICY
    ) != NULL);

    assert(cache_lookup(
        &cache, "B", "student2", &LFU_CACHE_POLICY
    ) != NULL);

    /*
     * D needs 24 KiB. C has the lowest frequency,
     * so LFU must evict C.
     */
    assert(cache_insert(
        &cache, "D", 24 * KIB, 95,
        "student3", &LFU_CACHE_POLICY, &evictions
    ) == 1);

    assert(evictions == 1);
    assert(cache_find(&cache, "C") == NULL);
    assert(cache_find(&cache, "D") != NULL);
    assert(cache.used_bytes == 98 * KIB);

    cache_destroy(&cache);

    puts("Shared cache + LFU integration -- PASS");
}

/*
 * Verifies that the real cache calls BFV correctly.
 */
static void test_bfv_integration(void)
{
    Cache cache;
    size_t evictions;

    assert(cache_init(&cache, CACHE_CAPACITY));

    assert(cache_insert(
        &cache, "A", 32 * KIB, 125,
        "student1", &BFV_CACHE_POLICY, &evictions
    ) == 1);

    assert(cache_insert(
        &cache, "B", 42 * KIB, 90,
        "student2", &BFV_CACHE_POLICY, &evictions
    ) == 1);

    assert(cache_insert(
        &cache, "C", 14 * KIB, 80,
        "student3", &BFV_CACHE_POLICY, &evictions
    ) == 1);

    /* BFV records both hit information and client sharing. */
    assert(cache_lookup(
        &cache, "A", "student1", &BFV_CACHE_POLICY
    ) != NULL);

    assert(cache_lookup(
        &cache, "B", "student2", &BFV_CACHE_POLICY
    ) != NULL);

    /*
     * BFV values small resources and retrieval cost per KiB.
     * At this point, B has the lowest score and is evicted.
     */
    assert(cache_insert(
        &cache, "D", 24 * KIB, 95,
        "student3", &BFV_CACHE_POLICY, &evictions
    ) == 1);

    assert(evictions == 1);
    assert(cache_find(&cache, "B") == NULL);
    assert(cache_find(&cache, "D") != NULL);
    assert(cache.used_bytes == 70 * KIB);

    cache_destroy(&cache);

    puts("Shared cache + BFV integration -- PASS");
}

int main(void)
{
    test_lfu_integration();
    test_bfv_integration();

    puts("All shared-cache integration tests -- PASS");

    return 0;

}