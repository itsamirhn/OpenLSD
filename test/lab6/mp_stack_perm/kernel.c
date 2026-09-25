#include <assert.h>

#include <cpu.h>
#include <kernel/mem.h>
#include <kernel/test/test.h>

static int check_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	(void)walker;

	if (!(*entry & PAGE_PRESENT)) {
		panic("%p is not mapped!\n", base);
	}
	if (!(*entry & PAGE_WRITE)) {
		panic("%p is not writable!\n", base);
	}
	if (!(*entry & PAGE_NO_EXEC)) {
		panic("%p is executable!\n", base);
	}
	if (*entry & PAGE_USER) {
		panic("%p is mapped as user page!\n", base);
	}

	return 0;
}

static int check_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	(void)walker;

	if (!(*entry & PAGE_HUGE)) {
		return 0;
	}
	
	if (KSTACK_SIZE < HPAGE_SIZE) {
		panic("page size is more than KSTACK_SIZE!\n");
	}

	if (!(*entry & PAGE_PRESENT)) {
		panic("%p is not mapped!\n", base);
	}
	if (!(*entry & PAGE_WRITE)) {
		panic("%p is not writable!\n", base);
	}
	if (!(*entry & PAGE_NO_EXEC)) {
		panic("%p is executable!\n", base);
	}
	if (*entry & PAGE_USER) {
		panic("%p is mapped as user page!\n", base);
	}

	return 0;
}


static int run_test() {
	struct page_walker walker = {
		.pte_callback = check_pte,
		.pde_callback = check_pde,
	};

	uintptr_t kstacktop;
	size_t i;
	for (i = 0; i < ncpus; ++i) {
		kstacktop = cpus[i].cpu_tss.rsp[0];
		walk_page_range(kernel_pml4, (void *)(kstacktop - KSTACK_SIZE), (void *)kstacktop, &walker);
	}

	return __checksum__;
}


extern void halt_kernel();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
