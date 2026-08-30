#pragma once

#include <types.h>
#include <kernel/symbols.h>
#include <kernel/test/probe.h>

typedef int (*test_callback)(struct probe_frame *);

struct test_definition {
	test_callback run_test;
	void *test_point;

	bool should_continue;
	int checksum;

	int symbol_count;
	struct symbol_def *symbols;

	int probe_count;
	struct probe probes[];
};

void tests_init();
