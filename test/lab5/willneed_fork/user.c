#include <lib.h>

int value = 1;

int main(void) {
	int status = 0;
	pid_t child;

	value = 2;
	child = fork();

	if (child == 0) {
		madvise((void *)ROUNDDOWN((uintptr_t)&value, 4096), 4096, MADV_WILLNEED);
		assert(value == 2); // child kept its data
		return value;
	}

	waitpid(child, &status, 0);
	assert(status == 2); // child kept its data
	assert(value == 2); // parent was not touched
	return 0;
}
