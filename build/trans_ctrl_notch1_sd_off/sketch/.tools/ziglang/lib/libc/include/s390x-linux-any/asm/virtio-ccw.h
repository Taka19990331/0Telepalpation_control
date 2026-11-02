#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\s390x-linux-any\\asm\\virtio-ccw.h"
/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 * Definitions for virtio-ccw devices.
 *
 * Copyright IBM Corp. 2013
 *
 *  Author(s): Cornelia Huck <cornelia.huck@de.ibm.com>
 */
#ifndef __KVM_VIRTIO_CCW_H
#define __KVM_VIRTIO_CCW_H

/* Alignment of vring buffers. */
#define KVM_VIRTIO_CCW_RING_ALIGN 4096

/* Subcode for diagnose 500 (virtio hypercall). */
#define KVM_S390_VIRTIO_CCW_NOTIFY 3

#endif