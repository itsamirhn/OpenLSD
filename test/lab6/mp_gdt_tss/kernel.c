#include <assert.h>

#include <cpu.h>
#include <spinlock.h>
#include <kernel/mem.h>
#include <kernel/sched.h>
#include <kernel/test/test.h>


static struct spinlock array_lock;
// global array to store GDT base addresses and TSS indices of each CPU
static struct gdtr cpu_gdt_bases[NCPUS];
static uint16_t cpu_tss_indices[NCPUS];

static void intercept_sched_yield(struct probe_frame *frame) {
	if (cpu_gdt_bases[this_cpu->cpu_id].entries == NULL) {
		spin_lock(&array_lock);
		// Store the GDT base address and TSS index for this CPU
		asm volatile("sgdt %0" : "=m" (cpu_gdt_bases[this_cpu->cpu_id]));
		asm volatile("str %0" : "=m" (cpu_tss_indices[this_cpu->cpu_id]));
		spin_unlock(&array_lock);
	}
}


static int run_test() {

	// first check whether each CPU has its own GDT or a shared one
	bool shared_gdt = false;
	for (size_t i = 1; i < ncpus; ++i) {
		if (cpu_gdt_bases[i].entries == cpu_gdt_bases[0].entries) {
			shared_gdt = true;
			break;
		}
	}
	
	// for shared GDT, check whether the TSS segments are different
	if (shared_gdt) {
		// extract TSS addresses from GDT entries
		struct tss *tss[ncpus];
		for (size_t i = 0; i < ncpus; ++i) {
			struct tss_entry *tss_entry = (struct tss_entry *)(cpu_gdt_bases[i].entries + (cpu_tss_indices[i] >> 3));
			tss[i] = (struct tss *)(
			    ((uint64_t)tss_entry->base_high << 32) |
			    ((uint64_t)tss_entry->entry.base_high << 24) |
			    ((uint64_t)tss_entry->entry.base_middle << 16) |
			    (uint64_t)tss_entry->entry.base_low);
		}
		// now check that all TSS addresses are different
		for (size_t i = 0; i < ncpus; ++i) {
			for (size_t j = i + 1; j < ncpus; ++j) {
				if (tss[i] == tss[j]) {
					panic("CPUs %d and %d have the same TSS address\n", i, j);
				}
			}
		}
	}
	// else each CPU has its own GDT, so the TSS segments are definitely different
	return __checksum__;
}


extern void sched_yield();
extern void halt_kernel();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,

	.probe_count = 1,
	.probes = {
		{
			.target = sched_yield,
			.callback = intercept_sched_yield,
		}
	},
};
