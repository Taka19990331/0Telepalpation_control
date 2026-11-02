#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-windows-any\\mdcommsg.h"
/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef _COMMSG_H_
#define _COMMSG_H_

#define RETURNCODETOHRESULT(rc) (((rc) < 0x10000) ? HRESULT_FROM_WIN32(rc) : (rc))
#define HRESULTTOWIN32(hres) ((HRESULT_FACILITY(hres)==FACILITY_WIN32) ? HRESULT_CODE(hres) : (hres))

#endif
