#include <assert.h>
#include <types.h>
#include <elf.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>
#include <kernel/sched/task.h>

extern void halt_kernel();
extern uint8_t *find_user_binary();
#define MAIN_RIP 0x800052 // Start of main() in user.c, a pain to actually find this

static int run_test() {

	void (*user_rip)(void) = (void (*)(void))MAIN_RIP;
	
	cprintf("[TEST] Executing user-space code at 0x%lx, without SMEP we access the userspace stack after main returns.\n", (unsigned long)user_rip);
	user_rip();


	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = task_pop_frame,
	.should_continue = false,
	.checksum = __checksum__,
};
