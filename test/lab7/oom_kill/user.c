
#include <lib.h>

#define HOG_SIZE  (128 * 1024 * 1024)
#define GROW_SIZE (128 * 1024 * 1024)

char hog[HOG_SIZE];
char grow[GROW_SIZE];

int main(int argc, char **argv)
{
	size_t i;

	memset(hog, 0xBE, sizeof hog);

	if (fork() != 0) {
		wait(NULL);
		printf("The parent survived!\n");
		return 0;
	}

	// Unmap hog in child and grow in size in order to get the parent OOM killed
	munmap(hog, sizeof hog);

	for (i = 0; i < GROW_SIZE; i += PAGE_SIZE) {
		grow[i] = 0xAB;
	}

	printf("OOM kill test successful.\n");

	return 0;
}
