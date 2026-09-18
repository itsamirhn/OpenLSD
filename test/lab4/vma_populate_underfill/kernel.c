/*
 * Test: VMA populate with vm_len overflow
 *
 * This test verifies that when a VMA has vm_len less than its size, the
 * populate function correctly:
 * 1. Copies source data only up to vm_len bytes
 * 2. Zeros the remaining bytes from vm_len to the end of the VMA
 *
 * This is critical for handling executables where the file size (vm_len)
 * may be smaller than the memory size (VMA size), requiring the BSS
 * section to be zeroed.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <kernel/test/test.h>
#include <kernel/sched.h>
#include <kernel/vma/insert.h>
#include <kernel/vma/populate.h>
#include <kernel/mem/lookup.h>
#include <kernel/mem/buddy.h>
#include <paging.h>

/* Test constants - magic numbers for test data */
#define TEST_VMA_BASE_ADDR      ((void *)0x2000000)  /* Base address for test VMA */
#define TEST_VMA_SIZE           (PAGE_SIZE * 2)      /* VMA spans 2 pages */
#define TEST_VMA_LEN            (PAGE_SIZE - 5)      /* vm_len is less than VMA size */
#define TEST_SOURCE_DATA_BYTE   0xAA                 /* Test pattern byte for source data */
#define TEST_ZERO_BYTE          0x00                 /* Expected zero byte */

static int run_test(struct probe_frame *frame) {
	/* Test source data buffer - filled with TEST_SOURCE_DATA_BYTE */
	void *test_src_data = page2kva(page_alloc(0));
	memset(test_src_data, TEST_SOURCE_DATA_BYTE, PAGE_SIZE);

	/* Get the task from task_destroy parameter (rdi in x86-64 calling convention).
	 * This runs when the task is being destroyed, but before it's freed,
	 * so the task and its VMAs are still valid. */
	struct task *task = (struct task *)frame->rdi;
	assert(task != NULL);

	cprintf("Test: VMA with vm_len less than VMA size\n");

	// Dobule check that our data is aligned
	assert(test_src_data - ROUNDDOWN(test_src_data, PAGE_SIZE) == 0);

	struct vma *vma = add_executable_vma(task, "test_exec", TEST_VMA_BASE_ADDR,
		TEST_VMA_SIZE, VM_READ | VM_WRITE, test_src_data, TEST_VMA_LEN, 0);
	assert(vma != NULL);
	assert(vma->vm_len == TEST_VMA_LEN);
	assert(vma->vm_end - vma->vm_base == TEST_VMA_SIZE);

	/* Populate the VMA - this should copy data up to vm_len and zero the rest */
	int ret = populate_vma_range(task, TEST_VMA_BASE_ADDR, TEST_VMA_SIZE,
		VM_READ | VM_WRITE);
	assert(ret == 0);

	/* Verify all bytes in the VMA:
	 * - Bytes 0 to TEST_VMA_LEN-1 should contain source data
	 * - Bytes TEST_VMA_LEN to TEST_VMA_SIZE-1 should be zeroed
	 */
	for (size_t i = 0; i < TEST_VMA_SIZE; i += PAGE_SIZE) {
		void *page_va = (char *)TEST_VMA_BASE_ADDR + i;
		size_t bytes_to_check;
		size_t j;

		/* Look up the page */
		physaddr_t *entry;
		struct page_info *page = page_lookup(task->task_pml4, page_va, &entry);
		assert(page != NULL);
		assert(*entry & PAGE_PRESENT);

		char *kva = page2kva(page);
		assert(kva != NULL);

		/* Calculate offset within the page */
		uintptr_t page_offset = (uintptr_t)page_va & (PAGE_SIZE - 1);

		/* Determine how many bytes to check in this page */
		bytes_to_check = PAGE_SIZE - page_offset;
		if (i + bytes_to_check > TEST_VMA_SIZE) {
			bytes_to_check = TEST_VMA_SIZE - i;
		}

		/* Verify each byte in this page */
		for (j = 0; j < bytes_to_check; j++) {
			size_t byte_index = i + j;
			unsigned char expected_byte;

			/* Determine expected value based on position relative to vm_len */
			if (byte_index < TEST_VMA_LEN) {
				/* Within vm_len: should contain source data */
				expected_byte = TEST_SOURCE_DATA_BYTE;
			} else {
				/* Beyond vm_len: should be zeroed */
				expected_byte = TEST_ZERO_BYTE;
			}

			/* Verify the byte matches expected value */
			assert((unsigned char)kva[page_offset + j] == expected_byte);
		}
	}

	cprintf("Test passed: Data copied up to vm_len, rest zeroed\n");

	return __checksum__;
}

extern void task_destroy(struct task *task);

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = task_destroy,  /* Run test when task is destroyed (before it's freed) */
	.should_continue = true,
	.checksum = __checksum__,
	.probe_count = 0,
	.probes = {},
};
