#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\x86-linux-any\\asm\\kvm_perf.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _ASM_X86_KVM_PERF_H
#define _ASM_X86_KVM_PERF_H

#include <asm/svm.h>
#include <asm/vmx.h>
#include <asm/kvm.h>

#define DECODE_STR_LEN 20

#define VCPU_ID "vcpu_id"

#define KVM_ENTRY_TRACE "kvm:kvm_entry"
#define KVM_EXIT_TRACE "kvm:kvm_exit"
#define KVM_EXIT_REASON "exit_reason"

#endif /* _ASM_X86_KVM_PERF_H */