#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-windows-any\\activecf.h"
/**"
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER within this package.
*/
#include <winapifamily.h>

#if WINAPI_FAMILY_PARTITION (WINAPI_PARTITION_DESKTOP)

#define CFSTR_VFW_FILTERLIST "Video for Windows 4 Filters"

typedef struct tagVFW_FILTERLIST {
  UINT cFilters;
  CLSID aClsId[1];
} VFW_FILTERLIST;

#endif
