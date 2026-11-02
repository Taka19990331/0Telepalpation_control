#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libcxx\\libc\\src\\__support\\number_pair.h"
//===-- Utilities for pairs of numbers. -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NUMBER_PAIR_H
#define LLVM_LIBC_SRC___SUPPORT_NUMBER_PAIR_H

#include "CPP/type_traits.h"
#include "src/__support/macros/config.h"

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {

template <typename T> struct NumberPair {
  T lo = T(0);
  T hi = T(0);
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NUMBER_PAIR_H
