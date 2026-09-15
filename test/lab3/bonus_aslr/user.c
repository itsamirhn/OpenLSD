#include <lib.h>
#include <stdio.h>

char *foo = "foo";

int main(int argc, char **argv) {
	printf("[ASLR] %s at %p\n", foo, foo);
	printf("[ASLR] puts at %p\n", &puts);
	panic("die");
}
