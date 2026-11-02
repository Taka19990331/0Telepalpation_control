#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\arch\\mips64\\kstat.h"
struct kstat {
	unsigned st_dev;
	int __pad1[3];
	ino_t st_ino;
	mode_t st_mode;
	unsigned st_nlink;
	uid_t st_uid;
	gid_t st_gid;
	unsigned st_rdev;
	int __pad2[3];
	off_t st_size;
	int st_atime_sec;
	int st_atime_nsec;
	int st_mtime_sec;
	int st_mtime_nsec;
	int st_ctime_sec;
	int st_ctime_nsec;
	unsigned st_blksize;
	unsigned __pad3;
	blkcnt_t st_blocks;
};
