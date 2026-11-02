#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\loongarch64-linux-musl\\bits\\user.h"
#define ELF_NGREG    45
#define ELF_NFPREG   34

struct user_regs_struct {
	unsigned long regs[32];
	unsigned long orig_a0;
	unsigned long csr_era;
	unsigned long csr_badv;
	unsigned long reserved[10];
};

struct user_fp_struct {
	unsigned long fpr[32];
	unsigned long fcc;
	unsigned int fcsr;
};

typedef unsigned long elf_greg_t, elf_gregset_t[ELF_NGREG];

typedef union {
	double d;
	float f;
} elf_fpreg_t;
typedef elf_fpreg_t elf_fpregset_t[ELF_NFPREG];