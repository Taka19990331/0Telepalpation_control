#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\gdtoa\\gdtoa_fltrnds.h"
	FPI *fpi, fpi1;
	int Rounding;
#ifdef Trust_FLT_ROUNDS /*{{ only define this if FLT_ROUNDS really works! */
	Rounding = Flt_Rounds;
#else /*}{*/
	Rounding = 1;
	switch(fegetround()) {
	  case FE_TOWARDZERO:	Rounding = 0; break;
	  case FE_UPWARD:	Rounding = 2; break;
	  case FE_DOWNWARD:	Rounding = 3;
	}
#endif /*}}*/
	fpi = &fpi0;
	if (Rounding != 1) {
		fpi1 = fpi0;
		fpi = &fpi1;
		fpi1.rounding = Rounding;
	}
