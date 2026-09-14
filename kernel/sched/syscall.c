
#include <error.h>
#include <string.h>
#include <assert.h>
#include <lib.h>

#include <x86-64/asm.h>
#include <x86-64/gdt.h>

#include <kernel/console.h>
#include <kernel/mem.h>
#include <kernel/sched.h>

extern void syscall64(void);

void syscall_init(void)
{
#ifdef BONUS_SYSCALL
	union star_reg star = { .reg = 0 };

	star.kern_sel = GDT_KCODE;
	star.user_sel = GDT_UDATA;
	write_msr(MSR_STAR, star.reg);
	write_msr(MSR_LSTAR, (uintptr_t)syscall64);
	write_msr(MSR_SFMASK, FLAGS_IF | FLAGS_DF | FLAGS_TF);
	write_msr(MSR_KERNEL_GS_BASE, (uintptr_t)this_cpu);
	write_msr(MSR_EFER, read_msr(MSR_EFER) | MSR_EFER_SCE);
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

	/* Print the string supplied by the user. */
	cprintf("%.*s", len, s);
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

static int sys_kill(pid_t pid)
{
	struct task *task;
	task = pid2task(pid, 1);

	if (!task) {
		return -1;
	}


	cprintf("[PID %5u] Exiting gracefully\n", task->task_pid);
	task_destroy(task);

	return 0;
}

static int sys_exit(int rcode)
{
	struct task *task = cur_task;

	cprintf("[PID %5u] Exiting gracefully with code %d\n", task->task_pid, rcode);

	task_destroy(task);

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
		case SYS_kill:
			return sys_kill((pid_t)a1);
		case SYS_exit:
			return sys_exit((int)a1);
		default:
			return -ENOSYS;
	}
}

void syscall_handler(uint64_t a1, uint64_t a2, uint64_t a3,
    uint64_t a4, uint64_t a5, uint64_t a6, uint64_t syscallno)
{
	struct int_frame *frame;

	/* Syscall from user mode. */
	assert(cur_task);

	/* Avoid using the frame on the stack. */
	frame = &cur_task->task_frame;

	/* Issue the syscall. */
	frame->rax = syscall(syscallno, a1, a2, a3, a4, a5, a6);

	/* Return to the current task, which should be running. */
	task_run(cur_task);
}
