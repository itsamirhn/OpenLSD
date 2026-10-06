#include <lib.h>

int main(int argc, char **argv)
{
	pid_t pid;

	pid = getpid();

	printf("[PID %5u] Running on CPU %d\n", pid, getcpuid());
	for (int i = 0; i < 10; i++) {
		printf("[PID %5u] Hello from user space!\n", pid);
		sched_yield();
	}

	return 0;
}
