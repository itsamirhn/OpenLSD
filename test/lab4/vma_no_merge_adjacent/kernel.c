/*
 * Test: Adjacent non-anonymous VMAs should not merge when permissions
 * differ or backing memory is not contiguous
 *
 * This test verifies that three adjacent non-anonymous VMAs
 * do not merge when either their permissions differ or their backing
 * memory is not contiguous.
 *
 * We create six non-anonymous VMAs at consecutive addresses
 * and verify all three remain separate.
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
static char *test_src_data = NULL;

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
	memset(test_src_data, 0xAA, PAGE_SIZE);
	memset(test_src_data + PAGE_SIZE, 0xBB, PAGE_SIZE);
	memset((test_src_data + (PAGE_SIZE * 2)) + 0x10, 0xCC, PAGE_SIZE - 0x10);
	memset((test_src_data + (PAGE_SIZE * 3)), 0xDD, PAGE_SIZE);
	memset((test_src_data + (PAGE_SIZE * 4)), 0xEE, PAGE_SIZE);
	memset((test_src_data + (PAGE_SIZE * 5)), 0xFF, PAGE_SIZE);
	
	// Dobule check that our data is aligned
	assert(test_src_data - ROUNDDOWN(test_src_data, PAGE_SIZE) == 0);

	/* Create first non-anonymous VMA at base */
	assert(add_executable_vma(task, "nonanon1", TEST_BASE_ADDR,
		PAGE_SIZE, VM_READ | VM_WRITE, test_src_data, PAGE_SIZE, 0) != NULL);

	/* Create non-anonymous VMA with different permissions */
	assert(add_executable_vma(task, "nonanon2", TEST_BASE_ADDR + PAGE_SIZE,
		PAGE_SIZE, VM_READ, test_src_data + PAGE_SIZE, PAGE_SIZE, 0) != NULL);
	
	/* Create non-anonymous VMA at base + PAGE_SIZE * 2 with different non contiguous backing memory */
	assert(add_executable_vma(task, "nonanon3", TEST_BASE_ADDR + PAGE_SIZE * 2,
		PAGE_SIZE, VM_READ |VM_WRITE, 
		test_src_data + (PAGE_SIZE * 2) + 0x10, PAGE_SIZE - 0x10, 0) != NULL);
	
	/* Create non-anonymous VMA at base + PAGE_SIZE * 3 with non zero rhs offset */
	assert(add_executable_vma(task, "nonanon4", TEST_BASE_ADDR + PAGE_SIZE * 3,
		PAGE_SIZE, VM_READ | VM_WRITE, 
		test_src_data + (PAGE_SIZE * 3), PAGE_SIZE - 0x100, 0x100) != NULL);

	/* Create non-anonymous VMA at base + PAGE_SIZE * 4 with non zero rhs offset */
	assert(add_executable_vma(task, "nonanon5", TEST_BASE_ADDR + PAGE_SIZE * 4,
		PAGE_SIZE * 2, VM_READ, 
		test_src_data + (PAGE_SIZE * 4), PAGE_SIZE, 0) != NULL);
	
	/* Create non-anonymous VMA at base + PAGE_SIZE * 6 for lhs part of previous check */
	assert(add_executable_vma(task, "nonanon6", TEST_BASE_ADDR + PAGE_SIZE * 6,
		PAGE_SIZE, VM_READ, 
		test_src_data + (PAGE_SIZE * 5), PAGE_SIZE, 0) != NULL);

	vmas_created = true;
}

static int run_test(struct probe_frame *frame) {
	assert(vmas_created);
	
	struct vma *vma, *old_vma;
	struct task *task = (struct task *)frame->rdi;
	if (!task || task->task_pid != 1)
		return 0;

	vma = task_find_vma(task, TEST_BASE_ADDR);
	assert(vma);
	assert(vma->vm_len == PAGE_SIZE);
	assert(vma->vm_src == test_src_data);
	
	old_vma = vma;
	vma = task_find_vma(task, TEST_BASE_ADDR + PAGE_SIZE);
	assert(vma && (old_vma != vma));
	assert(vma->vm_len == PAGE_SIZE);
	assert(vma->vm_src == test_src_data + PAGE_SIZE);
	
	old_vma = vma;
	vma = task_find_vma(task, TEST_BASE_ADDR + (2 * PAGE_SIZE));
	assert(vma && (old_vma != vma));
	assert(vma->vm_len == PAGE_SIZE - 0x10);
	assert(vma->vm_src == test_src_data + (2 * PAGE_SIZE) + 0x10);

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

