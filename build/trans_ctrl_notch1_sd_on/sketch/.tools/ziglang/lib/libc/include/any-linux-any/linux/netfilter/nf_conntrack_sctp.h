#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-linux-any\\linux\\netfilter\\nf_conntrack_sctp.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _NF_CONNTRACK_SCTP_H
#define _NF_CONNTRACK_SCTP_H
/* SCTP tracking. */

#include <linux/netfilter/nf_conntrack_tuple_common.h>

enum sctp_conntrack {
	SCTP_CONNTRACK_NONE,
	SCTP_CONNTRACK_CLOSED,
	SCTP_CONNTRACK_COOKIE_WAIT,
	SCTP_CONNTRACK_COOKIE_ECHOED,
	SCTP_CONNTRACK_ESTABLISHED,
	SCTP_CONNTRACK_SHUTDOWN_SENT,
	SCTP_CONNTRACK_SHUTDOWN_RECD,
	SCTP_CONNTRACK_SHUTDOWN_ACK_SENT,
	SCTP_CONNTRACK_HEARTBEAT_SENT,
	SCTP_CONNTRACK_HEARTBEAT_ACKED,	/* no longer used */
	SCTP_CONNTRACK_MAX
};

#endif /* _NF_CONNTRACK_SCTP_H */