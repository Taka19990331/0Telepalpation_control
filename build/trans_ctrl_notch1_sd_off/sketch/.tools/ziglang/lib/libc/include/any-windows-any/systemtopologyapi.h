#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-windows-any\\systemtopologyapi.h"
/**
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER within this package.
 */
#ifndef _SYSTEMTOPOLOGY_H_
#define _SYSTEMTOPOLOGY_H_

#include <apiset.h>
#include <apisetcconv.h>
#include <minwindef.h>
#include <minwinbase.h>

#ifdef __cplusplus
extern "C" {
#endif

#if WINAPI_FAMILY_PARTITION (WINAPI_PARTITION_DESKTOP) || NTDDI_VERSION >= NTDDI_WIN10_19H1
  WINBASEAPI WINBOOL WINAPI GetNumaHighestNodeNumber (PULONG HighestNodeNumber);
#endif

#if WINAPI_FAMILY_PARTITION (WINAPI_PARTITION_DESKTOP)
#if _WIN32_WINNT >= 0x0601
  WINBASEAPI WINBOOL WINAPI GetNumaNodeProcessorMaskEx (USHORT Node, PGROUP_AFFINITY ProcessorMask);
#endif
#endif

#ifdef __cplusplus
}
#endif
#endif
