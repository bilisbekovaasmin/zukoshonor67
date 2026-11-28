#ifndef CACHE_POLICIES_H
#define CACHE_POLICIES_H

#include "cache.h"

/*LFU uses the cache's default insertion and hit handling */
extern const CachePolicy LFU_CACHE_POLICY;
/*BFV uses its own initialization, hit recording, and scoring */
extern const CachePolicy BFV_CACHE_POLICY;

#endif //CACHE_POLICIES_H
