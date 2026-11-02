#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\libsrc\\dxerr9.c"
/*

	dxerr9.c - DirectX 9 Error Functions

	Written by Filip Navara <xnavara@volny.cz>

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

*/

#define DXGetErrorString	DXGetErrorString9A
#define DXGetErrorDescription	DXGetErrorDescription9A
#define DXTrace	DXTraceA
#define DXERROR9(v,n,d) {v, n, d},
#define DXERROR9LAST(v,n,d) {v, n, d}
#include "dxerr.c"
