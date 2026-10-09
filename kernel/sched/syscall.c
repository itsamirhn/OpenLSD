
#include <error.h>
#include <string.h>
#include <assert.h>
#include <lib.h>
#include <cpu.h>

#include <x86-64/asm.h>
#include <x86-64/gdt.h>

#include <kernel/acpi.h>
#include <kernel/console.h>
#include <kernel/mem.h>
#include <kernel/sched.h>
#include <kernel/sched/hotplug.h>
#include <kernel/vma/syscall.h>
#include <kernel/vma.h>
#include <kernel/symbols.h>

#ifdef USE_BIG_KERNEL_LOCK
extern struct spinlock kernel_lock;
#endif

extern void syscall64(void);

void syscall_init(void)
{
#ifdef BONUS_SYSCALL
	union star_reg star = { .reg = 0 };

	star.kern_sel = GDT_KCODE;
	star.user_sel = GDT_KDATA;
	write_msr(MSR_STAR, star.reg);
	write_msr(MSR_LSTAR, (uintptr_t)syscall64);
	write_msr(MSR_SFMASK, FLAGS_IF | FLAGS_DF | FLAGS_TF);
	write_msr(MSR_GS_BASE, (uintptr_t)this_cpu);
	write_msr(MSR_EFER, read_msr(MSR_EFER) | MSR_EFER_SCE);
#endif
}

void syscall_init_mp(void)
{
	return syscall_init();
}

static inline void protected_copy(void *dst, const void *src, size_t len)
{
	#ifdef BONUS_SMEP_SMAP
	stac();
	#endif
	memcpy(dst, src, len);
	#ifdef BONUS_SMEP_SMAP
	clac();
	#endif
}

/*
 * Print a string to the system console.
 * The string is exactly 'len' characters long.
 * Destroys the environment on memory errors.
 */
static void sys_cputs(const char *s, size_t len)
{
	/* Check that the user has permission to read memory [s, s+len).
	 * Destroy the environment if not. */
	assert_user_mem(cur_task, (void *)s, len, PROT_READ);
	#ifdef BONUS_SMEP_SMAP
		stac(); // seperate buffer seems a bit too much right now and it should be safe-ish
	#endif

	sys_madvise((void *)s, len, MADV_WILLNEED);

	/* Print the string supplied by the user. */
	cprintf("%.*s", len, s);

	#ifdef BONUS_SMEP_SMAP
		clac();
	#endif
}

/*
 * Read a character from the system console without blocking.
 * Returns the character, or 0 if there is no input waiting.
 */
static int sys_cgetc(void)
{
	return cons_getc();
}

/* Returns the PID of the current task. */
static pid_t sys_getpid(void)
{
	return cur_task->task_pid;
}

static int sys_gettimeofday(struct timeval *tv) {
	if (!tv) return -EINVAL;

	assert_user_mem(cur_task, tv, sizeof *tv, PROT_WRITE);

	struct timespec ts;
	time_now(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000; // Nano to micro

	return 0;
}

static int sys_clock_gettime(clockid_t clock, struct timespec *ts) {
	if (!ts) return -EINVAL;

	assert_user_mem(cur_task, ts, sizeof *ts, PROT_WRITE);

	switch (clock) {
		case CLOCK_REALTIME:
			time_now(ts);
			return 0;
		case CLOCK_MONOTONIC:
			time_monotonic(ts);
			return 0;
		default:
			return -EINVAL;
	}
}

static int sys_clock_nanosleep(clockid_t clock, int flags, const struct timespec *req) {
	if (clock != CLOCK_REALTIME && clock != CLOCK_MONOTONIC) return -EINVAL;
	assert_user_mem(cur_task, (void *)req, sizeof *req, PROT_READ);
	if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= NSEC_PER_SEC) return -EINVAL;
	uint64_t ns = req->tv_sec * NSEC_PER_SEC + req->tv_nsec;
	if (flags & TIMER_ABSTIME) {
		struct timespec now;
		if (clock == CLOCK_REALTIME) time_now(&now);
		else time_monotonic(&now);
		if (now.tv_sec * NSEC_PER_SEC + now.tv_nsec > ns) return 0;
		ns -= now.tv_sec * NSEC_PER_SEC + now.tv_nsec;
	}
	sched_sleep(ns);
	return 0;
}


static int sys_kill(pid_t pid)
{
	struct task *task;

	task = pid2task(pid, 1);

	if (!task) {
		return -1;
	}

	cprintf("[PID %5u] Exiting gracefully\n", task->task_pid);

	fine_spin_lock(&task->task_lock);
	task->task_killed = true;
	fine_spin_unlock(&task->task_lock);
	while (task != cur_task && task->task_status != TASK_NOT_RUNNABLE && task->task_status != TASK_DYING) {
		if (sched_dequeue(task)) break;
		big_spin_unlock(&kernel_lock);
		asm volatile("pause" ::: "memory");
		big_spin_lock(&kernel_lock);
	}

	task_destroy(task);

	return 0;
}

static int sys_exit(int rcode)
{
	struct task *task = cur_task;

	if (!task) return -1;
	task->task_exit_status = rcode;

	cprintf("[PID %5u] Exiting gracefully with code %d\n", task->task_pid, rcode);

	task_destroy(task);

	return 0;
}

int sys_exec(char *binary_name)
{
	char symbol_name[256];
	char name[128];
	size_t prefix_length;

	int length = strlen(binary_name);

	assert_user_mem(cur_task, (void *)binary_name, length + 1, PROT_READ);
		
	if (length >= (int)sizeof(name) - 1){
		return -EINVAL;
	}

	protected_copy(name, binary_name, length + 1); // just if SMAP is on.
	
	strlcpy(symbol_name, "_binary_obj_user_", sizeof(symbol_name));
	prefix_length = strlen(symbol_name);
	if (prefix_length + length + sizeof("_start") > sizeof(symbol_name)){
		return -EINVAL;
	}
	memcpy(symbol_name + prefix_length, name, length);
	strlcpy(symbol_name + prefix_length + length, "_start", sizeof(symbol_name) - prefix_length - length);

	uint8_t *binary = find_symbol(symbol_name, ELF_SYM_TYPE_ANY);
	if (!binary){
		return -EINVAL;
	}

	return task_exec(binary);
}


static int sys_getcpuid(void)
{
	return lapic_cpunum();
}

static int sys_sched_setaffinity(pid_t pid, unsigned cpusetsize, cpu_set_t *mask) {
	struct task *task = pid2task(pid, 1);
	cpu_set_t set;

	if (!task) return -EINVAL;
	if (cpusetsize < sizeof set) return -EINVAL;

	assert_user_mem(cur_task, mask, sizeof set, PROT_READ);
	protected_copy(&set, mask, sizeof set);

	set &= CPUS_MASK;
	if (!set) return -EINVAL;
#ifdef BONUS_CORE_HOTPLUGGING
	if (!(set & core_allowed_mask())) return -EINVAL; // Cannot set affinity to cores that were manually disabled
#endif

	task->task_affinity = set;

	// Move off this CPU right away if we are no longer allowed on it
	if (cur_task == task && !(set & (1ULL << lapic_cpunum()))) {
		task->task_frame.rax = 0;
		sched_yield();
	}

	return 0;
}

static int sys_sched_getaffinity(pid_t pid, unsigned cpusetsize, cpu_set_t *mask) {
	struct task *task = pid2task(pid, 1);

	if (!task) return -EINVAL;
	if (cpusetsize < sizeof *mask) return -EINVAL;

	assert_user_mem(cur_task, mask, sizeof *mask, PROT_WRITE);
	protected_copy(mask, &task->task_affinity, sizeof *mask);

	return 0;
}

/* Dispatches to the correct kernel function, passing the arguments. */
int64_t syscall(uint64_t syscallno, uint64_t a1, uint64_t a2, uint64_t a3,
        uint64_t a4, uint64_t a5, uint64_t a6)
{
	/*
	 * Call the function corresponding to the 'syscallno' parameter.
	 * Return any appropriate return value.
	 */

	switch (syscallno) {
		case SYS_cputs:
			sys_cputs((const char *)a1, (size_t)a2);
			return 0;
		case SYS_cgetc:
			return sys_cgetc();
		case SYS_getpid:
			return sys_getpid();
		case SYS_gettimeofday:
			return sys_gettimeofday((struct timeval *)a1);
		case SYS_clock_gettime:
			return sys_clock_gettime((clockid_t)a1, (struct timespec *)a2);
		case SYS_kill:
			return sys_kill((pid_t)a1);
		case SYS_exit:
			return sys_exit((int)a1);
		case SYS_mquery:
			return sys_mquery((struct vma_info *)a1, (void *)a2);
		case SYS_mmap:
			return (int64_t)sys_mmap((void *)a1, (size_t)a2, (int)a3, (int)a4, (int)a5, (uintptr_t)a6);
		case SYS_munmap:
			sys_munmap((void *)a1, (size_t)a2);
			return 0;
		case SYS_mprotect:
			return sys_mprotect((void *)a1, (size_t)a2, (int)a3);
		case SYS_madvise:
			return sys_madvise((void *)a1, (size_t)a2, (int)a3);
		case SYS_yield:
			sched_yield();
			return 0;
		case SYS_wait:
			return sys_wait((int *)a1);
		case SYS_waitpid:
			return sys_waitpid((pid_t)a1, (int *)a2, (int)a3);
		case SYS_fork:
			return sys_fork();
		case SYS_clock_nanosleep:
			return sys_clock_nanosleep((clockid_t)a1, (int)a2, (const struct timespec *)a3);
		case SYS_exec:
			return sys_exec((char *)a1);
		case SYS_getcpuid:
			return sys_getcpuid();
		case SYS_sched_setaffinity:
			return sys_sched_setaffinity((pid_t)a1, (unsigned)a2, (cpu_set_t *)a3);
		case SYS_sched_getaffinity:
			return sys_sched_getaffinity((pid_t)a1, (unsigned)a2, (cpu_set_t *)a3);
	#ifdef BONUS_CORE_HOTPLUGGING
		case SYS_core_enable:
			return sys_core_enable((int)a1);
		case SYS_core_disable:
			return sys_core_disable((int)a1);
	#endif
		default:
			return -ENOSYS;
	}
}

void syscall_handler(uint64_t a1, uint64_t a2, uint64_t a3,
    uint64_t a4, uint64_t a5, uint64_t a6, uint64_t syscallno)
{
	struct int_frame *frame;

	big_spin_lock(&kernel_lock);

	/* Syscall from user mode. */
	assert(cur_task);

	/* Avoid using the frame on the stack. */
	frame = &cur_task->task_frame;

	/* Issue the syscall. */
	frame->rax = syscall(syscallno, a1, a2, a3, a4, a5, a6);

	/* Return to the current task, which should be running. */
	task_run(cur_task);
}
