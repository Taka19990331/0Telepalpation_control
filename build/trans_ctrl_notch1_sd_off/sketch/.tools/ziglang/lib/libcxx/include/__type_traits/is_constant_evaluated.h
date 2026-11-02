#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libcxx\\include\\__type_traits\\is_constant_evaluated.h"
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___TYPE_TRAITS_IS_CONSTANT_EVALUATED_H
#define _LIBCPP___TYPE_TRAITS_IS_CONSTANT_EVALUATED_H

#include <__config>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

#if _LIBCPP_STD_VER >= 20
_LIBCPP_HIDE_FROM_ABI inline constexpr bool is_constant_evaluated() noexcept {
  return __builtin_is_constant_evaluated();
}
#endif

_LIBCPP_HIDE_FROM_ABI inline _LIBCPP_CONSTEXPR bool __libcpp_is_constant_evaluated() _NOEXCEPT {
  return __builtin_is_constant_evaluated();
}

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___TYPE_TRAITS_IS_CONSTANT_EVALUATED_H
