#include <lib.h>
// ran with 4 cpus
int main(int argc, char **argv) {
	cpu_set_t mask;
	int i;

	assert(core_disable(0) == -EPERM); // Boot cpu should fail
	assert(core_disable(4) == -EINVAL); // 5 cpu should not exist in this test
	assert(core_enable(-1) == -EINVAL); // Negative value should fail

	assert(core_disable(2) == 0);
	for (i = 0; i < 1000; ++i) {
		assert(getcpuid() != 2); // Check that we do not run on second after being turned off
		sched_yield();
	}
	printf("[PID %5u] Never ran on disabled CPU 2\n", getpid());

	mask = 1 << 2;
	assert(sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -EINVAL); // cannot set affinity on disabled core

	assert(core_enable(2) == 0);
	assert(sched_setaffinity(0, sizeof(cpu_set_t), &mask) == 0);
	for (i = 0; i < 100; ++i) {
		assert(getcpuid() == 2); // Should run on core with affinity set and turned on
		sched_yield();
	}
	printf("[PID %5u] Ran pinned on re-enabled CPU 2\n", getpid());

	// Disable the core we are pinned to: the task should move, but keep its affinity.
	assert(core_disable(2) == 0);
	assert(getcpuid() != 2);
	assert(sched_getaffinity(0, sizeof(cpu_set_t), &mask) == 0);
	assert(mask == 1 << 2);
	printf("[PID %5u] Moved off disabled CPU 2\n", getpid());

	// Enable it again, the task should be assigned to it again.
	assert(core_enable(2) == 0);
	for (i = 0; i < 100; ++i) {
		assert(getcpuid() == 2);
		sched_yield();
	}
	printf("[PID %5u] Ran pinned on re-enabled CPU 2 again\n", getpid());

	return 0;
}
