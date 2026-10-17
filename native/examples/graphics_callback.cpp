#include "../sdk/ds2fx_api.h"
static const DS2FX_Api* host=nullptr;
static uint64_t handle=0;
static void frame(const DS2FX_GraphicsFrame* graphics,void*) {
    // Borrow context/back_buffer for this callback only. Never call engine APIs
    // here. Save/restore every graphics binding you modify. This sample observes
    // presentation without changing the image, HUD leases, controls or saves.
    if(graphics && graphics->version==1 && graphics->sequence%600==0)
        host->log("graphics-example","Core graphics callback is active");
}
static void event(uint32_t,const DS2FX_PlayerState*){}
DS2FX_EXPORT uint32_t DS2FX_InitializePlugin(const DS2FX_Api* api,DS2FX_Plugin* output) {
    if(!api || !output || api->version!=2 || api->size<offsetof(DS2FX_Api,unregister_graphics_callback)+sizeof(api->unregister_graphics_callback) || !api->register_graphics_callback)return 0;
    host=api;DS2FX_GraphicsCallback callback{sizeof(callback),1,frame,nullptr};
    handle=api->register_graphics_callback(&callback);if(!handle)return 0;
    *output={sizeof(*output),2,"graphics-example","0.1.0",event};return 1;
}
