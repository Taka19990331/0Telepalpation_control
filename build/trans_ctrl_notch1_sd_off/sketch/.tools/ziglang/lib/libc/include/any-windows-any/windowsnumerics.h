#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-windows-any\\windowsnumerics.h"
/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef _WINDOWSNUMERICS_
#define _WINDOWSNUMERICS_

#ifndef __cplusplus
#error windowsnumerics.h requires C++
#endif /* __cplusplus */

#define _WINDOWS_NUMERICS_NAMESPACE_ Windows::Foundation::Numerics

#define _WINDOWS_NUMERICS_BEGIN_NAMESPACE_ \
  namespace Windows { \
    namespace Foundation { \
      namespace Numerics

#define _WINDOWS_NUMERICS_END_NAMESPACE_ \
    } \
  }

#include "windowsnumerics.impl.h"

#undef _WINDOWS_NUMERICS_NAMESPACE_
#undef _WINDOWS_NUMERICS_BEGIN_NAMESPACE_
#undef _WINDOWS_NUMERICS_END_NAMESPACE_

#endif /* _WINDOWSNUMERICS_ */
