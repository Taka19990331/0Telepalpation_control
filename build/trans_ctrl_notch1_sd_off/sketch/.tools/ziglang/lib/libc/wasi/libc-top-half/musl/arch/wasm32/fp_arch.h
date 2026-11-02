#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-top-half\\musl\\arch\\wasm32\\fp_arch.h"
// WebAssembly has no floating-point exceptions or alternate rounding modes,
// so there's no need to prevent expressions from moving or force their
// evaluation.

#define fp_barrierf fp_barrierf
static inline float fp_barrierf(float x)
{
	return x;
}

#define fp_barrier fp_barrier
static inline double fp_barrier(double x)
{
	return x;
}

#define fp_force_evalf fp_force_evalf
static inline void fp_force_evalf(float x)
{
}

#define fp_force_eval fp_force_eval
static inline void fp_force_eval(double x)
{
}
