#include <assert.h>
#include <stdio.h>

#include <kernel/test/test.h>

#include "x86-64/paging.h"

static int pf_count = 0;
static bool mmap_called = false;
static uint64_t pf_va = 0x1000000;

static void mmap_handle(struct probe_frame *frame) {
	mmap_called = true;
}

static void pf_handle(struct probe_frame *frame) {
	if(frame->rsi >= pf_va && frame->rsi <= pf_va + HPAGE_SIZE)
		pf_count++;
}

static int run_test(struct probe_frame *frame) {
	assert(mmap_called);

	cprintf("Detected %d page faults in the mapped area, expected 3\n", pf_count);
	assert(pf_count == 3);

	return __checksum__;
}

extern void halt_kernel();
extern void sys_mmap();
extern void task_page_fault_handler();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = true,
	.checksum = __checksum__,

	.probe_count = 2,
	.probes = {
		{
			.target = sys_mmap,
			.callback = mmap_handle
		},
		{
			.target = task_page_fault_handler,
			.callback = pf_handle
		},
	},
};
