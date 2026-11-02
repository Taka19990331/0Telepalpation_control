#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\ipc\\ftok.c"
#include <sys/ipc.h>
#include <sys/stat.h>

key_t ftok(const char *path, int id)
{
	struct stat st;
	if (stat(path, &st) < 0) return -1;

	return ((st.st_ino & 0xffff) | ((st.st_dev & 0xff) << 16) | ((id & 0xffu) << 24));
}
