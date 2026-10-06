#include <lib.h>

int main(int argc, char **argv) {
	cpu_set_t mask = 1 << 2;
	int i;

	assert(sched_setaffinity(0, sizeof mask, &mask) == 0);

	for (i = 0; i < 1000; ++i) {
		assert(getcpuid() == 2);
		sched_yield();
	}

	mask = 0;
	assert(sched_getaffinity(0, sizeof mask, &mask) == 0);
	assert(mask == 1 << 2);

	printf("[PID %5u] Pinned to CPU 2\n", getpid());

	return 0;
}
