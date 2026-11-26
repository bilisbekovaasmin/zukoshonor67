#include "cache.h"

#include <stdlib.h>
#include <string.h>

int cache_init(Cache *cache, size_t capacity_bytes) {
    if (cache == NULL || capacity_bytes == 0) return 0;
    cache->entries = NULL;
    cache->capacity_bytes = capacity_bytes;
    cache->used_bytes = 0;
    cache->count = 0;
    cache->allocated = 0;
    cache->clock = 0;
    return 1;
}

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

const PolicyEntry *cache_find(const Cache *cache, const char *key) {
    size_t i;
    if (cache == NULL || key == NULL) return NULL;
    for (i = 0; i < cache->count; ++i)
        if (cache->entries[i].key != NULL &&
            strcmp(cache->entries[i].key, key) == 0)
            return &cache->entries[i];
    return NULL;
}

PolicyEntry *cache_lookup(Cache *cache, const char *key) {
    size_t i;
    if (cache == NULL || key == NULL) return NULL;
    for (i = 0; i < cache->count; ++i) {
        PolicyEntry *entry = &cache->entries[i];
        if (entry->key != NULL && strcmp(entry->key, key) == 0) {
            ++cache->clock;
            if (entry->frequency < UINT64_MAX) ++entry->frequency;
            entry->last_access_tick = cache->clock;
            return entry;
        }
    }
    return NULL;
}

int cache_remove_at(Cache *cache, size_t index) {
    if (cache == NULL || index >= cache->count) return 0;
    cache->used_bytes -= cache->entries[index].size_bytes;
    if (index + 1 < cache->count)
        memmove(&cache->entries[index], &cache->entries[index + 1],
                (cache->count - index - 1) * sizeof(*cache->entries));
    --cache->count;
    return 1;
}

static int reserve_one(Cache *cache) {
    size_t next_count;
    PolicyEntry *next;
    if (cache->count < cache->allocated) return 1;
    next_count = cache->allocated == 0 ? 8 : cache->allocated * 2;
    if (next_count < cache->allocated ||
        next_count > SIZE_MAX / sizeof(*next)) return 0;
    next = realloc(cache->entries, next_count * sizeof(*next));
    if (next == NULL) return 0;
    cache->entries = next;
    cache->allocated = next_count;
    return 1;
}

int cache_insert(Cache *cache, const char *key, size_t size_bytes,
                 unsigned origin_latency_ms, CacheVictimSelector select_victim,
                 size_t *evictions) {
    size_t removed = 0;
    PolicyEntry *entry;

    if (evictions != NULL) *evictions = 0;
    if (cache == NULL || key == NULL) return 0;
    if (size_bytes > cache->capacity_bytes) return 0;
    if (cache_find(cache, key) != NULL) return 1;
    if (!reserve_one(cache)) return -1;

    while (cache->capacity_bytes - cache->used_bytes < size_bytes) {
        size_t victim;
        if (select_victim == NULL || cache->count == 0) return -1;
        victim = select_victim(cache->entries, cache->count);
        if (victim >= cache->count || !cache_remove_at(cache, victim)) return -1;
        ++removed;
    }

    ++cache->clock;
    entry = &cache->entries[cache->count];
    entry->key = key;
    entry->size_bytes = size_bytes;
    entry->origin_latency_ms = origin_latency_ms;
    entry->frequency = 1; /* The request that inserts it counts as first use. */
    entry->last_access_tick = cache->clock;
    entry->insertion_tick = cache->clock;
    entry->client_sketch = 0;
    ++cache->count;
    cache->used_bytes += size_bytes;
    if (evictions != NULL) *evictions = removed;
    return 1;
}
