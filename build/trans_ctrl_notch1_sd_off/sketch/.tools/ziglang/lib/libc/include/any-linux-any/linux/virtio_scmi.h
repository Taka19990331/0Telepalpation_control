#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-linux-any\\linux\\virtio_scmi.h"
/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 * Copyright (C) 2020-2021 OpenSynergy GmbH
 * Copyright (C) 2021 ARM Ltd.
 */

#ifndef _LINUX_VIRTIO_SCMI_H
#define _LINUX_VIRTIO_SCMI_H

#include <linux/virtio_types.h>

/* Device implements some SCMI notifications, or delayed responses. */
#define VIRTIO_SCMI_F_P2A_CHANNELS 0

/* Device implements any SCMI statistics shared memory region */
#define VIRTIO_SCMI_F_SHARED_MEMORY 1

/* Virtqueues */

#define VIRTIO_SCMI_VQ_TX 0 /* cmdq */
#define VIRTIO_SCMI_VQ_RX 1 /* eventq */
#define VIRTIO_SCMI_VQ_MAX_CNT 2

#endif /* _LINUX_VIRTIO_SCMI_H */