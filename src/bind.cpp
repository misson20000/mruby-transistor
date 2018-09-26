#include<mruby.h>
#include<mruby/array.h>
#include<mruby/value.h>
#include<mruby/string.h>

#include<libtransistor/types.h>
#include<libtransistor/svc.h>
#include<libtransistor/address_space.h>
#include<libtransistor/ipc/pm.h>

#include<string>
#include<array>
#include<utility>

#include "trn.h"

struct BindTableEntry {
	const char *name;
	mrb_value (*func)(mrb_state *mrb, mrb_value self);
	size_t num_args;
};

template<auto>
struct S;

template<typename Ret, typename... Args, Ret (*Func)(Args...)>
struct S<Func> {
	template<size_t I>
	using get_arg = std::tuple_element_t<I, std::tuple<Args...>>;
	static mrb_value Bind(mrb_state* mrb, mrb_value self) {
		std::string arg_spec(sizeof...(Args), 'i');
		std::array<uint64_t, sizeof...(Args)> u64_args;
		return std::apply([&](auto&... args) {
				mrb_get_args(mrb, arg_spec.c_str(), &args...);
				if constexpr(std::is_void<Ret>::value) {
					Func(((Args) args)...);
					return mrb_fixnum_value(0);
				} else if constexpr(std::is_same<Ret, void*>::value) {
					return mrb_fixnum_value((uint64_t) Func(((Args) args)...));
				} else {
					return mrb_fixnum_value(Func(((Args) args)...));
				}
			}, u64_args);
	}

	
	static BindTableEntry MakeEntry(const char *name) {
		return {name, &Bind, sizeof...(Args)};
	}
};

static void bind(mrb_state *mrb, BindTableEntry *table, struct RClass *mod) {
	for(BindTableEntry *head = table; head->name != NULL; head++) {
		mrb_define_class_method(mrb, mod, head->name, head->func, MRB_ARGS_ARG(head->num_args, 0));
	}
}

extern "C" void mrb_transistor_bind_init(mrb_state *mrb) {
	BindTableEntry bind_table[] = {
		S<malloc>::MakeEntry("malloc"),
		S<free>::MakeEntry("free"),
		S<as_reserve>::MakeEntry("as_reserve"),
		S<as_release>::MakeEntry("as_release"),
		{NULL, NULL, 0}
	};
	bind(mrb, bind_table, mod_transistor_ll);
	
	BindTableEntry svc_table[] = {
		S<svcSetHeapSize>::MakeEntry("set_heap_size"),
		S<svcSetMemoryPermission>::MakeEntry("set_memory_permission"),
		S<svcSetMemoryAttribute>::MakeEntry("set_memory_attribute"),
		S<svcMapMemory>::MakeEntry("map_memory"),
		S<svcUnmapMemory>::MakeEntry("unmap_memory"),
		S<svcQueryMemory>::MakeEntry("query_memory"),
		S<svcExitProcess>::MakeEntry("exit_process"),
		S<svcCreateThread>::MakeEntry("create_thread"),
		S<svcStartThread>::MakeEntry("start_thread"),
		S<svcExitThread>::MakeEntry("exit_thread"),
		S<svcSleepThread>::MakeEntry("sleep_thread"),
		S<svcGetThreadPriority>::MakeEntry("get_thread_priority"),
		S<svcSetThreadCoreMask>::MakeEntry("set_thread_core_mask"),
		S<svcGetCurrentProcessorNumber>::MakeEntry("get_current_processor_number"),
		S<svcSignalEvent>::MakeEntry("signal_event"),
		S<svcClearEvent>::MakeEntry("clear_event"),
		S<svcMapSharedMemory>::MakeEntry("map_shared_memory"),
		S<svcUnmapSharedMemory>::MakeEntry("unmap_shared_memory"),
		S<svcCreateTransferMemory>::MakeEntry("create_transfer_memory"),
		S<svcCloseHandle>::MakeEntry("close_handle"),
		S<svcResetSignal>::MakeEntry("reset_signal"),
		S<svcWaitSynchronization>::MakeEntry("wait_synchronization"),
		S<svcCancelSynchronization>::MakeEntry("cancel_synchronization"),
		S<svcArbitrateLock>::MakeEntry("arbitrate_lock"),
		S<svcArbitrateUnlock>::MakeEntry("arbitrate_unlock"),
		S<svcWaitProcessWideKeyAtomic>::MakeEntry("wait_process_wide_key_atomic"),
		S<svcSignalProcessWideKey>::MakeEntry("signal_process_wide_key"),
		S<svcGetSystemTick>::MakeEntry("get_system_tick"),
		S<svcConnectToNamedPort>::MakeEntry("connect_to_named_port"),
		S<svcSendSyncRequest>::MakeEntry("send_sync_request"),
		S<svcSendSyncRequestWithUserBuffer>::MakeEntry("send_sync_request_with_user_buffer"),
		S<svcGetProcessId>::MakeEntry("get_process_id"),
		S<svcGetThreadId>::MakeEntry("get_thread_id"),
		S<svcOutputDebugString>::MakeEntry("output_debug_string"),
		S<svcReturnFromException>::MakeEntry("return_from_exception"),
		S<svcGetInfo>::MakeEntry("get_info"),
		S<svcCreateSession>::MakeEntry("create_session"),
		S<svcAcceptSession>::MakeEntry("accept_session"),
		S<svcReplyAndReceive>::MakeEntry("reply_and_receive"),
		S<svcReplyAndReceiveWithUserBuffer>::MakeEntry("reply_and_receive_with_user_buffer"),
		S<svcReadWriteRegister>::MakeEntry("read_write_register"),
		S<svcCreateSharedMemory>::MakeEntry("create_shared_memory"),
		S<svcMapTransferMemory>::MakeEntry("map_transfer_memory"),
		S<svcUnmapTransferMemory>::MakeEntry("unmap_transfer_memory"),
		S<svcQueryIoMapping>::MakeEntry("query_io_mapping"),
		S<svcAttachDeviceAddressSpace>::MakeEntry("attach_device_address_space"),
		S<svcDetachDeviceAddressSpace>::MakeEntry("detach_device_address_space"),
		S<svcMapDeviceAddressSpaceByForce>::MakeEntry("map_device_address_space_by_force"),
		S<svcMapDeviceAddressSpaceAligned>::MakeEntry("map_device_address_space_aligned"),
		S<svcUnmapDeviceAddressSpace>::MakeEntry("unmap_device_address_space"),
		S<svcDebugActiveProcess>::MakeEntry("debug_active_process"),
		S<svcQueryDebugProcessMemory>::MakeEntry("query_debug_process_memory"),
		S<svcReadDebugProcessMemory>::MakeEntry("read_debug_process_memory"),
		S<svcWriteDebugProcessMemory>::MakeEntry("write_debug_process_memory"),
		S<svcSetProcessMemoryPermission>::MakeEntry("set_process_memory_permission"),
		S<svcMapProcessCodeMemory>::MakeEntry("map_process_code_memory"),
		S<svcUnmapProcessCodeMemory>::MakeEntry("unmap_process_code_memory"),
		{NULL, NULL, 0}
	};
	bind(mrb, svc_table, mrb_define_module_under(mrb, mod_transistor_ll, "SVC"));

	BindTableEntry pm_table[] = {
		S<pm_init>::MakeEntry("init"),
		S<pm_terminate_process_by_title_id>::MakeEntry("terminate_process_by_title_id"),
		S<pm_finalize>::MakeEntry("finalize"),
		{NULL, NULL, 0}
	};
	bind(mrb, pm_table, mrb_define_module_under(mrb, mod_transistor_ll, "PM"));
}
