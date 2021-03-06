#include<stdint.h>
#include<malloc.h>
#include<string.h>

#include<mruby.h>
#include<mruby/array.h>
#include<mruby/value.h>
#include<mruby/string.h>
#include<mruby/data.h>

#include<libtransistor/nx.h>

#include "trn.h"

struct RClass *mod_transistor;
struct RClass *mod_transistor_ll;
struct RClass *exc_ResultError;

void mrb_trn_assert_ok(mrb_state *mrb, result_t r) {
	if(r != RESULT_OK) {
		printf("result error: 0x%x\n", r);
		mrb_raise(mrb, exc_ResultError, "ResultError");
	}
}

static mrb_value mrb_trn_alloc_pages(mrb_state *mrb, mrb_value self) {
	size_t min, max;
	mrb_int num_args = mrb_get_args(mrb, "ii", &min, &max);
	if(num_args == 1) {
		max = min;
	}

	size_t actual;
	void *ptr = alloc_pages(min, max, &actual);
	if(ptr == NULL) {
		mrb_raise(mrb, E_RUNTIME_ERROR, "out of memory");
	}

	mrb_value array = mrb_ary_new(mrb);
	mrb_ary_push(mrb, array, mrb_fixnum_value((uint64_t) ptr));
	mrb_ary_push(mrb, array, mrb_fixnum_value(actual));
	return array;
}

static mrb_value mrb_trn_malloc(mrb_state *mrb, mrb_value self) {
	size_t sz;
	mrb_int num_args = mrb_get_args(mrb, "i", &sz);
	return mrb_fixnum_value((uint64_t) malloc(sz));
}

static mrb_value mrb_trn_free(mrb_state *mrb, mrb_value self) {
	uint64_t ptr;
	mrb_int num_args = mrb_get_args(mrb, "i", &ptr);
	free((void*) ptr);
	return mrb_fixnum_value(0);
}

static mrb_value mrb_trn_read(mrb_state *mrb, mrb_value self) {
	uint64_t ptr;
	size_t size;
	mrb_int num_args = mrb_get_args(mrb, "ii", &ptr, &size);

	mrb_value str = mrb_str_buf_new(mrb, size);
	return mrb_str_cat(mrb, str, (const char*) ptr, size);
}

static mrb_value mrb_trn_write(mrb_state *mrb, mrb_value self) {
	uint64_t dst;
	char *str;
	size_t size;
	mrb_int num_args = mrb_get_args(mrb, "is", &dst, &str, &size);

	memcpy((void*) dst, str, size);

	return mrb_fixnum_value(0);
}

static mrb_value mrb_trn_get_process_handle(mrb_state *mrb, mrb_value self) {
	return mrb_fixnum_value(loader_config.process_handle);
}

void mrb_transistor_gem_init(mrb_state *mrb) {
	mod_transistor = mrb_define_module(mrb, "TRN");

	exc_ResultError = mrb_define_class_under(mrb, mod_transistor, "ResultError", E_RUNTIME_ERROR);
	
	// lowlevel
	mod_transistor_ll = mrb_define_module_under(mrb, mod_transistor, "LL");
	mrb_define_class_method(mrb, mod_transistor_ll, "alloc_pages", mrb_trn_alloc_pages, MRB_ARGS_ARG(1, 1));
	mrb_define_class_method(mrb, mod_transistor_ll, "malloc", mrb_trn_malloc, MRB_ARGS_ARG(1, 0));
	mrb_define_class_method(mrb, mod_transistor_ll, "free", mrb_trn_free, MRB_ARGS_ARG(1, 0));
	mrb_define_class_method(mrb, mod_transistor_ll, "read", mrb_trn_read, MRB_ARGS_ARG(2, 0));
	mrb_define_class_method(mrb, mod_transistor_ll, "write", mrb_trn_write, MRB_ARGS_ARG(2, 0));
	mrb_define_class_method(mrb, mod_transistor_ll, "process_handle", mrb_trn_get_process_handle, MRB_ARGS_ARG(0, 0));

	// other modules
	mrb_transistor_bind_init(mrb);
	mrb_transistor_ipc_init(mrb);
}

void mrb_transistor_gem_final(mrb_state *mrb) {
}
