#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libcxx\\include\\__new\\align_val_t.h"
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___NEW_ALIGN_VAL_T_H
#define _LIBCPP___NEW_ALIGN_VAL_T_H

#include <__config>
#include <__cstddef/size_t.h>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_BEGIN_UNVERSIONED_NAMESPACE_STD
#if _LIBCPP_HAS_LIBRARY_ALIGNED_ALLOCATION && !defined(_LIBCPP_ABI_VCRUNTIME)
#  ifndef _LIBCPP_CXX03_LANG
enum class align_val_t : size_t {};
#  else
enum align_val_t { __zero = 0, __max = (size_t)-1 };
#  endif
#endif
_LIBCPP_END_UNVERSIONED_NAMESPACE_STD

#endif // _LIBCPP___NEW_ALIGN_VAL_T_H
