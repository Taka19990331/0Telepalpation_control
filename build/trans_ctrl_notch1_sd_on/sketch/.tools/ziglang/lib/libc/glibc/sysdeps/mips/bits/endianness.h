#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\glibc\\sysdeps\\mips\\bits\\endianness.h"
#ifndef _BITS_ENDIANNESS_H
#define _BITS_ENDIANNESS_H 1

#ifndef _BITS_ENDIAN_H
# error "Never use <bits/endianness.h> directly; include <endian.h> instead."
#endif

/* MIPS has selectable endianness.  */
#ifdef __MIPSEB
# define __BYTE_ORDER __BIG_ENDIAN
#endif
#ifdef __MIPSEL
# define __BYTE_ORDER __LITTLE_ENDIAN
#endif

#endif /* bits/endianness.h */
