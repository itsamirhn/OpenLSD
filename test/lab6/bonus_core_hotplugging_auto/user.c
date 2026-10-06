#include <lib.h>

#define NCHILDREN 3

static void spin(long ms)
{
	struct timeval start, now;

	gettimeofday(&start);
	do {
		gettimeofday(&now);
	} while ((now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000 < ms);
}

int main(int argc, char **argv) {
	unsigned cpus = 0;
	int i, status;
	pid_t child;

	// Only one task runs, so the other cores get disabled 
	spin(200);

	// More tasks than online cores: idle cores enabled
	for (i = 0; i < NCHILDREN; ++i) {
		child = fork();
		assert(child >= 0);
		if (child == 0) {
			spin(500);
			exit(getcpuid());
		}
	}

	for (i = 0; i < NCHILDREN; ++i) {
		assert(waitpid(-1, &status, 0) > 0);
		cpus |= 1 << status;
	}

	assert(cpus & (cpus - 1));
	printf("[PID %5u] Children ran on more than one core\n", getpid());

	return 0;
}
