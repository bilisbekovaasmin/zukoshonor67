#ifndef CACHE_POLICIES_H
#define CACHE_POLICIES_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    POLICY_FIFO,
    POLICY_LRU
} PolicyKind;

typedef struct PolicyCache PolicyCache;
typedef struct PolicyEntry PolicyEntry;

/*
 * Embed a PolicyEntry in each resource record, or keep one alongside it.
 * Zero-initialize it before first use. The caller owns the record and must
 * keep it at a stable address while it is cached. Treat these fields as
 * read-only outside this module.
 */
struct PolicyEntry {
    size_t size_bytes;
    PolicyEntry *previous;
    PolicyEntry *next;
    PolicyCache *owner;
};

/*
 * The head is the next victim and the tail is the newest insertion (FIFO)
 * or most recent access (LRU). Treat these fields as read-only as well.
 */
struct PolicyCache {
    PolicyKind kind;
    size_t capacity_bytes;
    size_t used_bytes;
    PolicyEntry *head;
    PolicyEntry *tail;
};

/* Called once per detached victim, in eviction order. It may release the
 * victim but must not mutate this policy cache or the incoming entry. */
typedef void (*PolicyEvictionFn)(PolicyEntry *victim, void *context);

/* Initialize a new cache with POLICY_FIFO or POLICY_LRU. Keep its address
 * stable and remove its entries before reinitializing or discarding it. */
void policy_init(PolicyCache *cache, PolicyKind kind, size_t capacity_bytes);

/* For a hit, call this with the entry found by the simulator's lookup.
 * Returns false if the entry does not belong to this cache. */
bool policy_touch(PolicyCache *cache, PolicyEntry *entry);

/*
 * For a miss, admit a detached entry. Returns false without changing the
 * cache if the entry is already attached or exceeds total capacity.
 * Otherwise, evicts as many entries as needed and adds the entry at the
 * tail. Pass a callback to keep the simulator's lookup and eviction log
 * in sync, or pass NULL when no notification is needed.
 */
bool policy_admit(PolicyCache *cache, PolicyEntry *entry, size_t size_bytes,
                  PolicyEvictionFn on_evict, void *context);

/* Explicitly detach an entry, e.g. during cleanup. Returns false if the
 * entry does not belong to this cache. The detached entry can be reused. */
bool policy_remove(PolicyCache *cache, PolicyEntry *entry);

#endif
