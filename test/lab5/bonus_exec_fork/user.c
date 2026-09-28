#include <lib.h>

int main(void)
{

	int pid = fork();
	if (pid == 0) {
	
		printf("[TEST] Executing embedded hello binary\n");
		int ret = exec("hello");
	}else{
		return wait(NULL);
	}

	return 1;
}
