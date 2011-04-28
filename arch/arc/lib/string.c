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

#ifdef __HAVE_ARCH_MEMSET

#undef memset

#define OPTIMIZED

//#define STOCK


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

#ifdef OPTIMIZED

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

#endif

#ifdef STOCK
    d_char = dest;
    while(count--)
        *d_char++ = c;
#endif

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



#if 0

void * memcpy ( void * to, const void * from, size_t count)
{

    unsigned int counter;   // words left.
    unsigned int *d;        // dest as int
    unsigned int *s;        // source as int
    unsigned char *s_char;  // source as a char
    unsigned char *d_char;  // dest as a char
    unsigned int remainder; // what's left.
    unsigned int word1,word2,word3,word4;


#ifdef OPTIMIZED

    if(count > 256)
    {
    if( (((unsigned int) from) & 0x03) || (((unsigned int) to) & 0x03))
    {           // unaligned - slow
        d_char = (unsigned char *) to;
        s_char = (unsigned char *) from;

        while(count--)
            *d_char++ = *s_char++;
    }
    else
    {   // it's aligned
        counter = count / 4;    // whole words to write.
        remainder = count % 4;

        d = (unsigned int *) to;
        s = (unsigned int *) from;


// Are there more than 4 words to copy ?  If so, do back to back reads/writes

        while(counter >= 4)
        {
            word1= *s++;
            word2= *s++;
            word3= *s++;
            word4= *s++;

            *d++ = word1;
            *d++ = word2;
            *d++ = word3;
            *d++ = word4;
            counter -=4;
        }

        while(counter--)  // do any remaining stragglers
        {
            *d++ = *s++;
        }

        d_char = (unsigned char *) d;
        s_char = (unsigned char *) s;


// Do remaining bytes

        while(remainder--)
        {
            *d_char++ = *s_char++;
        }


    }

    }
    else


    {

    d_char = to;
    s_char = from;
    while(count--)
        *d_char++ = *s_char++;

    }

#endif

#ifdef STOCK
    d_char = to;
    s_char = from;
    while(count--)
        *d_char++ = *s_char++;

#endif


    return(to);

}

#endif



EXPORT_SYMBOL(memcpy);

#endif
