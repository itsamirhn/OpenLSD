#pragma once

#include <types.h>
#include <task.h>
#include <cpu.h>

#ifdef BONUS_CORE_HOTPLUGGING
int sys_core_enable(int num);
int sys_core_disable(int num);

bool core_is_disabled(struct cpuinfo *cpu);
uint64_t core_allowed_mask(void);
uint64_t core_task_affinity(struct task *task);
bool core_should_power_off(uint64_t idle_start);
void core_auto_wake(void);
void __attribute__((noreturn)) core_park(void);
#endif
