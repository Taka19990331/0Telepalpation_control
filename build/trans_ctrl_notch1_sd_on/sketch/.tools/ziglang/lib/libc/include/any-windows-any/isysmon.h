#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-windows-any\\isysmon.h"
/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef _INC_ISYSMON
#define _INC_ISYSMON
#if (_WIN32_WINNT >= 0x0600)
#ifdef __cplusplus
extern "C" {
#endif

typedef enum _SysmonDataType {
  sysmonDataAvg     = 1,
  sysmonDataMin     = 2,
  sysmonDataMax     = 3,
  sysmonDataTime    = 4,
  sysmonDataCount   = 5 
} SysmonDataType;

typedef enum _SysmonBatchReason {
  SysmonBatchNone          = 0,
  SysmonBatchAddFiles      = 1,
  SysmonBatchAddCounters   = 2 
} SysmonBatchReason;

#ifdef __cplusplus
}
#endif
#endif /*(_WIN32_WINNT >= 0x0600)*/
#endif /*_INC_ISYSMON*/
