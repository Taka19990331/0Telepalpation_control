#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-top-half\\musl\\arch\\wasm32\\atomic_arch.h"
#define a_cas(p, t, s) (__sync_val_compare_and_swap((p), (t), (s)))
#define a_crash() (__builtin_trap())
#define a_clz_32 __builtin_clz
#define a_clz_64 __builtin_clzll
#define a_ctz_32 __builtin_ctz
#define a_ctz_64 __builtin_ctzll
