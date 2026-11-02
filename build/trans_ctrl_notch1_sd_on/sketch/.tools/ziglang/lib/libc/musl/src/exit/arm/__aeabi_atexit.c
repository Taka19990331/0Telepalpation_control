#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\exit\\arm\\__aeabi_atexit.c"
int __cxa_atexit(void (*func)(void *), void *arg, void *dso);

int __aeabi_atexit (void *obj, void (*func) (void *), void *d)
{
	return __cxa_atexit (func, obj, d);
}
