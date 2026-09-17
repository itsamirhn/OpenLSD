/*
 * Test: Non-anonymous VMAs should merge when backing memory is continuous
 *
 * This test verifies that two adjacent non-anonymous VMAs
 * merge when they have the same permissions and their backing memory is
 * exactly consecutive.
 *
 * We create two non-anonymous VMAs at consecutive addresses, both with
 * VM_READ | VM_WRITE, where the second VMA's backing data starts
 * immediately after the first VMA's backing data ends. We then verify
 * that the two VMAs have been merged into a single VMA spanning both
 * ranges with the combined backing length.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <kernel/test/test.h>
#include <kernel/sched.h>
#include <kernel/vma/insert.h>
#include <kernel/vma/show.h>
#include <kernel/vma/find.h>
#include <kernel/mem/buddy.h>
#include <vma.h>
#include <paging.h>

/* Test constants - magic numbers for addresses */
#define TEST_BASE_ADDR       ((void *)0x1000000)  /* Base address for test VMAs */
static bool vmas_created = false;
static char *test_src_data;

static void handle_task_run(struct probe_frame *frame) {
	/* Only create VMAs once, for the first user task (PID 1) */
	if (vmas_created)
		return;
	
	/* Get the task from task_run parameter (rdi in x86-64 calling convention) */
	struct task *task = (struct task *)frame->rdi;
	if (!task || task->task_pid != 1)
		return;
	
	/* Test source data for non-anonymous VMA */
	test_src_data = page2kva(page_alloc(ALLOC_HUGE | ALLOC_ZERO));
	memset(test_src_data, 0xAA, PAGE_SIZE * 2);
	
	// Dobule check that our data is aligned
	assert(test_src_data - ROUNDDOWN(test_src_data, PAGE_SIZE) == 0);

	/* Create non-anonymous VMA with offset */
	assert(add_executable_vma(task, "nonanon1", TEST_BASE_ADDR,
		PAGE_SIZE, VM_READ | VM_WRITE, test_src_data, PAGE_SIZE - 0x100, 0x100) != NULL);
	
	/* Create another non-anonymous VMA with continuous backing */
	assert(add_executable_vma(task, "nonanon2", TEST_BASE_ADDR + PAGE_SIZE,
		PAGE_SIZE, VM_READ | VM_WRITE, test_src_data + PAGE_SIZE - 0x100, PAGE_SIZE, 0) != NULL);
	
	vmas_created = true;
}

static int run_test(struct probe_frame *frame) {
	assert(vmas_created);

	struct task *task = (struct task *)frame->rdi;
	if (!task || task->task_pid != 1)
		return 0;

	struct vma *vma = task_find_vma(task, TEST_BASE_ADDR);

	assert(vma);
	assert(vma->vm_len == (2 * PAGE_SIZE) - 0x100);
	assert(vma->vm_src == test_src_data);

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
