#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\arch\\x86_64\\crt_arch.h"
__asm__(
".text \n"
".global " START " \n"
".type " START ",%function \n"
START ": \n"
"	xor %rbp,%rbp \n"
"	mov %rsp,%rdi \n"
".weak _DYNAMIC \n"
".hidden _DYNAMIC \n"
"	lea _DYNAMIC(%rip),%rsi \n"
"	andq $-16,%rsp \n"
"	call " START "_c \n"
);
