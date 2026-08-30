#include "kernel/symbols.h"

#include <string.h>
#include <types.h>
#include <elf.h>
#include <types.h>
#include <kernel/mem/buddy.h>

// These need to be weak, they are only valid
// symbols in the second linking pass. Null otherwise
extern char kstrtab_start[]           __attribute__((weak));
extern char kstrtab_end[]             __attribute__((weak));
extern struct elf_sym ksymtab_start[] __attribute__((weak));
extern struct elf_sym ksymtab_end[]   __attribute__((weak));

int load_symbol(char *name, void **symbol, uint8_t type)
{
	assert(name == NULL || *symbol == NULL);

	if ((*symbol = find_symbol(name, type)) == NULL)
		panic("Symbol %s could not be found\n", name);

	return 0;
}

void *find_symbol(char *name, int8_t type)
{
	assert(ksymtab_start && ksymtab_end);
	assert(kstrtab_start && kstrtab_end);

	struct elf_sym *symbol = ksymtab_start;
	while(symbol < ksymtab_end) {
		char *symbol_name = kstrtab_start + symbol->s_name;
		assert(symbol_name < kstrtab_end);

		if(strcmp(name, symbol_name) == 0
			&& (type == ELF_SYM_TYPE_ANY || ELF_SYM_TYPE(symbol->s_info) == type)
		) {
			return (void *) symbol->s_value;
		}

		symbol++;
	}

	return NULL;
}
