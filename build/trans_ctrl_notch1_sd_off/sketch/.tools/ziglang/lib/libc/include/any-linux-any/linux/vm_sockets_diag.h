#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-linux-any\\linux\\vm_sockets_diag.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/* AF_VSOCK sock_diag(7) interface for querying open sockets */

#ifndef __VM_SOCKETS_DIAG_H__
#define __VM_SOCKETS_DIAG_H__

#include <linux/types.h>

/* Request */
struct vsock_diag_req {
	__u8	sdiag_family;	/* must be AF_VSOCK */
	__u8	sdiag_protocol;	/* must be 0 */
	__u16	pad;		/* must be 0 */
	__u32	vdiag_states;	/* query bitmap (e.g. 1 << TCP_LISTEN) */
	__u32	vdiag_ino;	/* must be 0 (reserved) */
	__u32	vdiag_show;	/* must be 0 (reserved) */
	__u32	vdiag_cookie[2];
};

/* Response */
struct vsock_diag_msg {
	__u8	vdiag_family;	/* AF_VSOCK */
	__u8	vdiag_type;	/* SOCK_STREAM or SOCK_DGRAM */
	__u8	vdiag_state;	/* sk_state (e.g. TCP_LISTEN) */
	__u8	vdiag_shutdown; /* local RCV_SHUTDOWN | SEND_SHUTDOWN */
	__u32   vdiag_src_cid;
	__u32   vdiag_src_port;
	__u32   vdiag_dst_cid;
	__u32   vdiag_dst_port;
	__u32	vdiag_ino;
	__u32	vdiag_cookie[2];
};

#endif /* __VM_SOCKETS_DIAG_H__ */