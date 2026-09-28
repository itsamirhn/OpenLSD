#include <lib.h>

int main(int argc, char **argv) {
	pid_t pid = getpid();
	int i;

	for (i = 0; i < 1000; i++, pid = getpid()) assert(pid == 1);
	printf("[VDSO] getpid() returned %d without a syscall\n", pid);

	struct timeval tv;
	for (i = 0; i < 1000; i++) gettimeofday(&tv);
	printf("[VDSO] gettimeofday() returned %ld.%06ld without a syscall\n", (long)tv.tv_sec, (long)tv.tv_usec);

	print_vmas();

	return 0;
}
