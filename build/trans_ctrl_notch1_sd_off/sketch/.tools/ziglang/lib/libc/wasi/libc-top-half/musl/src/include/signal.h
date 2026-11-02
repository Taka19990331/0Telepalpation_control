#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-top-half\\musl\\src\\include\\signal.h"
#ifndef SIGNAL_H
#define SIGNAL_H

#include "../../include/signal.h"

#ifdef __wasilibc_unmodified_upstream // WASI has no sigaction
hidden int __sigaction(int, const struct sigaction *, struct sigaction *);

hidden void __block_all_sigs(void *);
hidden void __block_app_sigs(void *);
hidden void __restore_sigs(void *);

hidden void __get_handler_set(sigset_t *);
#endif

#endif
