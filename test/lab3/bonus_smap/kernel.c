#include <assert.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>
#include <kernel/sched/task.h>


extern void halt_kernel();


static int run_test() {
	char *user_buf_stack = (char *) USTACK_TOP - 0x1000;

	cprintf("[TEST] Accessing user-space memory, this should page fault under SMAP\n");
	char test = user_buf_stack[0];


	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = task_pop_frame,
	.should_continue = false,
	.checksum = __checksum__,
};
