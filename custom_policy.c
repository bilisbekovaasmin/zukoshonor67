/*Custom policy implementation 
Implements the custom balanced fetch value cache replacement policy
*/

#include "custom_policy.h" //Provides the function prototypes for the custom policy

#include <stdint.h> //Provides fixed-size types such as uint64_t

/*
Converts a client ID string into a repeatable number 

"student1 will always produce the same hash value
This is the FNV-1a hash algorithm*/
static uint64_t hash_client_id(const char *client_id) {
    uint64_t hash = UINT64_C(14695981039346656037); // FNV offset basis
    if (client_id == NULL) {
    return 0;
}
    while (*client_id) {
        hash ^= (unsigned char)(*client_id++); // XOR with the byte
        hash *= UINT64_C(1099511628211); // FNV prime
    }
    return hash;
}

/*Records approximately which clients used a resource*/
static void update_client_sketch(PolicyEntry *entry, 
    const char *client_id) {
    uint64_t hash = hash_client_id(client_id);
    entry->client_sketch |= UINT64_C(1) << (hash & 63U);   // Update the client sketch using bitwise OR
}

/*Initializes an entry when a cache miss imserts a resource */
void custom_init_entry(PolicyEntry *entry,
                       const char *key,
                       size_t size_bytes,
                       unsigned origin_latency_ms,
                       const char *client_id,
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
    /*Initialize client sketch to 0, as no clients have accessed it yet*/
    entry->client_sketch = 0;
    /*Record the client responsible for this insertion*/
    update_client_sketch(entry, client_id);
}
    /*Updates the entry when a resource is found in the cache*/
void custom_record_hit(
    PolicyEntry *entry,
    const char *client_id,
    uint64_t tick
) {
    /*Increment the frequency since the resource was accessed again*/
    entry->frequency++;
    /*Update the last accessed tick to the current request number*/
    entry->last_access_tick = tick;
    /*Record the client responsible for this hit*/
    update_client_sketch(entry, client_id);
}   

/*Counts how many client bits are switched on
Each switched-on bit represents a unique client that has accessed the resource*/
static size_t count_client_bits(uint64_t bits) {
    size_t count = 0;
    while (bits !=0) { //removes the lowest switched on bit (1100 becomes 1000)
        bits &= bits - 1;
        count++;
    }
    return count;
}
/*Calculates the value of a resource based on its frequency, recency, and client diversity*/
static unsigned integer_log2(uint64_t value) {
    unsigned result = 0;
    while (value > 1) {
        value >>= 1;
        result++;
    }
    return result;
}

/*Calculates the final score for a resource based on its attributes*/
double custom_calculate_score(
    const PolicyEntry *entry,
    uint64_t current_tick
) {
    // Implementation for calculating the score
    double size_kib;
    double latency_per_kib;
    double frequency_factor;
    double fairness_factor;
    double recency_factor;

    uint64_t age;
    unsigned clients;

    /*Coverts bytes to KiB
    Use 1 KiB if size is zero to avoid division by zero
    */
   if (entry->size_bytes == 0) {
        size_kib = 1.0;
    } else {
        size_kib = (double)entry->size_bytes / 1024.0;
    }
    /*Calculates how many request ticks have passed since the resource's latest access
    */
   if (current_tick > entry->last_access_tick) {
        age = current_tick - entry->last_access_tick; // Prevent negative age if current_tick is less than last_access_tick
    } else {
        age = 0;
    }

    /*Estimates the number of unique clients that have accessed the resource*/
    clients = count_client_bits(entry->client_sketch);

    /*limits the fairness bonus to five clients*/
    if (clients > 5) {
        clients = 5;
    }

    /*Expensive, small resources receive a higher score*/
    latency_per_kib = (double)entry->origin_latency_ms / size_kib;
    /*Reward repeated accesses
     This prevents an old popular resource from becoming immortal
    */frequency_factor = 1.0+(double)integer_log2(entry->frequency);
    /*Give a 25% bonus to resources with high client diversity*/
    fairness_factor =
    1.0 + 0.25 * (double)(clients > 0 ? clients - 1 : 0);
    /*Reward recent accesses*/
    recency_factor = 1.0 / (1.0 + (double)age/8.0);
    /*Calculate the final score*/
    return latency_per_kib * 
            frequency_factor * 
            fairness_factor * 
            recency_factor;
}
/*Selects the resource with the lowest retention score
If scores are equal, LRU is used as a tiebreaker*/
size_t custom_choose_victim(
    const PolicyEntry entries[],
    size_t count,
    uint64_t current_tick
) {
    if (count == 0) {
        return SIZE_MAX; // Return SIZE_MAX if the array is empty
    }

    size_t victim = 0;
    double victim_score = 
        custom_calculate_score(&entries[0], current_tick);

    for (size_t i = 1; i < count; i++) {
        double score = 
            custom_calculate_score(&entries[i], current_tick); //Compare every remaining entry with the current victim
            //Primary custom policy rule: the entry with the lowest value is evicted
        if (score < victim_score) {
            victim = i; // Update victim index based on score
            victim_score = score; // Update the lowest score
        }
           /*LRU tiebreaker
           If scores are equal, choose the entry that was accessed less recently
           */
          else if (
                score == victim_score &&
                entries[i].last_access_tick < 
                    entries[victim].last_access_tick) {
            victim = i; // Update victim index based on LRU tiebreaker
            victim_score = score; // Update the lowest score
    }
    //Final deterministic tie-break: If score and last access are equal, choose the resource inserted earlier 
    else if(
        score == victim_score &&
        entries[i].last_access_tick == 
            entries[victim].last_access_tick &&
        entries[i].insertion_tick < 
            entries[victim].insertion_tick
    ) {
        victim = i; // Update victim index based on insertion tick as a final tiebreaker
        victim_score = score; // Update the lowest score
    }
}
    return victim; // Return the index of the selected victim
}
