#include <lib.h>

int main(void) {
	pid_t child = fork();
	if (child == 0) while(true) {}

	int ret = kill(child);
	printf("[TEST] kill returned %d\n", ret);
	assert(ret == 0);

	assert(wait(NULL) == child);
	printf("[TEST] killed child reaped\n");

	return 0;
}
