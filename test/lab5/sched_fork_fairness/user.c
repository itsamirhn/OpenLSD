#include <lib.h>

#define RUN_NS MSEC_TO_NSEC(1000)
#define CHUNKS_PER_TASK 20
#define CHUNK_JOB 2000

static uint64_t now_ns(void) {
	struct timespec ts;
	assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
	return ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
}

static void chunk(void) { for (volatile int i = 0; i < CHUNK_JOB; i++) {} }

static int spinner(uint64_t end) {
	int chunks = 0;
	while (now_ns() < end) {
		chunk();
		chunks++;
	}
	return chunks;
}

static int fork_chain(uint64_t end) {
	int chunks = 0;
	while (now_ns() < end) {
		for (int i = 0; i < CHUNKS_PER_TASK; i++) {
			chunk();
			chunks++;
		}

		pid_t child = fork();
		if (child > 0) {
			// Parent wait for child's total
			int total;
			assert(wait(&total) == child);
			return total;
		}
		// Child
	}
	return chunks;
}

int main(void) {
	uint64_t end = now_ns() + RUN_NS;
	int spinner_chunks, chain_chunks;

	pid_t spinner_pid = fork();
	if (spinner_pid == 0) return spinner(end);

	pid_t chain_pid = fork();
	if (chain_pid == 0) return fork_chain(end);

	assert(waitpid(spinner_pid, &spinner_chunks, 0) == spinner_pid);
	assert(waitpid(chain_pid, &chain_chunks, 0) == chain_pid);
	printf("[TEST] spinner %d chunks, fork chain %d chunks\n", spinner_chunks, chain_chunks);

	// Both get the same CPU time, and the chain spends part of it on forking
	assert(spinner_chunks >= chain_chunks);
	printf("[TEST] forking does not starve other tasks\n");

	return 0;
}
