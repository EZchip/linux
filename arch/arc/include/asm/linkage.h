/*Sameer: */
#ifndef __ASM_LINKAGE_H
#define __ASM_LINKAGE_H

#define SYMBOL_NAME_STR(X) #X
#define SYMBOL_NAME(X) X

#define SYMBOL_NAME_LABEL(X) X:

#ifdef __ASSEMBLY__
; the ENTRY macro in linux/linkage.h does not work for us ';' is treated
; as a comment and I could not find a way to put a new line in a #define

.macro ARC_ENTRY name
  .globl SYMBOL_NAME(\name)
  .align 4
  SYMBOL_NAME_LABEL(\name)
.endm
#endif

#endif
