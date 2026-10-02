
#include <x86-64/gdt.h>
#include <x86-64/memory.h>

#include <cpu.h>
#include <kernel/acpi.h>
#include <kernel/sched/gdt.h>

#define GDT_ENTRIES_COUNT (5 + 2 * NCPUS)
struct gdt_entry gdt_entries[GDT_ENTRIES_COUNT] = {
	[GDT_KCODE >> 3] = { .flags = GDT_KCODE_FLAGS | GDT_LONG_MODE },
	[GDT_KDATA >> 3] = { .flags = GDT_KDATA_FLAGS },
	[GDT_UCODE >> 3] = { .flags = GDT_UCODE_FLAGS | GDT_LONG_MODE },
	[GDT_UDATA >> 3] = { .flags = GDT_UDATA_FLAGS },
};

struct gdtr gdtr = {
	.limit = sizeof(gdt_entries) - 1,
	.entries = gdt_entries,
};

void gdt_init(void)
{
	/* Set up the kernel stack pointer in the TSS. Add the TSS to the GDT.
	 * Load the GDT and the task selector.
	 */
	uint16_t tss_sel = GDT_TSS0 + lapic_cpunum() * sizeof(struct tss_entry);
	set_tss_entry((struct tss_entry *)(gdt_entries + (tss_sel >> 3)),
	    &this_cpu->cpu_tss);
	load_gdt(&gdtr, GDT_KCODE, GDT_KDATA);
	load_task_sel(tss_sel);
}

void gdt_init_mp(void)
{
	return gdt_init();
}
