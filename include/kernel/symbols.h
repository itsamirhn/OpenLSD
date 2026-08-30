#pragma once

#include <types.h>

struct symbol_def {
	char *name;
	void **symbol;
	uint8_t type;
};

int load_symbol(char *name, void **symbol, uint8_t type);
void *find_symbol(char *name, int8_t type);
