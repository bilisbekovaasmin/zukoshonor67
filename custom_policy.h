/*Declares the functions used by the custom balanced fetch value cache replacement policy
*/

#ifndef CUSTOM_POLICY_H
#define CUSTOM_POLICY_H //Preventing this header from being included twice

#include "policy_types.h" //Provides the PolicyEntry structure
#include <stddef.h> //Provides the size_t data type
#include <stdint.h> //Provides fixed-size types such as uint32_t

/*initializes custom policy metadata when a resource is inserted into the cache */
void custom_init_entry(PolicyEntry *entry,
                       const char *key,
                       size_t size_bytes,
                       unsigned origin_latency_ms,
                        const char *client_id,
                       uint64_t tick
                   );

/*Records a cache hit. It increases frequency, updates rececy, and remembers the requesting client */
void custom_record_hit(
    PolicyEntry *entry, 
    const char *client_id,
    uint64_t tick 
);

/*Calculates how valuable a resource is to the cache.
A larger score means the resource is more valuable and should recieve more protection from eviction*/

double custom_calculate_score(
    const PolicyEntry *entry,
    uint64_t current_tick 
);

/*Calculates every rrsource's score and returns the array index of the resource's score and returns the array index of the resource with the lowest score
Equal scores use LRU as the tiebreaker
returns SIZE_MAX if the array is empty 
*/
size_t custom_choose_victim(
    const PolicyEntry entries[],
    size_t count,
    uint64_t current_tick
);
#endif // CUSTOM_POLICY_H
