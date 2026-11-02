#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\search\\tsearch.h"
#include <search.h>
#include <features.h>

/* AVL tree height < 1.44*log2(nodes+2)-0.3, MAXH is a safe upper bound.  */
#define MAXH (sizeof(void*)*8*3/2)

struct node {
	const void *key;
	void *a[2];
	int h;
};

hidden int __tsearch_balance(void **);
