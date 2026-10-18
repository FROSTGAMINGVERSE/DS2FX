# Early content startup

Core 0.5.46-alpha.1 installs manifest-backed content routes before the supported
official Seamless launcher's direct game child enters the executable. Earlier
injection waited for the title window, allowing startup content to load before
the selected overhaul routes were active. Installing content at executable
entry allowed an older reference character that hung under late routing to load
and reload in the Core + Seeker + official Seamless diagnostic.

## Launch contract

The launcher continues to authenticate the supported game, official launcher,
official DLL and Core seal. It captures only that launcher's direct child and
verifies the primary thread is still in the official `CREATE_SUSPENDED` state,
with its initial x64 entry context matching the executable PE entrypoint.
Already running or differently suspended threads are rejected.

The launcher adds one owned suspension and invokes `DS2FX_ContentBootstrap` in
the authenticated child. This launcher entry point installs content routing
only. Player, HUD, graphics, save and network adapters do not start there.
After successful content setup, the launcher releases exactly its suspension;
the official launcher retains responsibility for its own suspension. Official
Seamless executable and DLL bytes are not modified.

Full `DS2FX_Bootstrap` follows the existing module/window readiness gate. It
re-audits the selected pack roots, effective routes and asset hashes, then
retains the installed hook and selected modules. A selection change is rejected
instead of replacing live startup routes. Targets remain bounded to the game
mount and use relative paths; the original native resolver return is preserved.
This does not change the public module API or enable additional gameplay hooks.

An initializer that exceeds 20 seconds remains owned and awaited until it
finishes or the game exits. An early failure keeps the startup hold while the
live process remains, and the launcher keeps Root Builder's deployment lifetime.
The failure message explains when the suspended game needs closing through
Windows. Full runtime readiness gets its own timeout after early setup succeeds.

## Focused checks

Build the Core, launcher and relevant owned fixtures in an x64 Windows build:

```bat
cmake -S native -B build -A x64
cmake --build build --config Release --target ds2fx_core ds2fx_bridge_core ds2fx_auto ds2fx_seamless ds2fx_content_tests ds2fx_initial_thread_hold_tests ds2fx_bridge_fixture_core ds2fx_bridge_fixture_loader ds2fx_bridge_fixture_host ds2fx_bridge_fixture_official ds2fx_bridge_fixture_vendor
ctest --test-dir build -C Release -R "^(content_routing|initial_thread_hold)$" --output-on-failure
```

`tests/seamless-integration.ps1` stages a small original content file and uses an
owned launcher, host and vendor-identity fixture. It checks the actual Core hook
during host CRT, one hook activation, retained routing after full bootstrap,
module identity rejection and normal exit cleanup. Provide the existing pinned
fixture plugin directory with `-FixturePlugins`; the fixture never disables
production plugin identity gates. Run with `-ContentDelayMs 26000` to exercise
the slow initialization path and verify CRT remains unstarted until content is
ready. The thread-hold fixture covers both release orders, duplicate ownership,
wrong entrypoints and already running children.

`[Diagnostics] ContentRequests=1` enables bounded request capture for diagnosis.
It defaults to off. Request capture does not log or wait on the native resolver
thread, and a native resolver result alone is not proof that an asset was read.

The compiled experimental welcome adapter is retained to match the release
source. `[Presentation] SkipSeamlessWelcome` defaults to `0` and remains disabled
in the repaired reference setup. Its owned classifier/detour fixture does not
establish successful popup suppression in the game.

## Reference verification

On 2026-10-18, the older FROST character loaded in the normal DS2 Remastered
reference profile with Core 0.5.46-alpha.1, Remastered HUD, Wearable Lantern,
Seeker of Fire, official Seamless, DS2LE, OptiScaler and ReShade enabled. A
read-only runtime sample showed native scene `30`, loading `0` and player health
`1659`; graphics telemetry showed HUD frame submissions. These samples support
the visible in-world result and do not establish broader graphics performance.

The user confirmed the custom HUD, Lantern lighting/stowing and a successful
title-screen reload of the same older character, then requested normal game
exit. The launcher recorded exit code `3221225477` (`0xC0000005`). This exit
crash remains under investigation. The bridge subsequently verified all runtime
processes idle and Root Builder deployment clean; that cleanup does not prove
clean process shutdown. The successful reference load/reload and local
interaction remain separate evidence from the owned fixture checks above.

Long-session stability, broader world transitions and remote-peer multiplayer
remain unproved. The routine Seamless welcome popup is unchanged by this update.
