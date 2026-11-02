#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\include\\signal.h"
#ifndef SIGNAL_H
#define SIGNAL_H

#include "../../include/signal.h"

hidden int __sigaction(int, const struct sigaction *, struct sigaction *);

hidden void __block_all_sigs(void *);
hidden void __block_app_sigs(void *);
hidden void __restore_sigs(void *);

hidden void __get_handler_set(sigset_t *);

#endif
