#include <assert.h>
#include <stdio.h>
#include <types.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>
#include <kernel/sched/task.h>
#include <task.h>

// Test state tracking
static struct {
	int write_faults;
	int cow_events;

	uintptr_t current_cow_va;
	bool called_memcpy;
	bool called_tlb;
	bool called_fork;
} test_state = {0};

static void pf_handle(struct probe_frame *frame) {
	struct task *task = (struct task *)frame->rdi;
	void *va = (void *)frame->rsi;
	int fault_flags = (int)frame->rdx;

	/* Only check write faults and only if we have a valid task */
	if (!(fault_flags & PAGE_WRITE) || !task || !task->task_pml4)
		return;

	test_state.write_faults++;

	// If the current page does not have W permissions, this is a CoW
	// fault, so we expect a memcpy and tlb_invalidate afterwards

	physaddr_t *entry = NULL;
	struct page_info *page = page_lookup(task->task_pml4, va, &entry);

	if (page && entry) {
		if(!(*entry & PAGE_WRITE) && page->pp_ref > 1) {
			test_state.cow_events++;
			test_state.current_cow_va = (uintptr_t)va;
			test_state.called_memcpy = false;
			test_state.called_tlb = false;
		}
	}
}

static void memcpy_handle(struct probe_frame *frame) {
	size_t size = (size_t)frame->rdx;

	/* Check if this looks like a CoW memcpy (PAGE_SIZE copy) */
	if (size != PAGE_SIZE || test_state.current_cow_va == 0)
		return;

	test_state.called_memcpy = true;
	assert(!test_state.called_tlb);
}

static void tlb_handle(struct probe_frame *frame) {
	// If we do not currently handle a CoW event, return
	if(test_state.current_cow_va == 0)
		return;

	if (!cur_task || !cur_task->task_pml4) {
		test_state.current_cow_va = 0;
		return;
	}

	// Find the CoW page and check that its flags have changed
	physaddr_t *entry;
	struct page_info *page = page_lookup(cur_task->task_pml4,
	                                     (void *)test_state.current_cow_va, &entry);

	if (!page || !entry) {
		test_state.current_cow_va = 0;
		return;
	}

	uint64_t flags = *entry & PAGE_MASK;

	// Check if the new page is now writable and private
	assert(flags & PAGE_WRITE);
	assert(page->pp_ref == 1); // this is the new page, with refcount 1
	assert(test_state.called_memcpy);

	// Clear
	test_state.current_cow_va = 0;
}

static void fork_handle(struct probe_frame *frame) {
	test_state.called_fork = true;
}

static int run_test(struct probe_frame *frame) {
	if (!test_state.called_fork)
		panic("No fork syscall was observed during test execution");

	if (test_state.write_faults == 0)
		panic("Expected write faults to trigger CoW, but saw none");

	return __checksum__;
}

extern void halt_kernel();
extern void sys_fork();
extern void task_page_fault_handler();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,

	.probe_count = 4,
	.probes = {
		{
			.target = sys_fork,
			.callback = fork_handle
		},
		{
			.target = task_page_fault_handler,
			.callback = pf_handle
		},
		{
			.target = memcpy,
			.callback = memcpy_handle
		},
		{
			.target = tlb_invalidate,
			.callback = tlb_handle
		}
	},
};
