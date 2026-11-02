#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\arch\\generic\\bits\\ipc.h"
struct ipc_perm {
	key_t __ipc_perm_key;
	uid_t uid;
	gid_t gid;
	uid_t cuid;
	gid_t cgid;
	mode_t mode;
	int __ipc_perm_seq;
	long __pad1;
	long __pad2;
};
