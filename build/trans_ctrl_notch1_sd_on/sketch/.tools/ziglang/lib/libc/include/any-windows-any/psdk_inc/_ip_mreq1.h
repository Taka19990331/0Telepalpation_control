#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-windows-any\\psdk_inc\\_ip_mreq1.h"
/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */

/*
 * Shared data types between winsock and winsock2
 */

#ifndef _MINGW_IP_MREQ1_H
#define _MINGW_IP_MREQ1_H

#include <inaddr.h>

typedef struct ip_mreq {
  struct in_addr imr_multiaddr;
  struct in_addr imr_interface;
} IP_MREQ, *PIP_MREQ;

#endif	/* _MINGW_IP_MREQ1_H */
