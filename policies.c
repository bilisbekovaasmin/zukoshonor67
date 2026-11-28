#include "policies.h"

/* The list represents age directly, so requests need no timestamps. */
static void append_entry(PolicyCache *cache, PolicyEntry *entry)
{
    entry->previous = cache->tail;
    entry->next = NULL;

    if (cache->tail != NULL) {
        cache->tail->next = entry;
    } else {
        cache->head = entry;
    }
    cache->tail = entry;
}

static void unlink_entry(PolicyCache *cache, PolicyEntry *entry)
{
    if (entry->previous != NULL) {
        entry->previous->next = entry->next;
    } else {
        cache->head = entry->next;
    }

    if (entry->next != NULL) {
        entry->next->previous = entry->previous;
    } else {
        cache->tail = entry->previous;
    }

    entry->previous = NULL;
    entry->next = NULL;
}

void policy_init(PolicyCache *cache, PolicyKind kind, size_t capacity_bytes)
{
    *cache = (PolicyCache){
        .kind = kind,
        .capacity_bytes = capacity_bytes
    };
}

bool policy_touch(PolicyCache *cache, PolicyEntry *entry)
{
    if (entry->owner != cache) {
        return false;
    }

    if (cache->kind == POLICY_LRU && entry != cache->tail) {
        unlink_entry(cache, entry);
        append_entry(cache, entry);
    }

    return true;
}

bool policy_remove(PolicyCache *cache, PolicyEntry *entry)
{
    if (entry->owner != cache) {
        return false;
    }

    unlink_entry(cache, entry);
    cache->used_bytes -= entry->size_bytes;
    entry->owner = NULL;
    return true;
}

bool policy_admit(PolicyCache *cache, PolicyEntry *entry, size_t size_bytes,
                  PolicyEvictionFn on_evict, void *context)
{
    if (entry->owner != NULL || size_bytes > cache->capacity_bytes) {
        return false;
    }

    /* Compare against free space to avoid overflowing a byte sum. */
    while (size_bytes > cache->capacity_bytes - cache->used_bytes) {
        PolicyEntry *victim = cache->head;
        policy_remove(cache, victim);
        if (on_evict != NULL) {
            on_evict(victim, context);
        }
    }

    entry->size_bytes = size_bytes;
    entry->owner = cache;
    append_entry(cache, entry);
    cache->used_bytes += size_bytes;
    return true;
}
