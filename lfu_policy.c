/*Implements the LFU cache replacement policy*/
#include "lfu_policy.h" //Provides the function prototypes for the LFU policy

/*Initializes a resource when it is inserted into the cache */
void lfu_init_entry(PolicyEntry *entry,
                    const char *key,
                    size_t size_bytes,
                    unsigned origin_latency_ms,
                    uint64_t tick
                ) {
                    /*Store the resource's identifying information*/
                    entry->key = key;
                    entry->size_bytes = size_bytes;
                    entry->origin_latency_ms = origin_latency_ms;
                    /*Initialize the frequency to 1, since the insertion counts as the first access*/
                    entry->frequency = 1;
                    /*The insertion request is also the latest access*/
                    entry->last_access_tick = tick;
                    /*Remember when the resource entered the cache*/
                    entry->insertion_tick = tick;
                    /*LFU does not use client_sketch, but we initialize it so
                    the structure never contains an unknown value */
                    entry->client_sketch = 0;

    // Implementation for initializing LFU policy
}

/*Updates LFU information when a resource is found in the cache*/
void lfu_record_hit(PolicyEntry *entry, uint64_t tick) {
    /*Increment the frequency since the resource was accessed again*/
    entry->frequency++;
    /*Update the last accessed tick to the current request number*/
    entry->last_access_tick = tick;

    // Implementation for updating LFU policy
}

/*Selects the cache entry that should be evicted */
size_t lfu_choose_victim(const PolicyEntry entries[], size_t count) {
    if (count == 0) {
        return SIZE_MAX; // Return SIZE_MAX if the array is empty
    }

    size_t victim = 0;
    for (size_t i = 1; i < count; i++) {
        if (entries[i].frequency < entries[victim].frequency ||
            (entries[i].frequency == entries[victim].frequency &&
             entries[i].last_access_tick < entries[victim].last_access_tick)) {
            victim = i; // Update victim index based on LFU criteria
        }
        /*If frequencies are equal, choose the entry whose most recent access happened earlier
        */
       else if (entries[i].frequency == entries[victim].frequency &&
                entries[i].last_access_tick == entries[victim].last_access_tick) {
            if (entries[i].insertion_tick < entries[victim].insertion_tick) {
                victim = i; // Update victim index based on insertion tick as a tie-breaker
            }
        }
    }
    return victim; // Return the index of the selected victim
}