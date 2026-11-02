#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\arch\\riscv32\\bits\\fenv.h"
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

typedef unsigned int fexcept_t;
typedef unsigned int fenv_t;

#define FE_DFL_ENV      ((const fenv_t *) -1)
