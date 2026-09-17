/*
 * Test: Non-anonymous VMAs should not merge with anonymous VMAs
 *
 * This test verifies that a non-anonymous VMA does not merge
 * with adjacent anonymous VMAs, even when all three have the same
 * permissions.
 *
 * We create two anonymous VMAs with a non-anonymous VMA
 * in between them, all with VM_READ | VM_WRITE, and verify
 * that all three remain separate.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <kernel/test/test.h>
#include <kernel/sched.h>
#include <kernel/vma/insert.h>
#include <kernel/vma/show.h>
#include <kernel/mem/buddy.h>
#include <vma.h>
#include <paging.h>

/* Test constants - magic numbers for addresses */
#define TEST_BASE_ADDR       ((void *)0x1000000)  /* Base address for test VMAs */
static bool vmas_created = false;

static void handle_task_run(struct probe_frame *frame) {
	/* Only create VMAs once, for the first user task (PID 1) */
	if (vmas_created)
		return;
	
	/* Get the task from task_run parameter (rdi in x86-64 calling convention) */
	struct task *task = (struct task *)frame->rdi;
	if (!task || task->task_pid != 1)
		return;
	
	/* Test source data for non-anonymous VMA */
	char *test_src_data = page2kva(page_alloc(0));
	memset(test_src_data, 0xAA, PAGE_SIZE);
	
	// Dobule check that our data is aligned
	assert(test_src_data - ROUNDDOWN(test_src_data, PAGE_SIZE) == 0);

	/* Create first anonymous VMA at base */
	assert(add_anonymous_vma(task, "anon1", TEST_BASE_ADDR,
		PAGE_SIZE, VM_READ | VM_WRITE) != NULL);
	
	/* Create non-anonymous VMA in the middle */
	assert(add_executable_vma(task, "nonanon", TEST_BASE_ADDR + PAGE_SIZE,
		PAGE_SIZE, VM_READ | VM_WRITE, test_src_data, PAGE_SIZE, 0) != NULL);
	
	/* Create second anonymous VMA at base + PAGE_SIZE * 2 */
	assert(add_anonymous_vma(task, "anon2", TEST_BASE_ADDR + (2 * PAGE_SIZE),
		PAGE_SIZE, VM_READ | VM_WRITE) != NULL);
	
	vmas_created = true;
}

static int run_test(struct probe_frame *frame) {
	/* Test point - just verify the test ran */
	assert(vmas_created);
	return __checksum__;
}

extern void task_run(struct task *task);
extern void task_destroy(struct task *task);

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = task_destroy,
	.should_continue = true,
	.checksum = __checksum__,
	.probe_count = 1,
	.probes = {
		{
			.target = task_run,
			.callback = handle_task_run,
		}
	},
};

