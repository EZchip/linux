/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *
 *  linux/arch/arc/kernel/process.c
 *
 *  Copyright (C)
 *
 *  Authors : Sameer Dhavale
 */

#include <linux/module.h>
#include <linux/moduleloader.h>
#include <linux/kernel.h>
#include <linux/elf.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/string.h>

#define BITS(word,s,e)          (((word) << (31 - e)) >> (s + (31 -e)))

static inline unsigned long arc_read_me(unsigned char *addr)
{
	unsigned long value = 0;
	value = (*addr & 255) << 16;
	value |= (*(addr + 1) & 255) << 24;
	value |= (*(addr + 2) & 255);
	value |= (*(addr + 3) & 255) << 8;
	return value;
}

static inline void arc_write_me(unsigned short *addr, unsigned long value)
{
	*addr = (value & 0xffff0000) >> 16;
	*(addr+1) = (value & 0xffff) ;
}

void *module_alloc(unsigned long size)
{
	if(size == 0)
		return NULL;

	return vmalloc(size);

}

void module_free(struct module *module, void *region)
{
	vfree(region);
}

/* Sameer: Do we have any arch-specific sections? */
int module_frob_arch_sections(Elf_Ehdr *hdr,
			      Elf_Shdr *sechdrs,
			      char *secstrings,
			      struct module *mod)
{
	return 0;
}

int
apply_relocate(Elf32_Shdr *sechdrs, const char *strtab, unsigned int symindex,
	       unsigned int relindex, struct module *module)
{
	printk(KERN_ERR "module %s: SHT_REL type unsupported\n",
	       module->name);

	return 0;
}

int apply_relocate_add(Elf32_Shdr *sechdrs,
		   const char *strtab,
		   unsigned int symindex,
		   unsigned int relsec,
		   struct module *module)
{
	unsigned int i;
	Elf32_Rela *rel = (void *)sechdrs[relsec].sh_addr;
	Elf32_Sym *sym;
	Elf32_Addr relocation;
	uint32_t *location;
	unsigned int insn;

#ifdef DEBUG_ARC_MODULES
	printk("apply_relocate_add()\n");

	printk("Applying relocate section %u to %u\n", relsec,
	       sechdrs[relsec].sh_info);
#endif

	for (i = 0; i < sechdrs[relsec].sh_size / sizeof(*rel); i++) {
		/* This is where to make the change */
		location = (void *)sechdrs[sechdrs[relsec].sh_info].sh_addr
			+ rel[i].r_offset;
		/* This is the symbol it is referring to.  Note that all
		   undefined symbols have been resolved.  */
		sym = (Elf32_Sym *)sechdrs[symindex].sh_addr
			+ ELF32_R_SYM(rel[i].r_info);
		relocation = sym->st_value + rel[i].r_addend;

		switch (ELF32_R_TYPE(rel[i].r_info)) {
		case R_ARC_32:
#ifdef DEBUG_ARC_MODULES
			printk("R_ARC_32: location=0x%x, relocation=0x%x\n", location, (uint32_t *)relocation);
#endif
			*location = (uint32_t *)relocation;
			break;

		case R_ARC_32_ME:
			insn = arc_read_me((char *)location);
			insn = relocation;
#ifdef DEBUG_ARC_MODULES
			printk("R_ARC_32_ME: location=0x%x, relocation=0x%x, insn=0x%x\n", location, (uint32_t *)relocation, insn);
#endif
			arc_write_me((unsigned short *)location,insn);

			break;
		default:
			printk(KERN_ERR "%s: unknown relocation: %u\n",
			       module->name, ELF32_R_TYPE(rel[i].r_info));
			return -ENOEXEC;
		}

	}
	return 0;
}


int
module_finalize(const Elf32_Ehdr *hdr, const Elf_Shdr *sechdrs,
		struct module *module)
{
	return 0;
}

void
module_arch_cleanup(struct module *mod)
{
}
