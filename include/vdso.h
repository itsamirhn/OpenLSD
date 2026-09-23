#pragma once
#include <task.h>

#include <x86-64/memory.h>
#include <x86-64/paging.h>

#define VVAR_BASE (USTACK_TOP - 2 * PAGE_SIZE)
#define VDSO_MAX_PAGES 6
#define VDSO_BASE (VVAR_BASE - VDSO_MAX_PAGES * PAGE_SIZE)

struct vdso_data {
	pid_t pid; // The PID of the task this page belongs to.
	uint64_t tsc_base;
	uint64_t tsc_khz;
	int64_t  epoch_base;
};
