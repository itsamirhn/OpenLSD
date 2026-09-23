#include <assert.h>
#include <stdio.h>
#include <types.h>

#include <syscall.h>
#include <kernel/sched.h>
#include <kernel/test/test.h>

static int getpid_traps = 0, gettimeofday_traps = 0;

static void syscall_handle(struct probe_frame *frame) {
	if (frame->rdi == SYS_getpid) getpid_traps++;
	if (frame->rdi == SYS_gettimeofday) gettimeofday_traps++;
}

static int run_test(struct probe_frame *frame) {
	cprintf("[VDSO] SYS_getpid reached the kernel %d times, expected 0\n", getpid_traps);
	assert(getpid_traps == 0);

	cprintf("[VDSO] SYS_gettimeofday reached the kernel %d times, expected 0\n", gettimeofday_traps);
	assert(gettimeofday_traps == 0);

	return __checksum__;
}

extern int64_t syscall(uint64_t syscallno, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
extern void task_destroy(struct task *task);

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = task_destroy,
	.should_continue = true,
	.checksum = __checksum__,
	.probe_count = 1,
	.probes = {
		{
			.target = syscall,
			.callback = syscall_handle,
		},
	},
};
