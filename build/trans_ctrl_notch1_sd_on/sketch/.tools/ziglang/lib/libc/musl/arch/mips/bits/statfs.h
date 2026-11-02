#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\arch\\mips\\bits\\statfs.h"
struct statfs {
	unsigned long f_type, f_bsize, f_frsize;
	fsblkcnt_t f_blocks, f_bfree;
	fsfilcnt_t f_files, f_ffree;
	fsblkcnt_t f_bavail;
	fsid_t f_fsid;
	unsigned long f_namelen, f_flags, f_spare[5];
};
