#include "cache.h"
#include "cache_policies.h"
#include <stdlib.h>
#include <string.h>

/* Start with an empty cache and remember its maximum size. */
int cache_init(Cache *cache, size_t capacity_bytes) {
    if (cache == NULL || capacity_bytes == 0) return 0;
    cache->entries = NULL;
    cache->capacity_bytes = capacity_bytes;
    cache->used_bytes = 0;
    cache->count = 0;
    cache->allocated = 0;
    /* Used to tell which resource was accessed most recently. */
    cache->clock = 0;
    return 1;
}

/* Free the memory used by the cache when the simulation is over. */
void cache_destroy(Cache *cache) {
    if (cache == NULL) return;
    free(cache->entries);
    cache->entries = NULL;
    cache->capacity_bytes = 0;
    cache->used_bytes = 0;
    cache->count = 0;
    cache->allocated = 0;
    cache->clock = 0;
}

/* Search only. Unlike cache_lookup, this does not count as a resource access. */
const PolicyEntry *cache_find(const Cache *cache, const char *key) {
    size_t i;
    if (cache == NULL || key == NULL) return NULL;

    for (i = 0; i < cache->count; ++i)
        if (cache->entries[i].key != NULL &&
            strcmp(cache->entries[i].key, key) == 0)
            return &cache->entries[i];
    return NULL;
}

/* Search for a requested resource and update its usage information on a hit. */
PolicyEntry *cache_lookup(
    Cache *cache,
    const char *key,
    const char *client_id,
    const CachePolicy *policy)
    {
        size_t i;
        if (cache == NULL || key == NULL) {
            return NULL;
    }

    for (i = 0; i < cache->count; ++i) {
        PolicyEntry *entry = &cache->entries[i];

        if (entry->key != NULL && strcmp(entry->key, key) == 0) {
            /* These values help LFU count uses and LRU compare recent uses. */
            ++cache->clock;

            if (policy != NULL && policy->record_hit != NULL) {
                policy->record_hit(entry, client_id, cache->clock);
            } else
             {
                /* Default behavior if no custom policy is provided. */
                if (entry->frequency < UINT64_MAX) {
                    ++entry->frequency;
                }
                entry->last_access_tick = cache->clock;
            }
            return entry;
        }
    }
    return NULL;
}

/* Remove a chosen resource and keep the list of cached resources compact. */
int cache_remove_at(Cache *cache, size_t index) {
    if (cache == NULL || index >= cache->count) return 0;
    cache->used_bytes -= cache->entries[index].size_bytes;
    /* Close the gap so the next resource stays at this index. */
    if (index + 1 < cache->count)
        memmove(&cache->entries[index], &cache->entries[index + 1],
                (cache->count - index - 1) * sizeof(*cache->entries));
    --cache->count;
    return 1;
}

/* Grow the list when it runs out of room for more cached resources. */
static int reserve_one(Cache *cache) {
    size_t next_count;
    PolicyEntry *next;

    if (cache->count < cache->allocated) return 1;
    /* Grow in chunks so the list does not need new memory for every insert. */
    next_count = cache->allocated == 0 ? 8 : cache->allocated * 2;
    if (next_count < cache->allocated ||
        next_count > SIZE_MAX / sizeof(*next)) return 0;
    next = realloc(cache->entries, next_count * sizeof(*next));
    if (next == NULL) return 0;
    cache->entries = next;
    cache->allocated = next_count;
    return 1;
}

/* Add a missed resource. If space is short, ask the selected policy what to remove. */
int cache_insert(Cache *cache,
                const char *key,
                size_t size_bytes,
                unsigned origin_latency_ms,
                const char *client_id,
                const CachePolicy *policy,
                size_t *evictions)
    {
        size_t removed = 0;
        uint64_t request_tick;
        PolicyEntry *entry;

    if (evictions != NULL) {
        *evictions = 0;
    }
    if (cache == NULL || key == NULL) {
        return 0;
    }
    /* Oversized resources are served from the origin but are not stored. */
    if (size_bytes > cache->capacity_bytes) {
        return 0;
    }
    /* Keep one copy of each resource. */
    if (cache_find(cache, key) != NULL)
    { return 1;
    }
    if (!reserve_one(cache)) {
        return -1;
    }

    /*The miss that caused insertion is the next request tick. BFV uses thus while calculating resource ages*/
    request_tick = cache->clock + 1;

    /* A large resource may require removing more than one cached resource. */
    while (cache->capacity_bytes - cache->used_bytes < size_bytes) {
        size_t victim;
        if ( policy == NULL || policy->choose_victim == NULL || cache->count == 0) {
            return -1;
        }
        victim = policy->choose_victim(
            cache->entries,
            cache->count,
            request_tick);
        /* FIFO, LRU, or LFU chooses the resource; the cache removes it. */
        if (
            victim >= cache->count ||
            !cache_remove_at(cache, victim))
            { return -1;
            }
        ++removed;
    }
    /*The insertion request becomes the current tick*/
    cache->clock = request_tick;
    /*Point to the new entry*/
    entry = &cache->entries[cache->count];
    /*A custom policy can initialize all metadata itself*/
    if (policy != NULL && policy->init_entry != NULL) {
        policy->init_entry(
            entry,
            key,
            size_bytes,
            origin_latency_ms,
            client_id,
            cache->clock);
        } else {
        /*Default behavior if no custom policy is provided*/
        entry->key = key;
        entry->size_bytes = size_bytes;
        entry->origin_latency_ms = origin_latency_ms;
        entry->frequency = 1; /* The request that inserts it counts as first use.
        */
        entry->last_access_tick = cache->clock;
        entry->insertion_tick = cache->clock;
        entry->client_sketch = 0; /* The custom policy can fill this in later if it tracks client sharing. */
    }
    /* Store the resource and set its first-use information for the policies. */
    ++cache->count;
    cache->used_bytes += size_bytes;

    if (evictions != NULL) {
        *evictions = removed;
    }
    return 1;
}

