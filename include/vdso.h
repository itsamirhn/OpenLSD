#pragma once
#include <task.h>

#include <x86-64/memory.h>
#include <x86-64/paging.h>

#define VVAR_BASE (USTACK_TOP - 2 * PAGE_SIZE)

struct vdso_data {
	pid_t pid; // The PID of the task this page belongs to.
};
