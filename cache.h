#ifndef CACHE_H
#define CACHE_H

#include "policy_types.h"

/* The cache's current contents and space usage. */
typedef struct {
    /* The resources currently stored in the cache. */
    PolicyEntry *entries;

    /* Capacity and current usage are measured in bytes. */
    size_t capacity_bytes;
    size_t used_bytes;

    /* Number of resources in the list and number of slots allocated. */
    size_t count;
    size_t allocated;

    /* Helps policies compare when resources were last used or inserted. */
    uint64_t clock;
} Cache;

/* FIFO, LRU, or LFU uses this type to tell the cache which entry to remove. */
/*Selects an eviction victim
current_tick allows recency-aware policies to calculate how old each resource is*/

typedef size_t (*CacheVictimSelector)(
    const PolicyEntry entries[],
    size_t count,
    uint64_t current_tick);
/*Initializes policy metadata when a resource is inserted
Custom policies can use client_id. If this callback is Null, the cache uses its normal frequency and timestamp initialization*/
typedef void (*CachePolicyInitializer)(
    PolicyEntry *entry,
    const char *key,
    size_t size_bytes,
    unsigned origin_latency_ms,
    const char *client_id,
    uint64_t tick
);

/*Updates policy metadata after a cache hit
If this callback is NULL, the cache performs its normal frequency and recency update*/
typedef void (*CacheHitRecorder)(
    PolicyEntry *entry,
    const char *client_id,
    uint64_t tick
);
/*Groups the three operations belonging to one replacement policy*/
typedef struct {
    CacheVictimSelector choose_victim;
    CachePolicyInitializer init_entry;
    CacheHitRecorder record_hit;
} CachePolicy;
/* Set up an empty cache, then free it when the simulation ends. */
int cache_init(Cache *cache, size_t capacity_bytes);
void cache_destroy(Cache *cache);

/* Check requests: lookup records a hit; find only checks whether it is present. */
PolicyEntry *cache_lookup(
    Cache *cache,
    const char *key,
    const char *client_id,
    const CachePolicy *policy
);
const PolicyEntry *cache_find(const Cache *cache, const char *key);

/* Remove a selected resource to make space. */
int cache_remove_at(Cache *cache, size_t index);

/* Store a missed resource, asking the selected policy to make room if needed.
 * `evictions` can report how many resources had to be removed. */
int cache_insert(
    Cache *cache,
    const char *key,
    size_t size_bytes,
    unsigned origin_latency_ms,
    const char *client_id,
    const CachePolicy *policy,
    size_t *evictions
);

#endif
