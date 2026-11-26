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
typedef size_t (*CacheVictimSelector)(const PolicyEntry entries[], size_t count);

/* Set up an empty cache, then free it when the simulation ends. */
int cache_init(Cache *cache, size_t capacity_bytes);
void cache_destroy(Cache *cache);

/* Check requests: lookup records a hit; find only checks whether it is present. */
PolicyEntry *cache_lookup(Cache *cache, const char *key);
const PolicyEntry *cache_find(const Cache *cache, const char *key);

/* Remove a selected resource to make space. */
int cache_remove_at(Cache *cache, size_t index);

/* Store a missed resource, asking the selected policy to make room if needed.
 * `evictions` can report how many resources had to be removed. */
int cache_insert(Cache *cache, const char *key, size_t size_bytes,
                 unsigned origin_latency_ms, CacheVictimSelector select_victim,
                 size_t *evictions);

#endif
