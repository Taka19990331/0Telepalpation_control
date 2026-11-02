#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-bottom-half\\sources\\complex-builtins.c"
// Each of the following complex functions can be implemented with a single
// wasm instruction, so use that implementation rather than the portable
// one in libm.

#include <complex.h>

float (crealf)(float _Complex x) {
    return __builtin_crealf(x);
}

double (creal)(double _Complex x) {
    return __builtin_creal(x);
}

long double (creall)(long double _Complex x) {
    return __builtin_creall(x);
}

float (cimagf)(float _Complex x) {
    return __builtin_cimagf(x);
}

double (cimag)(double _Complex x) {
    return __builtin_cimag(x);
}

long double (cimagl)(long double _Complex x) {
    return __builtin_cimagl(x);
}
