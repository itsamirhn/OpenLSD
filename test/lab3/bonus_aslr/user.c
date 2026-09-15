#include <lib.h>
#include <stdio.h>

// It will be zeroed by the linker; without a relocation the %s below should faults on NULL
char *foo = "foo";

int main(int argc, char **argv)
{
	/* We expect 7 hex digits, which only a randomised base can give and not the current implementation */
	printf("[ASLR] %s at %p\n", foo, &main);

	return 0;
}
