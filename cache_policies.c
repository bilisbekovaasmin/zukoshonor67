#include "cache_policies.h"

#include "custom_policy.h" //Provides the function prototypes for the custom policy
#include "lfu_policy.h" //Provides the function prototypes for the LFU policy

/*The shared cache passes the current tick to every policy
LFU does not need it, so this adapter deliberately ignores it*/
static size_t lfu_choose_victim_for_cache(
    const PolicyEntry entries[],
    size_t count,
    uint64_t current_tick
) {
    (void)current_tick; // Ignore the current tick for LFU
    return lfu_choose_victim(entries, count);
}

/*LFU uses the cache's normal initialization and hit updates,
so its two optional callbacks are NULL*/
const CachePolicy LFU_CACHE_POLICY = {
    .init_entry = NULL,
    .record_hit = NULL,
    .choose_victim = lfu_choose_victim_for_cache
};

/*BFV supplies all three custom-policy operations */
const CachePolicy BFV_CACHE_POLICY = {
    .init_entry = custom_init_entry,
    .record_hit = custom_record_hit,
    .choose_victim = custom_choose_victim
};