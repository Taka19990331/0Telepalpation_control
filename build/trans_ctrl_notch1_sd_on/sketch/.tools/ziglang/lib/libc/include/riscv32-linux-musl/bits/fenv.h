#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\riscv32-linux-musl\\bits\\fenv.h"
#ifdef __riscv_float_abi_soft
#define FE_ALL_EXCEPT   0
#define FE_TONEAREST    0
#else
#define FE_INVALID      16
#define FE_DIVBYZERO    8
#define FE_OVERFLOW     4
#define FE_UNDERFLOW    2
#define FE_INEXACT      1

#define FE_ALL_EXCEPT   31

#define FE_TONEAREST    0
#define FE_DOWNWARD     2
#define FE_UPWARD       3
#define FE_TOWARDZERO   1
#endif

typedef unsigned int fexcept_t;
typedef unsigned int fenv_t;

#define FE_DFL_ENV      ((const fenv_t *) -1)
