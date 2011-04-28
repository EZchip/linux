/*****************************************************************
 * STRING LIBRARY
 *
 * ARC International
 * Simon Spooner
 * Oct 2nd 2008
******************************************************************/

#include <linux/string.h>
#include <linux/module.h>
#include <asm/uaccess.h>


unsigned long
slowpath_copy_from_user(void *to, const void *from, unsigned long n)
{
    long res;
    char val;
    const void *tfrom = from;

    __asm__ __volatile__ (
        "   mov     %0,%3               \n"
        "   mov.f   lp_count, %3        \n"
        "   lp 2f                       \n"
        "1: ldb.ab  %4, [%2, 1]         \n"
        "   stb.ab  %4, [%1, 1]         \n"
        "   sub     %0,%0,1             \n"
        "2: nop                         \n"
        "   .section .fixup, \"ax\"     \n"
        "   .align 4                    \n"
        "3: j   2b                      \n"
        "   .previous                   \n"
        "   .section __ex_table, \"a\"  \n"
        "   .align 4                    \n"
        "   .word   1b, 3b              \n"
        "   .previous                   \n"

        :"=r"(res), "=r"(to), "=r"(tfrom), "=r"(n), "=r"(val)
        :"3"(n), "1"(to), "2"(tfrom)
        :"lp_count"
    );

    return res;
}

EXPORT_SYMBOL(slowpath_copy_from_user);

unsigned long
slowpath_copy_to_user(void *to, const void *from, unsigned long n)
{
    long res;
    char val;
    const void *tfrom = from;

    __asm__ __volatile__ (
        "   mov %0,%3                   \n"
        "   mov.f   lp_count, %3        \n"
        "   lp  3f                      \n"
        "   ldb.ab  %4, [%2, 1]         \n"
        "1: stb.ab  %4, [%1, 1]         \n"
        "   sub %0, %0, 1               \n"
        "3: nop                         \n"
        "   .section .fixup, \"ax\"     \n"
        "   .align 4                    \n"
        "4: j   3b                      \n"
        "   .previous                   \n"
        "   .section __ex_table, \"a\"  \n"
        "   .align 4                    \n"
        "   .word   1b, 4b              \n"
        "   .previous                   \n"

        :"=r"(res), "=r"(to), "=r"(tfrom), "=r"(n), "=r"(val)
        :"3"(n), "1"(to), "2"(tfrom)
        : "lp_count"
    );

    return res;
}

EXPORT_SYMBOL(slowpath_copy_to_user);
#ifdef __HAVE_ARCH_MEMSET

#undef memset

// somewhat optized version of memset.
// Note : the code got larger in order to get faster.
// The original can be accessed by unsetting OPTIMIZED and setting STOCK

void * memset (void * dest, int c, size_t count)
{

    unsigned int counter;   // words left.
    unsigned int *d;        // dest
    unsigned char *d_char;  // dest as a char
    unsigned int remainder; // what's left.
    unsigned int word;      // word to write.

// See if there are enough words to copy to make the extra code
// introduced on the optimized version worth while.
// picking an arbitary number of bytes just to start with


    if((((unsigned int) dest) & 0x03) || (count < 256))
    {           // unaligned - slow
        d_char = dest;
        while(count--)
            *d_char++ = c;
    }
    else
    {   // it's aligned
        counter = count / 4;    // whole words to write.
        remainder = count % 4;

        d = (unsigned int *) dest;

        /* For word set, need to stitch four "FILL" chars to make
         *   a word. However if FILL word is zero dont need to do it.
         *   This saves 7 instructions
         */
        if (likely( !c ))
            word = 0;
        else
            word = (c << 24) | ( c << 16) | (c <<8) | c;

        /* Meat of the stuff. Do WORD sets */

        __asm__ __volatile__ (
        "   mov.f   lp_count, %2\n"     // setup ZOL counter Reg
        "   lpnz    1f\n"               // Enter the Loop
        "   st.ab   %3, [%1,4]\n"       // memset + ptr increment
        "1:\n"                          // End of loop
        "   mov  %0, %1"                // setup final ptr into d_char
        :"=r"(d_char)
        :"r" (d), "r" (counter), "r" (word)
        :"lp_count");

        /* Any BYTES left are done here.
         * Note: At the end of loop above d_char was setup
         */
        while(remainder--)
        {
            *d_char++ = c;
        }
    }

    return(dest);
}

EXPORT_SYMBOL(memset);

#endif  // # __HAVE_ARCH_MEMSET

#ifdef __HAVE_ARCH_MEMCPY

#undef memcpy


// After much experimentation if seems it's better to use
// the already existing optimized version of copy_from_user
// to do a quick memory copy

void * memcpy (void * to, const void * from, size_t count)
{

    __generic_copy_from_user(to, from, count);
    return(to);
}
EXPORT_SYMBOL(memcpy);

#endif
