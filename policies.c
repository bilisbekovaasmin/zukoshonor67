#include "policies.h"

#include <stdint.h>

/* FIFO removes the resource that entered the cache earliest. */
size_t fifo_choose_victim(const PolicyEntry entries[], size_t count) {
    if (count == 0) return SIZE_MAX;

    size_t victim = 0;
    for (size_t i = 1; i < count; ++i) {
        if (entries[i].insertion_tick < entries[victim].insertion_tick ||
            (entries[i].insertion_tick == entries[victim].insertion_tick &&
             i < victim)) {
            victim = i;
        }
    }
    return victim;
}

/* LRU removes the resource that has gone the longest without a hit. */
size_t lru_choose_victim(const PolicyEntry entries[], size_t count) {
    if (count == 0) return SIZE_MAX;

    size_t victim = 0;
    for (size_t i = 1; i < count; ++i) {
        if (entries[i].last_access_tick < entries[victim].last_access_tick ||
            (entries[i].last_access_tick == entries[victim].last_access_tick &&
             entries[i].insertion_tick < entries[victim].insertion_tick)) {
            victim = i;
        }
    }
    return victim;
}
