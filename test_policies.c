#include "policies.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef NDEBUG
#error Build these tests with assertions enabled.
#endif

typedef struct {
    PolicyEntry *victims[3];
    size_t count;
} EvictionLog;

static void record_eviction(PolicyEntry *victim, void *context)
{
    EvictionLog *log = context;
    assert(victim->owner == NULL);
    assert(victim->previous == NULL && victim->next == NULL);
    assert(log->count < sizeof log->victims / sizeof log->victims[0]);
    log->victims[log->count++] = victim;
}

static void test_hits_change_only_lru_order(PolicyKind kind)
{
    PolicyCache cache;
    PolicyEntry a = {0};
    PolicyEntry b = {0};
    PolicyEntry c = {0};
    PolicyEntry d = {0};
    EvictionLog log = {0};
    policy_init(&cache, kind, 13);

    /* A (5), B (4), hit A, then C (5): FIFO evicts A, LRU evicts B. */
    assert(policy_admit(&cache, &a, 5, record_eviction, &log));
    assert(policy_admit(&cache, &b, 4, record_eviction, &log));
    assert(policy_touch(&cache, &a));
    assert(cache.used_bytes == 9);
    assert(policy_admit(&cache, &c, 5, record_eviction, &log));
    assert(log.count == 1);
    if (kind == POLICY_FIFO) {
        assert(log.victims[0] == &a);
        assert(cache.head == &b && cache.used_bytes == 9);
    } else {
        assert(log.victims[0] == &b);
        assert(cache.head == &a && cache.used_bytes == 10);
    }
    assert(cache.tail == &c);
    assert(policy_touch(&cache, &c));
    assert(cache.tail == &c);

    /* An insertion counts as the newest use, even without a later hit. */
    assert(policy_admit(&cache, &d, 5, record_eviction, &log));
    assert(log.count == 2 && log.victims[1] != &c);
    assert(cache.head == &c && cache.tail == &d && cache.used_bytes == 10);
}

static void test_byte_capacity_and_reuse(PolicyKind kind)
{
    PolicyCache cache;
    PolicyEntry a = {0};
    PolicyEntry b = {0};
    PolicyEntry c = {0};
    PolicyEntry d = {0};
    EvictionLog log = {0};
    policy_init(&cache, kind, 10);

    assert(policy_admit(&cache, &a, 2, record_eviction, &log));
    assert(policy_admit(&cache, &b, 3, record_eviction, &log));
    assert(policy_admit(&cache, &c, 4, record_eviction, &log));
    assert(!policy_admit(&cache, &d, 11, record_eviction, &log));
    assert(!policy_admit(&cache, &b, 8, record_eviction, &log));
    assert(log.count == 0 && cache.used_bytes == 9);
    assert(cache.head == &a && cache.tail == &c);
    assert(!policy_touch(&cache, &d));
    assert(!policy_remove(&cache, &d));

    /* D (6) needs five more bytes, so both A and B must go. */
    assert(policy_admit(&cache, &d, 6, record_eviction, &log));
    assert(log.count == 2);
    assert(log.victims[0] == &a && log.victims[1] == &b);
    assert(cache.head == &c && cache.tail == &d && cache.used_bytes == 10);

    assert(policy_remove(&cache, &d));
    assert(cache.head == &c && cache.tail == &c && c.next == NULL);
    assert(policy_admit(&cache, &a, 6, record_eviction, &log));
    assert(log.count == 2 && cache.used_bytes == 10);
    assert(policy_remove(&cache, &c));
    assert(policy_remove(&cache, &a));
    assert(cache.head == NULL && cache.tail == NULL && cache.used_bytes == 0);
}

static void test_capacity_limits(void)
{
    PolicyCache empty;
    PolicyCache huge;
    PolicyEntry a = {0};
    PolicyEntry b = {0};
    EvictionLog log = {0};
    policy_init(&empty, POLICY_LRU, 0);
    policy_init(&huge, POLICY_FIFO, SIZE_MAX);

    assert(!policy_admit(&empty, &a, 1, record_eviction, &log));
    assert(empty.head == NULL && log.count == 0);
    assert(policy_admit(&huge, &a, SIZE_MAX, record_eviction, &log));
    assert(!policy_touch(&empty, &a));
    assert(!policy_remove(&empty, &a));
    assert(!policy_admit(&empty, &a, 0, record_eviction, &log));
    assert(policy_admit(&huge, &b, 1, record_eviction, &log));
    assert(log.count == 1 && log.victims[0] == &a && huge.used_bytes == 1);
}

typedef struct {
    bool hit;
    const char *evicted;
    const char *resident;
    size_t used_kib;
} ReferenceStep;

static void test_reference_trace(PolicyKind kind)
{
    const char resource_ids[] = "ABCDE";
    const size_t size_kib[] = {32, 42, 14, 24, 57};
    const char requests[] = "ABCABDACEBDAECB";

    /* Each row records the expected hit, victim order, residents, and KiB
     * from the matching expected CSV. The resident column checks membership.
     * The LRU list itself is ordered by access. */
    const ReferenceStep fifo[] = {
        {false, "",   "A",   32},
        {false, "",   "AB",  74},
        {false, "",   "ABC", 88},
        {true,  "",   "ABC", 88},
        {true,  "",   "ABC", 88},
        {false, "A",  "BCD", 80},
        {false, "B",  "CDA", 70},
        {true,  "",   "CDA", 70},
        {false, "CD", "AE",  89},
        {false, "A",  "EB",  99},
        {false, "E",  "BD",  66},
        {false, "",   "BDA", 98},
        {false, "BD", "AE",  89},
        {false, "A",  "EC",  71},
        {false, "E",  "CB",  56},
    };
    const ReferenceStep lru[] = {
        {false, "",   "A",   32},
        {false, "",   "AB",  74},
        {false, "",   "ABC", 88},
        {true,  "",   "ABC", 88},
        {true,  "",   "ABC", 88},
        {false, "C",  "ABD", 98},
        {true,  "",   "ABD", 98},
        {false, "B",  "ADC", 70},
        {false, "DA", "CE",  71},
        {false, "C",  "EB",  99},
        {false, "E",  "BD",  66},
        {false, "",   "BDA", 98},
        {false, "BD", "AE",  89},
        {false, "A",  "EC",  71},
        {false, "E",  "CB",  56},
    };
    const ReferenceStep *expected = kind == POLICY_FIFO ? fifo : lru;
    PolicyCache cache;
    PolicyEntry entries[sizeof resource_ids - 1] = {0};
    policy_init(&cache, kind, 100 * 1024);

    for (size_t step = 0; step < sizeof requests - 1; ++step) {
        size_t index = (size_t)(strchr(resource_ids, requests[step]) - resource_ids);
        PolicyEntry *entry = &entries[index];
        EvictionLog log = {0};
        bool hit = entry->owner == &cache;

        assert(hit == expected[step].hit);
        if (hit) {
            assert(policy_touch(&cache, entry));
        } else {
            assert(policy_admit(&cache, entry, size_kib[index] * 1024,
                                record_eviction, &log));
        }

        assert(cache.used_bytes == expected[step].used_kib * 1024);
        assert(log.count == strlen(expected[step].evicted));
        for (size_t i = 0; i < log.count; ++i) {
            assert(resource_ids[log.victims[i] - entries] == expected[step].evicted[i]);
        }
        for (size_t i = 0; i < sizeof entries / sizeof entries[0]; ++i) {
            bool should_be_resident = strchr(expected[step].resident, resource_ids[i]) != NULL;
            assert((entries[i].owner == &cache) == should_be_resident);
        }
    }
}

int main(void)
{
    test_hits_change_only_lru_order(POLICY_FIFO);
    test_hits_change_only_lru_order(POLICY_LRU);
    test_byte_capacity_and_reuse(POLICY_FIFO);
    test_byte_capacity_and_reuse(POLICY_LRU);
    test_capacity_limits();
    test_reference_trace(POLICY_FIFO);
    test_reference_trace(POLICY_LRU);
    puts("All FIFO and LRU tests passed.");
    return 0;
}
