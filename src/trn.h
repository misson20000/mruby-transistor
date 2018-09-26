#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include<mruby.h>

extern struct RClass *mod_transistor;
extern struct RClass *mod_transistor_ll;
extern struct RClass *exc_ResultError;

void mrb_trn_assert_ok(mrb_state *mrb, result_t r);
void mrb_transistor_bind_init(mrb_state *mrb);
void mrb_transistor_ipc_init(mrb_state *mrb);

#ifdef __cplusplus
}
#endif
