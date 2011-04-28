/******************************************************************************
 * Copyright ARC International (www.arc.com) 2009-2010
 *
 * Vineetg: July 2009
 *  -Switched from generic Atomic Dec based Mutex to Exchange based mutex
 *   because ARC700 has a atomic reg <---> mem exchange instruction
 *   Atomic dec requires disabling IRQs for ld/sub/st sequence
 *
 *****************************************************************************/
//#include <asm-generic/mutex-dec.h>
#include <asm-generic/mutex-xchg.h>
