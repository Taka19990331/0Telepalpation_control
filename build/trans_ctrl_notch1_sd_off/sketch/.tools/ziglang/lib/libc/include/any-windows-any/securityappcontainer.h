#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-windows-any\\securityappcontainer.h"
/**
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER within this package.
 */
#ifndef _APIAPPCONTAINER_
#define _APIAPPCONTAINER_

#include <apiset.h>
#include <apisetcconv.h>
#include <minwindef.h>
#include <minwinbase.h>

#if WINAPI_FAMILY_PARTITION (WINAPI_PARTITION_DESKTOP)
#ifdef __cplusplus
extern "C" {
#endif

#if NTDDI_VERSION >= 0x06020000
  WINBOOL GetAppContainerNamedObjectPath (HANDLE Token, PSID AppContainerSid, ULONG ObjectPathLength, LPWSTR ObjectPath, PULONG ReturnLength);
#endif

#ifdef __cplusplus
}
#endif
#endif
#endif
