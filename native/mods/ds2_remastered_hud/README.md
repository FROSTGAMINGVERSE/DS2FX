# DS2 Remastered HUD 0.1.28

Core 0.5.41-alpha.1 now owns a public graphics callback service. This HUD uses
that service for standalone SDR D3D11 rendering, with no Remastered modlist,
Lighting Engine, OptiScaler or ReShade requirement. The prior exact pinned
manual-ReShade adapter remains a compatibility fallback. A restored D3D11.1
state object now releases its HUD target bindings before returning to the game,
so it cannot retain a swap-chain buffer across ResizeBuffers.

The Nexus package includes a preparation helper that uses Core's self-contained
asset tool to extract and convert original covenant/status icons from the user's
game. Python, a separate .NET runtime and pre-extracted modlist assets are not
needed. Game-derived caches remain local. See docs/GRAPHICS-BRIDGE.md and the
Nexus release's validation record for current verification and remaining live
gameplay checks. The version history below describes earlier dependencies.

## 0.1.27: renderer compatibility repair

Forge's window order is reasserted without periodic demotion and repaint. Artwork publication uses an atomic shared frame so a busy mailbox cannot skip a GPU draw. New or recovered suppression leases warm up for one game frame: native UI has already rendered by Present, so replacement pixels begin on the following frame. Artwork still expires after 200 ms and native leases after 350 ms. Stale-frame diagnostics identify fallback; owned runtime pixel tests cover acquisition and recovery.

The in-game HUD continues drawing when the visible game loses keyboard focus. Minimize, death, loading, native menu opacity, stale ownership and a disabled HUD still govern visibility. The earlier desktop overlay focus-loss blanking rule no longer applies.

Artwork uses universally sampled RGBA8 textures with explicit SRVs; the pixel shader swizzles the existing CPU BGRA bytes. A failed texture/view pair is discarded and retried together, including uploading pixels to newly recreated layers. Resource failure diagnostics retain the operation and HRESULT.

Back-buffer RTVs use explicit format descriptors. When a resource reports an underlying device instead of the context's wrapper address, ownership is validated by creating its view through the context device; foreign-device resources remain rejected. Every early draw rejection has a specific diagnostic.

The missing HUD was traced in DS2 to its 10-bit SDR back buffer (R10G10B10A2_UNORM), which 0.1.26 rejected. The compositor now accepts that format alongside 8-bit RGBA/BGRA, while still rejecting non-SDR color spaces. It also supports ordinary D3D11 contexts when D3D11.1 context-state objects are unavailable: it captures and restores touched bindings and rejects active stream-output/UAV bindings it cannot restore safely. Bounded diagnostics report rejected frames and the first successful GPU submission. Both state paths pass pixel readback on all three formats; the pinned-runtime callback fixture also passes on a 10-bit swap chain. Live DS2 rendering and OBS capture require separate verification.

## 0.1.26: in-game rendering

Visible pixels now composite into the game's D3D11 back buffer through the existing ReShade 6.8 manual bridge, before effects and ReShade's menu. The adapter uses the `present` event, so disabling/empty/loading effects do not stop HUD drawing. This is a game-frame composition stage; it does not submit through DS2's native UI command lists.

Install both the HUD DLL and `Root/Game/reshade-addons/DS2FX-Remastered-HUD.addon64`. Requires DS2FX Core 0.5.36-alpha.1 and this modlist's exact pinned add-on-capable `ReShade68.dll` (6.8.0.6)/manual bridge. The manual bridge skips ordinary add-on discovery, so the HUD arms a bootstrap on its public update API and loads the adapter on the bridge's serialized render thread before event dispatch. The runtime and adapter hashes and callback caller are authenticated. Missing/mismatched files, unsupported graphics format/API, or stale CPU frames leave/restore native drawing. Supported path: SDR D3D11 on Windows; D3D11.1 context-state support is optional. A changed ReShade runtime needs a reviewed compatibility update.

The Forge IPC window is hidden, nonlayered and never topmost. Layout editing and preferences remain available. The cropped GDI+ artwork worker publishes separate covenant, bar and status layers. Native leases are granted on the rendering callback after resource validation, immediately before submitting only granted regions; submission failure revokes them. D3D11 state is isolated/restored and back-buffer references end before returning to Present/ResizeBuffers. CPU frames older than 200 ms are rejected; stalled leases expire after 350 ms.

HUD pixels now enter the game buffer that OBS Game Capture can capture. Actual recording and ReShade menu layering in DS2 require live verification; owned fixtures are distinct evidence. The reference debut is `F:/Modding/Dark Souls 2`; the published installer catalog is not changed by this local update.

## Preserved appearance and behavior

The candidate adds owned grimdark Hollow and Human frames for health, stamina,
the original covenant artwork, and a strip reusing original active status
effect icons. Native souls, quick slots, enemy/boss UI and prompts stay native.
Custom souls and quick slots remain disabled by default.

Visual pass 0.1.9 uses the approved cold fractured iron Hollow skin and intact
gunmetal/ember Human skin. Transparent original frame atlases retain the concept
materials; no decorative heraldry is added. Health is 332x14 reference pixels
(previously 420x18); stamina is 244x10 (previously 315x13). Effect sockets are
42px with original visible artwork fitted to a centered 26px square, preserving aspect ratio,
native tint, order and independent opacity. Transparent icon padding is removed
at draw time. Both skins share identical destination geometry. Pulse defaults
to 0. All bitmap resources release before the drawing backend shuts down.

Visual pass 0.1.10 adds a faint steady orange ember halo around the Human
covenant and bar frames. Interiors remain clear, and Hollow retains its cold finish.

Requires the matching packaged Core 0.5.14-alpha.1 and unified launcher.
ABI 2 and module contract 0.3.0-alpha.1 retain their existing prefixes;
icon observation, region leases and read-only character form are appended capabilities.

## Hollow and Human selection

`SkinMode=0` follows authenticated canonical PlayerCtrl+490 -> PlayerParam+1ac.
The supported executable's native setter clamps this byte to 0..32 and its
humanity reset sets zero. Level 0 selects Human; levels 1..32 select Hollow.
The byte and entire owner chain are read twice. No engine calls or writes.
Transient unavailable form retains the last known skin for at most 1500ms in
the same character epoch; unknown/new/stale character form defaults to Hollow.
Cosmetic state loss never revokes healthy HUD regions. `SkinMode=1/2` offers
Hollow/Human preview overrides without changing the actual character state.

Both immutable skin PNGs must validate before the overlay starts. Missing or
changed skin assets refuse initialization, leaving native HUD drawing active.

## Replacement behavior

The Core reads authenticated, game-owned UI components. Covenant identity
and active status icons are observed in native order, with opacity and tint.
An empty covenant has a socket. Status icons use their own visibility source.
Unknown, mismatched or reallocated components pass native draws through.
No game actions, save writes or network changes are involved.

Bars, covenant and effects have independent expiring render leases. Native
drawing is suppressed only after successful replacement presentation. A missing
covenant asset preserves the native covenant; a missing active effect asset
preserves the entire native effect strip. Renderer failure, stale observations
or lost ownership restore native drawing. Leave the game's HUD enabled so its
visibility controller can govern menu fades.

The region-mask receipt preserves valid bars/covenant when the effects region
falls back. Optional identity changes have separate draw serials, so an effect
change cannot expose an in-flight native bar. Icon keys are read twice from
authenticated original providers; temporary native GPU texture availability
does not invalidate the independently validated local artwork.

## Original artwork: local cache only

The ZIP excludes game-derived artwork. Without a valid local cache, bars
still work and covenant/status presentation remains native. Build the cache
from your supported Scholar installation outside the game directory:

```powershell
dotnet build native/tools/HudAssets -c Release -p:SoulsFormatsAssembly="<SoulsFormats.dll>" -p:BouncyCastleAssembly="<BouncyCastle.Crypto.dll>"
& native/tools/HudAssets/bin/Release/net8.0/HudAssets.exe "<Scholar Game folder>" "<ScholarDictionary.txt>" "<fresh external DDS folder>"
python native/scripts/convert-hud-icons.py "<DDS folder>" "<fresh PNG folder>"
```

Copy the generated PNG folder as `Assets` beside the HUD `plugin.dll` in the
unpacked MO2 mod. Keep `icons.tsv`; hashes and decoded size are validated
before use. Do not redistribute this local cache. The packaging script's
`-LocalAssets` option adds the cache only to the local unpacked mod, after
creating the artwork-free ZIP.

## Configuration

Edit `Root/Game/DS2FX/Plugins/ds2-remastered-hud/hud.ini` and restart:

| Setting | Range | Meaning |
| --- | --- | --- |
| Enabled | 0â€“1 | Enable replacement |
| ScalePercent | 50â€“200 | HUD size |
| Margin | 0â€“128 | Edge spacing in reference pixels |
| Opacity | 32â€“255 | Configured presentation opacity |
| Pulse | 0â€“1 | Decorative animation; default 0 |
| DiagnosticIds | 0â€“1 | Research identifiers |
| ShowQuickSlots | 0â€“1 | Development custom slots; default 0 |
| ShowSouls | 0â€“1 | Development custom souls; default 0 |
| SkinMode | 0â€“2 | 0 native form; 1 Hollow preview; 2 Human preview |

The hidden Forge IPC window never takes focus; HUD pixels belong to the game frame.
Fullscreen capture still requires validation with the active graphics stack. Observations update at 100 ms;
leases expire after 350 ms. Loading uses a 1.2-second stable-playable delay.

## Verification and rollback

The previous 0.1.7 candidate was confirmed by the user to have no flicker,
correct menu hide/return and successful loading transitions. The 0.1.9 skins,
read-only form, icon ownership and region failures have fixtures; consult the current
verification ledger for actual live results. Build success and a pinned
Seamless allowlist do not certify multiplayer.

Travel, death, character changes, performance and a two-player official
Seamless session require distinct live evidence. Preserve the official
Seamless launcher, DLL and settings.

Keep the prior MO2 candidates, especially stable 0.1.7, for rollback. Exit the game and
wait for successful Root Builder Clear before changing enabled mods, loader
paths or deployed configuration. Enable exactly one HUD candidate and point
the DS2FX executable entry at its `Tools/ds2fx_loader.exe`. Root Builder is the
sole deployment owner.

0.1.11: Human ember tongues remain within roughly 3 reference pixels of the frame. Both fills pulse gently by default (Pulse=1). Lost health holds 260ms then eases with a 250ms time constant; spent stamina holds 160ms then eases with a 180ms time constant. Muted red/grey-green trails reset on hidden scenes, character and maximum changes. Presentation timer is 66ms; runtime cadence/performance needs live verification.

0.1.12: persistent cropped premultiplied DIB, no per-frame full-screen copy; physical-size cached sprites/ember frames; 60fps presentation default with independently observed 100ms gameplay snapshots. Recovery interpolates between samples; damage remains immediate. Ember masks follow exterior atlas alpha, excluding internal sockets. Forge settings under ds2-remastered-hud apply live within one second. HUD ScalePercent, Margin, Opacity, Pulse, PulseStrength, EmberStrength, AnimationFPS, StatusIconSize, HealthHoldMs, HealthDrainMs, StaminaHoldMs, StaminaDrainMs, SkinMode are bounded. Lantern/Core changes require restart. Forge preserves existing Modern Souls 2 controls; no new unproven gameplay patches. Live render telemetry reports surface dimensions and average/max render plus Windows presentation time every 15 seconds; it is not game frame-time certification.

0.1.13: request1ms timer precision only while visible to prevent Windows timer coalescing from reducing HUD cadence. Hide/shutdown releases the timer request; hidden HUD polls at100ms. Telemetry reports actual presentation cadence. Native value sampling still defaults100ms.

## HUD layout editor (0.1.15)
Open Forge with END, select DS2 Remastered HUD, and click Editor (or press E). Drag the covenant, health, stamina or status-effects group. Drag its lower-right handle to resize while preserving artwork proportions. Switch Snap to grid / Free move; use grid -/+ for spacing. Save persists the layout; Cancel or ESC restores the previous layout. Reset layout restores default positions in the preview; Save commits it. TAB selects a group, arrow keys move it, ENTER saves.
Positions are normalized against the physical game client; artwork scale follows the smaller reference-screen axis. Layout is clamped on screen when resolution or aspect ratio changes. Status effects move and resize as one group, retaining native ordering and individual icon opacity. Grid and handles exist only in editor mode; the game continues running. Native souls and quick slots remain unchanged. The replacement HUD remains visible while the owned Forge window has focus, with healthy selective suppression leases required.

### Refinement (0.1.19)

The grid defaults to 100 columns across the screen. Snap magnetically catches nearby lines; hold Shift to temporarily move freely. StatBarLengths (default 1) reads the installed level-1/99 physical-stat bounds and MenuGauge lengths, using the supported native gauge interpolation. Native screen fractions use one compact 800px reference conversion (half of the 1600px canvas). Both bars share one length scale, independently of their source atlas proportions and saved thickness. In automatic mode, the corner adjusts thickness; manual length handles appear when automatic stat lengths are disabled or unavailable. Saved positions and sizes are retained. Missing gauge data retains authored geometry without guessing a conversion from engine stamina to the menu's stamina number.

Frames render directly from the original atlas at their physical screen size. Bar end caps keep their proportions as the middle rail changes length. Human ember contours follow this final frame alpha within three physical pixels, including after resize. ShadowStrength (default 40, range 0â€“100) controls a cached Gaussian fade drawn behind the embers. Fills extend underneath frame edges to cover socket seams. Decorations are cached until frame dimensions or opacity change.

The supported game's and reviewed local graphics libraries' SetCursorPos and ClipCursor imports are guarded only while the visible owned Forge editor or its game has foreground focus. Outside editing, arguments and return values pass through unchanged. Executable and graphics hashes are authenticated; pre-existing import hooks are skipped. This is local presentation code. Gameplay dragging and native gauge sizing still require live confirmation for this candidate.


### Slim motion pass
Both rails use the same 16px reference thickness and frame profile. The health Size setting supplies their common thickness scale; resizing either bar corner adjusts both together. Stat-derived lengths remain unchanged. Fills progress from dark left to bright right. Stamina receives a clipped energy sweep every 1.8 seconds. Health uses a continuous double heartbeat, 48 BPM at full health up to 150 BPM near zero, with strength rising as health drops. Pulse/PulseStrength control these effects; damage and stamina trails remain independent.


### Presets and themes (0.1.20)
Forge -> DS2 Remastered HUD -> Presets / themes (P) offers six saved UI slots.
Select a slot, Save current to slot (S), or Load selected slot (Enter).
Saving an occupied slot replaces it. Presets include all HUD menu options,
normalized positions, shared rail thickness, manual lengths, status sizing and editor grid.
They persist under %LOCALAPPDATA%/DS2FX/Forge/HUD-presets/preset-N.ini.
A complete bounded versioned preset is validated before settings are atomically replaced;
empty or malformed slots leave the current UI intact. Other mods and Forge panel settings are preserved.
Automatic follows native Hollow/Human state. Ashen locks Hollow iron, Ember locks the
Human frame, and Quiet ember locks the Human frame with subdued embers/pulses.
Theme buttons preserve positions, sizing and native stat lengths. Custom adjustments can be saved to any slot.
Human exterior embers use spatially seeded smooth irregular noise and blend cached frames
at physical asset resolution. Shadow stays behind the silhouette/embers. No per-frame random generation,
new textures, game memory writes or network-visible changes are introduced.


### Visibility lifecycle (0.1.21)
Presets use separate slot/status rows; Forge text does not wrap outside its controls.
Focus loss, minimization, death (HP0), loading and scene-settle states now hide the
replacement window and successfully refresh a transparent1x1surface every100ms.
That healthy blank presentation renews the same350ms suppression lease for the
current authenticated native UI owner; it does not expose the original bars.
Core UI ownership is independent of gameplay scene visibility. Snapshot age,
native-chain identity, character generation, renderer errors and lease expiry
still restore native drawing. Unknown or changed UI objects are never hidden.
Disabled HUD restores native drawing. No permanent suppression flag or engine
visibility writes are used. Death/loading and focus transitions require live
verification in addition to owned tests.
