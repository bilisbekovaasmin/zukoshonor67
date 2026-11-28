#ifndef POLICIES_H
#define POLICIES_H

#include "policy_types.h"

/* Return the array index of the entry selected for eviction. */
size_t fifo_choose_victim(const PolicyEntry entries[], size_t count);
size_t lru_choose_victim(const PolicyEntry entries[], size_t count);

#endif
