#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\libsrc\\dxerr8w.c"
/*

	dxerr8w.c - DirectX 8 Wide Character Error Functions

	Written by Filip Navara <xnavara@volny.cz>

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

*/

#define UNICODE
#define _UNICODE
#define DXGetErrorString	DXGetErrorString8W
#define DXGetErrorDescription	DXGetErrorDescription8W
#define DXTrace	DXTraceW
#define DXERROR8(v,n,d) {v, L##n, L##d},
#define DXERROR8LAST(v,n,d) {v, L##n, L##d}
#include "dxerr.c"

