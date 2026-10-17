#pragma once
#include "../sdk/ds2fx_graphics.h"
namespace fx::graphics {
uint64_t subscribe(HMODULE owner,const DS2FX_GraphicsCallback* callback);
uint32_t unsubscribe(HMODULE owner,uint64_t handle);
void shutdown();
}
