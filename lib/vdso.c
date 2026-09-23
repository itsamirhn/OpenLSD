#include <lib.h>
#include <elf.h>
#include <vdso.h>

void *vdso_sym(void *base, const char *name)
{
	struct elf *ehdr = base;
	struct elf_proghdr *phdr;
	struct elf_dyn *dyn = NULL;
	struct elf_sym *symtab = NULL;
	const char *strtab = NULL;
	uint32_t nsyms = 0;
	size_t i;

	if (!base || ehdr->e_magic != ELF_MAGIC) return NULL;

	phdr = (struct elf_proghdr *)((char *)base + ehdr->e_phoff);

	for (i = 0; i < ehdr->e_phnum; i++, phdr++) if (phdr->p_type == ELF_PROG_DYNAMIC) {
		dyn = (struct elf_dyn *)((char *)base + phdr->p_va);
		break;
	}

	if (!dyn) return NULL;

	for (; dyn->d_tag != ELF_DYN_NULL; dyn++)
		switch (dyn->d_tag) {
		case ELF_DYN_SYMTAB:
			symtab = (struct elf_sym *)((char *)base + dyn->d_val);
			break;
		case ELF_DYN_STRTAB:
			strtab = (const char *)base + dyn->d_val;
			break;
		case ELF_DYN_HASH:
			nsyms = ((uint32_t *)((char *)base + dyn->d_val))[1];
			break;
		}

	if (!symtab || !strtab || !nsyms) return NULL;

	for (i = 0; i < nsyms; i++) 
		if (symtab[i].s_name && !strcmp(strtab + symtab[i].s_name, name)) 
		return (char *)base + symtab[i].s_value;

	return NULL;
}
