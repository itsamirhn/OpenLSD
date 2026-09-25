#include <assert.h>

#include <cpu.h>
#include <kernel/mem.h>
#include <kernel/test/test.h>



static int run_test() {

	physaddr_t *entry;
	size_t i;
	uintptr_t stack_tops[ncpus];

	stack_tops[0] = cpus[0].cpu_tss.rsp[0];
	for (i = 1; i < ncpus; ++i) {
		stack_tops[i] = cpus[i].cpu_tss.rsp[0];
		// Check that there is a guard page (an unmapped page) at the stack top
		page_lookup(kernel_pml4, (void *)(stack_tops[i]), &entry);
		if (entry && (*entry & PAGE_PRESENT)) {
			panic("Guard page not found below kernel stack for CPU %d\n", i);
		}
		// Check that each CPU has a unique kernel stack
		for (size_t j = 0; j < i; ++j) {
			if (stack_tops[i] == stack_tops[j]) {
				assert(stack_tops[i] != stack_tops[j]);
			}
		}
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
