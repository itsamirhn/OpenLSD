#include <error.h>
#include <stdio.h>
#include <lib.h>
#include <cpu.h>

#include <x86-64/asm.h>

#include <kernel/acpi.h>
#include <kernel/sched.h>
#include <kernel/sched/hotplug.h>

#ifdef BONUS_CORE_HOTPLUGGING

#ifdef USE_BIG_KERNEL_LOCK
extern struct spinlock kernel_lock;
#endif

// Idle auto power-off time
#define CORE_IDLE_OFF_MS 10

extern struct rb_tree runq;
extern struct task **tasks;
extern pid_t pid_max;

// Cores not turned off by sys_core_disable()
uint64_t core_allowed_mask(void){
	uint64_t mask = 0;

	for (size_t i = 0; i < ncpus; ++i){
		if (!cpus[i].cpu_off_manual){
			mask |= 1ULL << i;
		}
	}

	return mask;
}

 // If affinity cores were turned off manually, then the task can run on all enabled cores
 // until at least one core is turned on, and thus the task can start running on those corresponding
 // cores with its affinitiy
uint64_t core_task_affinity(struct task *task){
	if(task->task_type == TASK_TYPE_KERNEL) return task->task_affinity; // kernel tasks should stay on the same core
	uint64_t allowed = core_allowed_mask();
	uint64_t mask = task->task_affinity & allowed;

	return mask ? mask : allowed;
}

 // Check or log if core is bound to any task
static bool core_scan_affinity(int num, const char *what){
	uint64_t bit = 1ULL << num, all = CPUS_MASK;
	uint64_t others = core_allowed_mask() & ~bit;
	struct task *task;
	bool ret = false;

	for (pid_t pid = 1; pid < pid_max; ++pid) {
		task = tasks[pid];

		if (!task || task->task_status == TASK_DYING || !(task->task_affinity & bit) || task->task_type == TASK_TYPE_KERNEL) {
			continue;
		}

		if (!what) {
			if ((task->task_affinity & all) != all){
				return true;
			}
		} else if (!(task->task_affinity & others)) {
			cprintf("[PID %5u] Affinity %s\n", task->task_pid, what);
			ret = true;
		}
	}

	return ret;
}

static void core_power_on(struct cpuinfo *cpu){
	cpu->cpu_off = false;
	lapic_ipi_cpu(cpu->cpu_id, IRQ_WAKEUP);
}


 // Wait till core reaches desired state
static void core_wait(struct cpuinfo *cpu, unsigned status){
	while (cpu->cpu_status != status) {
		big_spin_unlock(&kernel_lock);
		asm volatile("pause" ::: "memory");
		big_spin_lock(&kernel_lock);

		// Got turned off while waiting, so just yield 
		if (this_cpu->cpu_off) {
			cur_task->task_frame.rax = 0;
			sched_yield();
		}
	}
}

int sys_core_enable(int num){
	struct cpuinfo *cpu;

	if (num < 0 || (size_t)num >= ncpus){
		return -EINVAL;
	}

	cpu = cpus + num;
	if (cpu->cpu_off_manual) {
		cpu->cpu_off_manual = false;
		core_scan_affinity(num, "restored, its cores are enabled again");
	}

	if (cpu->cpu_off){
		core_power_on(cpu);
	}

	core_wait(cpu, CPU_STARTED);

	if (!(core_task_affinity(cur_task) & (1ULL << lapic_cpunum()))) {
		cur_task->task_frame.rax = 0;
		sched_yield();
	}
	
	return 0;
}

int sys_core_disable(int num){
	struct cpuinfo *cpu;

	if (num < 0 || (size_t)num >= ncpus){
		return -EINVAL;
	}

	cpu = cpus + num;

	// The boot CPU receives the legacy interrupts and halts the kernel.
	if (cpu == boot_cpu){
		return -EPERM;
	}

	if (!cpu->cpu_off_manual) {
		cpu->cpu_off_manual = true;
		core_scan_affinity(num, "broken, its cores are disabled");
	}
	cpu->cpu_off = true;

	if (cpu == this_cpu) {
		cur_task->task_frame.rax = 0;
		sched_yield();
	}

	lapic_ipi_cpu(cpu->cpu_id, IRQ_WAKEUP);
	core_wait(cpu, CPU_HALTED);
	return 0;
}

// Check if the core needs to be put to sleep due to idleling or it has an pinned task
bool core_should_power_off(uint64_t *idle_start){
	struct cpuinfo *cpu = this_cpu;

	if (cpu->cpu_off){
		return true;
	}

	if (cpu == boot_cpu || cpu->runq.root){
		return false;
	}

	if (read_tsc() - *idle_start < CORE_IDLE_OFF_MS * time_tsc_khz()){
		return false;
	}

	if (core_scan_affinity(cpu - cpus, NULL)) {
		*idle_start = read_tsc();
		return false;
	}

	cpu->cpu_off = true;
	return true;
}

 // Tasks added to the global rb_tree runq, we should wake an idle core for that
void core_auto_wake(void){
	uint64_t idle = 0, parked = 0, mask;
	struct task *task;

	for (size_t i = 0; i < ncpus; ++i) {
		struct cpuinfo *cpu = cpus + i;

		if (cpu->cpu_status == CPU_STARTED && !cpu->cpu_off && cpu->cpu_idle){
			idle |= 1ULL << i;
		}
		if (cpu->cpu_status == CPU_HALTED && cpu->cpu_off && !cpu->cpu_off_manual){
			parked |= 1ULL << i;
		}
	}

	if (!parked){
		return; // every core is active
	}

	for (int i = 0; i < runq.size; ++i) {
		task = container_of(rb_index_element(&runq, i), struct task, task_sched_rb);
		if (task->task_type == TASK_TYPE_KERNEL) {
			continue; // should not wake up a core for a kernel task
		}

		if (core_task_affinity(task) & idle){
			continue;
		}

		mask = core_task_affinity(task) & parked;
		if (mask) {
			core_power_on(cpus + __builtin_ctzll(mask)); // trailing zeros count, get cpu num like that from mask, or at least get one core for the task
			return;
		}
	}
}

void __attribute__((noreturn)) core_park_on_stack(void){
	struct cpuinfo *cpu = this_cpu;

	cpu->cpu_idle = false;
	lapic_timer_stop();
	xchg(&cpu->cpu_status, CPU_HALTED);
	cprintf("SMP: CPU %d powered off\n", cpu->cpu_id);
	big_spin_unlock(&kernel_lock);

	// sti only takes effect after the next instruction, so a wake-up IPI
	// arriving after the check still wakes us from hlt.
	while (cpu->cpu_off){
		asm volatile("sti; hlt; cli" ::: "memory");
	}

	big_spin_lock(&kernel_lock);
	lapic_timer_start();
	xchg(&cpu->cpu_status, CPU_STARTED);
	cprintf("SMP: CPU %d powered on\n", cpu->cpu_id);

	sched_yield();
	panic("sched_yield returned");
}


void core_park(void){
	// We may be running on the stack of a kernel thread, which can resume on
	// another core: switch to the per-CPU kernel stack.
	asm volatile(
		"movq %0, %%rsp\n"
		"call core_park_on_stack\n"
		:: "r" (this_cpu->cpu_tss.rsp[0] - 32) : "memory");
	__builtin_unreachable();
}

#endif
