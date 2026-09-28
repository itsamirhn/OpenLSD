#include <lib.h>

int main(void)
{
	printf("[TEST] Executing embedded hello binary\n");
	int ret = exec("hello");

	printf("[TEST] exec returned %d\n", ret);
	return 1;
}
