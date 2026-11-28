#ifndef POLICY_TYPES_H
#define POLICY_TYPES_H //Preventing this header from being included twice

#include <stddef.h> //Provides the size_t data type
#include <stdint.h> //Provides fixed-size types such as uint32_t

/*A structure containing information about one cached resource*/
typedef struct {
    /*A name or URL used to identify the resource*/
    const char *key;         //The name or URL of the cached resource
    size_t size_bytes;        //The size of the cached resource
    /*Time required to download the resource from its original server*/
    unsigned origin_latency_ms; //The time required to download the resource from its original server
    uint64_t frequency; //The number of times the resource has been accessed
    /*
     * Request number of the most recent access.
     * LFU uses this value for the required LRU tie-break.
     */
    uint64_t last_access_tick; //The request number of the most recent access
    /*Request number when the resource entered the cache
    * This provides a final predictable tie-break if necessary
    */
    uint64_t insertion_tick; //The request number when the resource entered the cache
    /*Compact approximation of which clients requested this resource
    *The custom policy uses it to reward resources shared by clients
    */
   uint64_t client_sketch; //A compact approximation of which clients requested this resource
} PolicyEntry; //Creates the PolicyEntry type, which is a structure containing information about one cached resource

#endif //POLICY_TYPES_H
