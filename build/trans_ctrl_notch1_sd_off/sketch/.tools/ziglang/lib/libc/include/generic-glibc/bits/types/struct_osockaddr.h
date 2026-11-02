#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\generic-glibc\\bits\\types\\struct_osockaddr.h"
#ifndef __osockaddr_defined
#define __osockaddr_defined 1

/* This is the 4.3 BSD `struct sockaddr' format, which is used as wire
   format in the grotty old 4.3 `talk' protocol.  */
struct osockaddr
{
  unsigned short int sa_family;
  unsigned char sa_data[14];
};

#endif