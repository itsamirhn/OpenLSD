#include <atomic.h>
#include <error.h>
#include <lib.h>
#include <cpu.h>

#include <x86-64/asm.h>

#include <kernel/acpi.h>
#include <kernel/sched.h>
#include <kernel/sched/hotplug.h>

#if defined(BONUS_CORE_HOTPLUGGING) || defined(BONUS_LAB6)

#ifdef USE_BIG_KERNEL_LOCK
extern struct spinlock kernel_lock;
#endif

// Idle auto power-off time
#define CORE_IDLE_OFF_MS 10

extern struct rb_tree runq;

// Cores turned off by sys_core_disable(); the load balancer leaves them off
static uint64_t core_disabled;

bool core_is_disabled(struct cpuinfo *cpu) { return core_disabled & (1ULL << (cpu - cpus)); }

uint64_t core_allowed_mask(void) { return CPUS_MASK & ~core_disabled; }

 // If affinity cores were turned off manually, then the task can run on all enabled cores
 // until at least one core is turned on, and thus the task can start running on those corresponding
 // cores with its affinitiy
uint64_t core_task_affinity(struct task *task){
	if(task->task_type == TASK_TYPE_KERNEL) return task->task_affinity; // kernel tasks should stay on the same core
	uint64_t allowed = core_allowed_mask();
	uint64_t mask = task->task_affinity & allowed;

	return mask ? mask : allowed;
}

// Only a parked core is woken, it leaves its hlt loop once it sees CPU_STARTED
static void core_power_on(struct cpuinfo *cpu){
	if (atomic_cmpxchg(&cpu->cpu_status, CPU_HALTED, CPU_STARTED))
		lapic_ipi_cpu(cpu->cpu_id, IRQ_WAKEUP);
}

// Move the caller to another core if it may no longer run on this one
static void core_move_caller(void){
	if (!(core_task_affinity(cur_task) & (1ULL << lapic_cpunum()))) {
		cur_task->task_frame.rax = 0;
		sched_yield();
	}
}

int sys_core_enable(int num){
	if (num < 0 || (size_t)num >= ncpus) return -EINVAL;

	// Atomically clear this core's bit in core_disabled, so the load balancer may use it again.
	__atomic_and_fetch(&core_disabled, ~(1ULL << num), __ATOMIC_SEQ_CST);
	core_power_on(cpus + num);
	core_move_caller();
	return 0;
}

int sys_core_disable(int num){
	if (num < 0 || (size_t)num >= ncpus) return -EINVAL;

	// The boot CPU receives the legacy interrupts and halts the kernel.
	if (cpus + num == boot_cpu) return -EPERM;

	// Atomically set this core's bit in core_disabled, so the load balancer leaves it off.
	// The core powers itself off on its next schedule, the IPI makes that happen now.
	__atomic_or_fetch(&core_disabled, 1ULL << num, __ATOMIC_SEQ_CST);
	lapic_ipi_cpu(cpus[num].cpu_id, IRQ_WAKEUP);
	core_move_caller();
	return 0;
}

// Check if the core needs to be put to sleep due to idleling or being disabled
// Tasks bound to this core wake it up again through core_auto_wake()
bool core_should_power_off(uint64_t idle_start){
	struct cpuinfo *cpu = this_cpu;
	if (core_is_disabled(cpu)) return true;
	if (cpu == boot_cpu || cpu->runq.root)return false;
	return read_tsc() - idle_start >= CORE_IDLE_OFF_MS * time_tsc_khz();
}

 // Tasks added to the global run queue so power on a parked core for each of them.
 // A core that is not needed after all powers off again once it idles.
void core_auto_wake(void){
	uint64_t parked = 0, mask;
	struct task *task;

	for (size_t i = 0; i < ncpus; ++i)
		if (cpus[i].cpu_status == CPU_HALTED && !core_is_disabled(cpus + i))
			parked |= 1ULL << i;

	for (int i = 0; i < runq.size && parked; ++i) {
		task = container_of(rb_index_element(&runq, i), struct task, task_sched_rb);
		if (task->task_type == TASK_TYPE_KERNEL) continue; // should not wake up a core for a kernel task
		if ((mask = core_task_affinity(task) & parked)) {
			parked &= ~(mask & -mask);
			core_power_on(cpus + __builtin_ctzll(mask)); // lowest parked core the task may run on
		}
	}
}

void core_park(void){
	struct cpuinfo *cpu = this_cpu;

	lapic_timer_stop();
	xchg(&cpu->cpu_status, CPU_HALTED);
	cprintf("SMP: CPU %d powered off\n", cpu->cpu_id);
	big_spin_unlock(&kernel_lock);

	// sti only takes effect after the next instruction, so a wake-up IPI
	// arriving after the check still wakes us from hlt.
	while (cpu->cpu_status == CPU_HALTED) asm volatile("sti; hlt; cli" ::: "memory");

	big_spin_lock(&kernel_lock);
	lapic_timer_start();
	cprintf("SMP: CPU %d powered on\n", cpu->cpu_id);

	// Nothing on the current stack is used any more, schedule from the top of
	// the per-CPU kernel stack, so park/wake cycles do not pile up stack frames.
	asm volatile(
		"movq %0, %%rsp\n"
		"call sched_yield\n"
		:: "r" (cpu->cpu_tss.rsp[0]) : "memory");
	__builtin_unreachable();
}

#endif
