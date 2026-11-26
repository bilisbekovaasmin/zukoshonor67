#ifndef CACHE_H
#define CACHE_H

#include "policy_types.h"

/* Stores PolicyEntry records so the cache and every replacement policy use
 * the same shared resource structure. */
typedef struct {
    PolicyEntry *entries;
    size_t capacity_bytes;
    size_t used_bytes;
    size_t count;
    size_t allocated;
    uint64_t clock; /* Increases once per simulated request/cache insertion. */
} Cache;

/* A policy returns the index of the entry to evict, or SIZE_MAX on failure. */
typedef size_t (*CacheVictimSelector)(const PolicyEntry entries[], size_t count);

int cache_init(Cache *cache, size_t capacity_bytes);
void cache_destroy(Cache *cache);

/* A hit updates frequency and last-access time. A miss returns NULL. */
PolicyEntry *cache_lookup(Cache *cache, const char *key);
const PolicyEntry *cache_find(const Cache *cache, const char *key);

/* Remove the entry at index. Indices may change after removal. */
int cache_remove_at(Cache *cache, size_t index);

/* Insert a resource. Evicts repeatedly until there is enough room.
 * Return: 1 inserted/already present, 0 invalid or too large, -1 error. */
int cache_insert(Cache *cache, const char *key, size_t size_bytes,
                 unsigned origin_latency_ms, CacheVictimSelector select_victim,
                 size_t *evictions);

#endif
