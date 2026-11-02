#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\x86_64-linux-muslx32\\bits\\statfs.h"
struct statfs {
	unsigned long f_type, __pad0, f_bsize, __pad1;
	fsblkcnt_t f_blocks, f_bfree, f_bavail;
	fsfilcnt_t f_files, f_ffree;
	fsid_t f_fsid;
	unsigned long f_namelen, __pad2, f_frsize, __pad3;
	unsigned long f_flags, __pad4;
	unsigned long long f_spare[4];
};