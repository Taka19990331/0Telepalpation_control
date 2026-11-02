#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\misc\\ftruncate.c"

int _chsize(int _FileHandle,long _Size);
int ftruncate(int __fd,int __length);

int ftruncate(int __fd,int __length)
{
  return _chsize (__fd,__length);
}
