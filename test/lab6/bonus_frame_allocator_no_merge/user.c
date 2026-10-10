#include <lib.h>

int main(int argc, char **argv)
{
	cpu_set_t mask = 1 << 1;

	// Keep CPU 1 busy, so core hotplugging does not turn it off
	assert(sched_setaffinity(0, sizeof mask, &mask) == 0);

	// Keep alive
	for (;;)
		sched_yield();

	return 0;
}
