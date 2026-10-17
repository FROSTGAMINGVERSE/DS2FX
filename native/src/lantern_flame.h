#pragma once
#include "common.h"
namespace fx::lantern_flame {
struct Status {uint64_t created,removed,missing_dummy,stale_handle,rejected_thread;int32_t handle;DWORD thread;bool enabled,fault;};
void prepare(uintptr_t base_slot,bool opt_in);
void native_tick(uintptr_t manager) noexcept;
void disable() noexcept;
Status status() noexcept;
#ifdef DS2FX_FIXTURE
using Create=int32_t(__fastcall*)(uintptr_t,uint16_t,int32_t,uint16_t,bool,float,int32_t);
using Remove=void(__fastcall*)(uintptr_t,int32_t);
using Resolve=bool(__fastcall*)(uintptr_t,int32_t,void*,bool);
void prepare_owned_fixture(uintptr_t base_slot,uintptr_t image,Create,Remove,Resolve);
#endif
}
