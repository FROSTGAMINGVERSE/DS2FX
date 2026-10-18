# DS2FX Core graphics bridge v1

Core 0.5.41-alpha.1 owns an optional SDR D3D11 presentation adapter. A separate
graphics mod, ReShade installation, Lighting Engine, OptiScaler, Remastered list
or HUD is not required. It starts only when a module subscribes. The same source
is compiled into the solo and official Seamless Core runtimes.

ABI 2 retains its original 128-byte prefix. Check `host->size` before reading
the appended `register_graphics_callback` and `unregister_graphics_callback`
function pointers. Register a version-1 sized `DS2FX_GraphicsCallback` during
`DS2FX_InitializePlugin`; handle zero means unavailable. Callback code must belong
to the initializing module. Only its owner can unregister its handle.

Callbacks receive the current window, immediate context, back buffer, dimensions,
format and frame sequence. They execute serially before the original DXGI Present
on the presentation thread. This is not an engine/game-thread executor. Test
presents, foreign processes, unsupported formats and multisampling are rejected.
There are 16 registration slots; callback order is registration order. Unregister
waits for active callbacks; DLLs stay resident until exit. Do not unload a plugin.

All graphics objects are borrowed for the callback duration. Do not retain a
swap chain, back buffer, or a view/state object referencing the back buffer.
Restore every D3D binding you touch, including state-object contents. Blocking,
throwing, engine calls and raw gameplay writes are outside the contract. A
graphics subscription grants no native HUD suppression; the authenticated HUD
continues using its separate expiring, owner-checked region lease API.

The adapter discovers the OS DXGI Present table through a temporary hidden
hardware swap chain. It authenticates both the function and table to system
DXGI, replaces one pointer atomically and restores it on shutdown only if still
owned. It does not install a proxy DLL or replace another graphics mod. A proxy
or other unsupported adapter may make registration return zero. HDR, D3D12 and
arbitrary third-party injection chains are not certified. The native Scholar
renderer is SDR; the generic adapter does not claim to detect an externally
forced HDR swap-chain color space. Ordering relative to external effects depends
on their own Present chain. The previous pinned ReShade HUD adapter remains as
the compatibility fallback when the public bridge is unavailable.

Core 0.5.42-alpha.1 detects an already-loaded manual `ReShade68.dll` before
creating the probe and returns zero without touching its swap chain. This avoids
an auxiliary hardware device/resize entering the DS2LE renderer during startup.
The HUD uses its authenticated manual-runtime fallback in that configuration;
the standalone Core callback path is unchanged when this backend is absent.

`examples/graphics_callback.cpp` is a complete observation-only plugin example.
Build with the normal DS2FX module template and package under
`DS2FX/Plugins/graphics-example`. It is a developer example, disabled and absent
from the player runtime. Custom modules are supported in solo mode; the existing
official Seamless exact-module review remains in force.

Validation includes real hardware swap-chain callbacks and pixel readback with
no ReShade, ownership rejection, TEST filtering, stale HUD fallback, recovery,
ResizeBuffers and unsubscribe/shutdown. These fixtures do not replace live DS2
gameplay, graphics-stack coexistence or two-player testing.
