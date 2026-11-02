#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-windows-any\\psdk_inc\\_wsadata.h"
/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */

#ifndef __MINGW_WSADATA_H
#define __MINGW_WSADATA_H

#define WSADESCRIPTION_LEN	256
#define WSASYS_STATUS_LEN	128

typedef struct WSAData {
	WORD		wVersion;
	WORD		wHighVersion;
#ifdef _WIN64
	unsigned short	iMaxSockets;
	unsigned short	iMaxUdpDg;
	char		*lpVendorInfo;
	char		szDescription[WSADESCRIPTION_LEN+1];
	char		szSystemStatus[WSASYS_STATUS_LEN+1];
#else
	char		szDescription[WSADESCRIPTION_LEN+1];
	char		szSystemStatus[WSASYS_STATUS_LEN+1];
	unsigned short	iMaxSockets;
	unsigned short	iMaxUdpDg;
	char		*lpVendorInfo;
#endif
} WSADATA, *LPWSADATA;

#endif	/* __MINGW_WSADATA_H */

