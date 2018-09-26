#include<stdint.h>
#include<malloc.h>
#include<string.h>

#include<mruby.h>
#include<mruby/array.h>
#include<mruby/data.h>
#include<mruby/string.h>
#include<mruby/value.h>
#include<mruby/variable.h>

#include<libtransistor/nx.h>

#include "trn.h"

static struct RClass *mod_transistor_ipc;
static struct RClass *class_ipc_Object;
static struct RClass *class_ipc_Message;

static void mrb_trn_ipc_object_dfree(mrb_state *mrb, void *data) {
	if(data) {
		ipc_close(*((ipc_object_t*) data));
		mrb_free(mrb, data);
	}
}

static const mrb_data_type dt_ipc_Object = {"Object", mrb_trn_ipc_object_dfree};

static mrb_value mrb_trn_get_service(mrb_state *mrb, mrb_value self) {
	char *str;
	mrb_int size;
	mrb_int num_args = mrb_get_args(mrb, "s", &str, &size);

	mrb_trn_assert_ok(mrb, sm_init());

	ipc_object_t *session = mrb_malloc(mrb, sizeof(*session)); // potentital to lose sm reference here but idrc
	result_t r = sm_get_service(session, str);
	sm_finalize();
	if(r != RESULT_OK) {
		free(session);
	}
	mrb_trn_assert_ok(mrb, r);

	return mrb_obj_value(Data_Wrap_Struct(mrb, class_ipc_Object, &dt_ipc_Object, session));
}

typedef struct {
	ipc_request_t rq;
	ipc_response_fmt_t rs;
	uint64_t pid_storage;
} message_format_t;

static void mrb_trn_ipc_message_dfree(mrb_state *mrb, void *data) {
	message_format_t *fmt = data;
	for(int i = 0; i < fmt->rq.num_buffers; i++) {
		mrb_free(mrb, fmt->rq.buffers[i]);
	}
	mrb_free(mrb, fmt->rq.buffers);
	mrb_free(mrb, fmt->rq.raw_data);
	mrb_free(mrb, fmt->rq.copy_handles);
	mrb_free(mrb, fmt->rq.move_handles);
	mrb_free(mrb, fmt->rq.objects);
	mrb_free(mrb, fmt->rs.copy_handles);
	mrb_free(mrb, fmt->rs.move_handles);
	mrb_free(mrb, fmt->rs.objects);
	mrb_free(mrb, fmt->rs.raw_data);
	mrb_free(mrb, fmt->rs.pid);
	mrb_free(mrb, fmt);
}

static mrb_value mrb_trn_ipc_object_close(mrb_state *mrb, mrb_value self) {
	ipc_object_t *obj = mrb_data_get_ptr(mrb, self, &dt_ipc_Object);
	if(obj) {
		ipc_close(*obj);
		mrb_free(mrb, obj);
		DATA_PTR(self) = NULL;
	}
	return mrb_nil_value();
}

static const mrb_data_type dt_ipc_Message = {"Message", mrb_trn_ipc_message_dfree};

static mrb_value mrb_trn_ipc_object_send(mrb_state *mrb, mrb_value self) {
	ipc_object_t *obj = mrb_data_get_ptr(mrb, self, &dt_ipc_Object);
	if(!obj) {
		mrb_raise(mrb, E_RUNTIME_ERROR, "ipc object is closed");
	}
	mrb_value message_value;
	mrb_value input_hash_value;
	
	mrb_int num_args = mrb_get_args(mrb, "oH!", &message_value, &input_hash_value);
	mrb_funcall(mrb, message_value, "_pack", 1, input_hash_value); // pack
	message_format_t *fmt = mrb_data_get_ptr(mrb, message_value, &dt_ipc_Message);
	result_t r = ipc_send(*obj, &fmt->rq, &fmt->rs);
	mrb_trn_assert_ok(mrb, r);
	return mrb_funcall(mrb, message_value, "_unpack", 0);
}

static mrb_value mrb_trn_ipc_message_new(mrb_state *mrb, mrb_value self) {
	message_format_t *fmt = mrb_malloc(mrb, sizeof(*fmt));
	fmt->rq = ipc_default_request;
	fmt->rs = ipc_default_response_fmt;
	
	mrb_value obj = mrb_obj_value(Data_Wrap_Struct(mrb, class_ipc_Message, &dt_ipc_Message, fmt));

	mrb_value *argv;
	mrb_int argc;
	mrb_get_args(mrb, "*", &argv, &argc);
	mrb_funcall_argv(mrb, obj, mrb_intern_lit(mrb, "initialize"), argc, argv);
	return obj;
}

static mrb_value mrb_trn_ipc_message_invalidate(mrb_state *mrb, mrb_value self) {
	message_format_t *fmt = mrb_data_get_ptr(mrb, self, &dt_ipc_Message);

	// request id
	fmt->rq.request_id = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@command_id")));

	// buffers
	mrb_value buffers = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@buffers"));
	// in case we're shrinking
	for(mrb_int i = RARRAY_LEN(buffers); i < fmt->rq.num_buffers; i++) {
		mrb_free(mrb, fmt->rq.buffers[i]);
	}
	fmt->rq.buffers = mrb_realloc(
		mrb, fmt->rq.buffers, RARRAY_LEN(buffers) * sizeof(ipc_buffer_t*));
	// in case we grew
	for(mrb_int i = fmt->rq.num_buffers; i < RARRAY_LEN(buffers); i++) {
		fmt->rq.buffers[i] = mrb_calloc(mrb, sizeof(ipc_buffer_t), 1);
	}
	fmt->rq.num_buffers = RARRAY_LEN(buffers);
	// adjust buffer types
	for(int i = 0; i < fmt->rq.num_buffers; i++) {
		ipc_buffer_t *ipc_buffer = fmt->rq.buffers[i];
		mrb_value buffer = mrb_ary_entry(buffers, i);
		ipc_buffer->type = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@type")));
	}
	
	// raw data
	fmt->rq.raw_data_size =
		mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@in_raw_size")));
	fmt->rs.raw_data_size =
		mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@out_raw_size")));
	fmt->rq.raw_data = mrb_realloc(
		mrb, fmt->rq.raw_data, fmt->rq.raw_data_size);
	fmt->rs.raw_data = mrb_realloc(
		mrb, fmt->rs.raw_data, fmt->rs.raw_data_size);

	// objects
	fmt->rq.num_objects =
		mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@in_object_count")));
	fmt->rs.num_objects =
		mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@out_object_count")));
	fmt->rq.objects = mrb_realloc(
		mrb, fmt->rq.objects, fmt->rq.num_objects * sizeof(ipc_object_t));
	fmt->rs.objects = mrb_realloc(
		mrb, fmt->rs.objects, fmt->rs.num_objects * sizeof(ipc_object_t));

	// copy handles
	fmt->rq.num_copy_handles =
		mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@in_copy_handle_count")));
	fmt->rs.num_copy_handles =
		mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@out_copy_handle_count")));
	fmt->rq.copy_handles = mrb_realloc(
		mrb, fmt->rq.copy_handles, fmt->rq.num_copy_handles * sizeof(handle_t));
	fmt->rs.copy_handles = mrb_realloc(
		mrb, fmt->rs.copy_handles, fmt->rs.num_copy_handles * sizeof(handle_t));

	// copy handles
	fmt->rq.num_move_handles =
		mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@in_move_handle_count")));
	fmt->rs.num_move_handles =
		mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@out_move_handle_count")));
	fmt->rq.move_handles = mrb_realloc(
		mrb, fmt->rq.move_handles, fmt->rq.num_move_handles * sizeof(handle_t));
	fmt->rs.move_handles = mrb_realloc(
		mrb, fmt->rs.move_handles, fmt->rs.num_move_handles * sizeof(handle_t));

	// pid
	fmt->rq.send_pid = mrb_bool(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@send_pid")));
	fmt->rs.has_pid  = mrb_bool(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@recv_pid")));
	fmt->rs.pid = &fmt->pid_storage;
	
	return mrb_nil_value();
}

static mrb_value mrb_trn_ipc_message_copy_in_raw(mrb_state *mrb, mrb_value self) {
	message_format_t *fmt = mrb_data_get_ptr(mrb, self, &dt_ipc_Message);
	mrb_int pos;
	char *data;
	mrb_int size;
	mrb_int num_args = mrb_get_args(mrb, "is", &pos, &data, &size);
	memcpy(fmt->rq.raw_data + pos, data, size);
	return mrb_nil_value();
}

static mrb_value mrb_trn_ipc_message_copy_out_raw(mrb_state *mrb, mrb_value self) {
	message_format_t *fmt = mrb_data_get_ptr(mrb, self, &dt_ipc_Message);
	mrb_int pos;
	mrb_int size;
	mrb_int num_args = mrb_get_args(mrb, "ii", &pos, &size);
	return mrb_str_new(mrb, fmt->rs.raw_data + pos, size);
}

static mrb_value mrb_trn_ipc_message_copy_in_object(mrb_state *mrb, mrb_value self) {
	message_format_t *fmt = mrb_data_get_ptr(mrb, self, &dt_ipc_Message);
	mrb_int index;
	mrb_value object;
	mrb_int num_args = mrb_get_args(mrb, "io", &index, &object);

	ipc_object_t *ipc_object = mrb_data_get_ptr(mrb, object, &dt_ipc_Object);
	fmt->rq.objects[index] = *ipc_object;
	return mrb_nil_value();
}

static mrb_value mrb_trn_ipc_message_copy_out_object(mrb_state *mrb, mrb_value self) {
	message_format_t *fmt = mrb_data_get_ptr(mrb, self, &dt_ipc_Message);
	mrb_int index;
	mrb_int num_args = mrb_get_args(mrb, "i", &index);

	ipc_object_t ipc_object = fmt->rs.objects[index];
	ipc_object_t *storage = malloc(sizeof(ipc_object));
	*storage = ipc_object;
	return mrb_obj_value(Data_Wrap_Struct(mrb, class_ipc_Object, &dt_ipc_Object, storage));
}

static mrb_value mrb_trn_ipc_message_copy_in_copy_handle(mrb_state *mrb, mrb_value self) {
	message_format_t *fmt = mrb_data_get_ptr(mrb, self, &dt_ipc_Message);
	mrb_int index;
	mrb_int handle;
	mrb_int num_args = mrb_get_args(mrb, "ii", &index, &handle);

	fmt->rq.copy_handles[index] = handle;
	return mrb_nil_value();
}

static mrb_value mrb_trn_ipc_message_copy_out_copy_handle(mrb_state *mrb, mrb_value self) {
	message_format_t *fmt = mrb_data_get_ptr(mrb, self, &dt_ipc_Message);
	mrb_int index;
	mrb_int num_args = mrb_get_args(mrb, "i", &index);

	return mrb_fixnum_value(fmt->rq.copy_handles[index]);
}

static mrb_value mrb_trn_ipc_message_copy_in_move_handle(mrb_state *mrb, mrb_value self) {
	message_format_t *fmt = mrb_data_get_ptr(mrb, self, &dt_ipc_Message);
	mrb_int index;
	mrb_int handle;
	mrb_int num_args = mrb_get_args(mrb, "ii", &index, &handle);

	fmt->rq.move_handles[index] = handle;
	return mrb_nil_value();
}

static mrb_value mrb_trn_ipc_message_copy_out_move_handle(mrb_state *mrb, mrb_value self) {
	message_format_t *fmt = mrb_data_get_ptr(mrb, self, &dt_ipc_Message);
	mrb_int index;
	mrb_int num_args = mrb_get_args(mrb, "i", &index);

	return mrb_fixnum_value(fmt->rq.move_handles[index]);
}

static mrb_value mrb_trn_ipc_message_copy_out_pid(mrb_state *mrb, mrb_value self) {
	message_format_t *fmt = mrb_data_get_ptr(mrb, self, &dt_ipc_Message);

	return mrb_fixnum_value(fmt->pid_storage);
}

static mrb_value mrb_trn_ipc_message_set_buffer(mrb_state *mrb, mrb_value self) {
	message_format_t *fmt = mrb_data_get_ptr(mrb, self, &dt_ipc_Message);
	mrb_int index;
	mrb_value string;
	mrb_int num_args = mrb_get_args(mrb, "is", &index, &string);

	fmt->rq.buffers[index]->addr = (void*) mrb_string_value_ptr(mrb, string);
	fmt->rq.buffers[index]->size = mrb_string_value_len(mrb, string);

	return mrb_nil_value();
}

void mrb_transistor_ipc_init(mrb_state *mrb) {
	mod_transistor_ipc = mrb_define_module_under(mrb, mod_transistor, "IPC");
	class_ipc_Object = mrb_define_class_under(mrb, mod_transistor_ipc, "Object", mrb->object_class);
	mrb_define_class_method(mrb, mod_transistor, "get_service", mrb_trn_get_service, MRB_ARGS_ARG(1, 0));
	mrb_define_method(mrb, class_ipc_Object, "close", mrb_trn_ipc_object_close, MRB_ARGS_ARG(0, 0));
	mrb_define_method(mrb, class_ipc_Object, "send", mrb_trn_ipc_object_send, MRB_ARGS_ARG(1, 1));
	class_ipc_Message = mrb_define_class_under(mrb, mod_transistor_ipc, "Message", mrb->object_class);
	mrb_define_class_method(mrb, class_ipc_Message, "new", mrb_trn_ipc_message_new, MRB_ARGS_ARG(1, 0));
	mrb_define_method(mrb, class_ipc_Message, "_invalidate", mrb_trn_ipc_message_invalidate, MRB_ARGS_ARG(0, 0));
	mrb_define_method(mrb, class_ipc_Message, "_copy_in_raw", mrb_trn_ipc_message_copy_in_raw, MRB_ARGS_ARG(2, 0));
	mrb_define_method(mrb, class_ipc_Message, "_copy_out_raw", mrb_trn_ipc_message_copy_out_raw, MRB_ARGS_ARG(2, 0));
	mrb_define_method(mrb, class_ipc_Message, "_copy_in_object", mrb_trn_ipc_message_copy_out_object, MRB_ARGS_ARG(2, 0));
	mrb_define_method(mrb, class_ipc_Message, "_copy_out_object", mrb_trn_ipc_message_copy_out_object, MRB_ARGS_ARG(1, 0));
	mrb_define_method(mrb, class_ipc_Message, "_copy_in_copy_handle", mrb_trn_ipc_message_copy_in_copy_handle, MRB_ARGS_ARG(2, 0));
	mrb_define_method(mrb, class_ipc_Message, "_copy_out_copy_handle", mrb_trn_ipc_message_copy_out_copy_handle, MRB_ARGS_ARG(1, 0));
	mrb_define_method(mrb, class_ipc_Message, "_copy_in_move_handle", mrb_trn_ipc_message_copy_in_move_handle, MRB_ARGS_ARG(2, 0));
	mrb_define_method(mrb, class_ipc_Message, "_copy_out_move_handle", mrb_trn_ipc_message_copy_out_move_handle, MRB_ARGS_ARG(1, 0));
	mrb_define_method(mrb, class_ipc_Message, "_copy_out_pid", mrb_trn_ipc_message_copy_out_pid, MRB_ARGS_ARG(0, 0));
	mrb_define_method(mrb, class_ipc_Message, "_set_buffer", mrb_trn_ipc_message_set_buffer, MRB_ARGS_ARG(2, 0));
}
