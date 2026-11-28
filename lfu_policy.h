#ifndef LFU_POLICY_H
#define LFU_POLICY_H //Preventing this header from being included twice

#include "policy_types.h" //Provides the PolicyEntry structure
#include <stddef.h> //Provides the size_t data type
#include <stdint.h> //Provides fixed-size types such as uint32_t

/*Initializes policy information when a resource enters the cache
*The request that caused the insertion counts as the first access, so its initial frequency will be 1 */
void lfu_init_entry(PolicyEntry *entry,
                    const char *key,
                    size_t size_bytes,
                    unsigned origin_latency_ms,
                    uint64_t tick
                );
/*Examines every cached entry and returns the array index of the resource
that should be evicted.

Selection rules:
1. Choose the lowest frequency
2. If frequencies are equal, choose the one with the longest time since last access
Returns SIZE_MAX if the array is empty
*/
void lfu_record_hit(PolicyEntry *entry, uint64_t tick);
size_t lfu_choose_victim(
    const PolicyEntry entries[],
    size_t count);
#endif // LFU_POLICY_H

