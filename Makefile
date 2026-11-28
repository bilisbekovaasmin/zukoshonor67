CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
LDLIBS ?=

CORE_SOURCES = cache.c policies.c lfu_policy.c custom_policy.c \
               data_loader.c metrics.c simulation.c
SOURCES = main.c $(CORE_SOURCES)
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean test

all: cache_sim

cache_sim: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDLIBS) -o $@

tests/test_lfu: tests/test_lfu.c lfu_policy.c lfu_policy.h policy_types.h
	$(CC) $(CFLAGS) -I. lfu_policy.c tests/test_lfu.c -o $@

tests/test_custom: tests/test_custom.c custom_policy.c custom_policy.h policy_types.h
	$(CC) $(CFLAGS) -I. custom_policy.c tests/test_custom.c -o $@

tests/test_loader: tests/test_loader.c data_loader.c data_loader.h
	$(CC) $(CFLAGS) -I. data_loader.c tests/test_loader.c -o $@

tests/test_metrics: tests/test_metrics.c metrics.c metrics.h
	$(CC) $(CFLAGS) -I. metrics.c tests/test_metrics.c -lm -o $@

tests/test_simulation: tests/test_simulation.c $(CORE_SOURCES)
	$(CC) $(CFLAGS) -I. $(CORE_SOURCES) tests/test_simulation.c -o $@

test: cache_sim tests/test_lfu tests/test_custom tests/test_loader \
      tests/test_metrics tests/test_simulation
	./tests/test_lfu
	./tests/test_custom
	./tests/test_loader
	./tests/test_metrics
	./tests/test_simulation
	python3 tests/test_reference.py

clean:
	rm -f $(OBJECTS) cache_sim tests/test_lfu tests/test_custom \
	      tests/test_loader tests/test_metrics tests/test_simulation \
	      test_metrics_results.csv lfu_trace.csv lfu_metrics.csv
