#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libcxx\\include\\__cstddef\\ptrdiff_t.h"
//===---------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//

#ifndef _LIBCPP___CSTDDEF_PTRDIFF_T_H
#define _LIBCPP___CSTDDEF_PTRDIFF_T_H

#include <__config>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

using ptrdiff_t = decltype(static_cast<int*>(nullptr) - static_cast<int*>(nullptr));

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___CSTDDEF_PTRDIFF_T_H
