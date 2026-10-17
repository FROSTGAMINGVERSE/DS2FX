# DS2 Remastered HUD research record

Supported game: `DarkSoulsII.exe` SHA-256
`0045931b8914504531b7864a9488d396dc50cbaf524964016e1d69c3d1173131`.
BaseA resolves through the existing DS2FX executable signature; prior runtime
logs place its slot at RVA `0x16148f0`. The new Core observer does not write to
the game or call its functions.

## Source leads (not yet live validated for this HUD)

- [DS2S-META](https://github.com/Nordgaren/DS2S-META) Scholar offsets:
  BaseA to PlayerCtrl `+0xd0`, PlayerCtrl to PlayerParam `+0x490`, PlayerParam
  souls `+0xec`, covenant byte `+0x1ad`.
- [Bob Edition Scholar table](https://github.com/boblord14/Dark-Souls-2-SotFS-CT-Bob-Edition)
  equipment chain: PlayerCtrl `+0x378`; active left/right hand indices
  `+0x5a4/+0x5a8`; left hand IDs `+0x9c/+0xc4/+0xec`, right hand IDs
  `+0xb0/+0xd8/+0x100`.
- Equipped quick items appear at equipment `+0x4f8..+0x51c`. Attuned spells
  appear at PlayerCtrl `+0xac4` onward. These are *slot lists*, not proof of
  the currently selected slot. Item count and remaining spell uses remain
  unmapped. The original icon assets also remain unmapped.

Core's `hud_observation.h` checks the canonical player chain before and after
reading. Souls, covenant, and each hand are independent validity bits. An
unavailable field must not be shown as zero. Quick item and spell validity
bits remain unset until the selected indices are verified in a live character.

## Read-only selection probe

`ds2fx_hud_probe.exe` is a separate **offline-only research tool**. It requires
the supported game hash and loaded offline `ds2fx_core.dll`; it refuses the
Seamless bridge. It opens the process with `PROCESS_VM_READ` and
`PROCESS_QUERY_LIMITED_INFORMATION`, and writes only a new CSV in DS2FX's
local research folder. It does not modify the game, saves, MO2, or controls.

When a character is in a playable 3D scene, start the probe with a fresh path:

```powershell
& 'F:\Game Dev\01_Projects\Tools\DS2FX\local\native-build\Release\ds2fx_hud_probe.exe' `
  'F:\Game Dev\01_Projects\Tools\DS2FX\local\research\hud-selection-probe\cycle-YYYYMMDD-HHMMSS.csv'
```

For its 90-second run, cycle exactly one direction for several seconds, pause,
then do the next direction. Suggested order: Down item, Up spell, Left hand,
Right hand. Log each direction's approximate elapsed time. The CSV records
changed 32-bit words in narrow candidate ranges as `elapsed_ms,region,offset,
old_dec,new_dec`. A candidate offset is not accepted merely because it changed:
repeat with multiple equipped items and characters, then confirm its value
matches the original HUD and stays valid through death, travel, and reload.

## Vanilla HUD suppression gate

Do not hide the original HUD yet. A full overlay must first present the
selected item and spell, correct count/uses, and the original icons. Selective
suppression must preserve menus, prompts, and other UI. An in-game auto-hide
option or broad shader toggle is not proof that this finer control exists.

### Active texture override atlas inspection

The enabled Smooth UI mod supplies `ui_health_bars.dds` (2048x1024),
`ui_background_weaponslots.dds` (4096x2048), `ui_background_elements.dds`
(4096x1024), and `ui_soulcount.dds` (1024x1024). A visual contact sheet is in
`local/research/hud-atlases/contact.png`. The first atlas contains bar fills;
the latter atlases contain slot/background and souls presentation artwork.
These are shared texture sheets, so replacing an entire DDS with transparency
would also remove unrelated sprites. The original selected item/spell art was
not identified on these four sheets. Resolve atlas sprite regions or a verified
render-stage HUD classification before suppressing the original elements.

## 2026-09-30 live preview observation

The user supplied a 3840x2160 gameplay screenshot from the `DS2 Remastered`
profile. Fresh `runtime-4512.log` reported Core `0.5.7-alpha.1`, authenticated
Seamless bridge, `ds2-remastered-hud 0.1.0`, Lantern `0.4.0`, and `READY
modules=2`. Process modules included the official `ds2sc.dll` and
`ds2fx_bridge_core.dll`. The overlay rendered souls `1054`, covenant ID and
hand IDs; the base HUD and Lantern HUD remained visible. No item/spell icon
or selection proof was obtained.

At 150% Windows display scaling, the game's physical client measured
3840x2160 while a DPI-virtualized window query returned 2560x1440. The
0.1.0 overlay covered only the latter extent, placing its souls plate inside
the screen. Preview 0.1.1 maps the game client relative to DWM's physical
frame bounds. Synthetic 150% geometry and plugin tests pass; a new live
screenshot is still needed to prove presentation and positioning.

## 2026-09-30 loading gate evidence

Two user-driven bonfire travels were recorded with a separate read-only
process sampler. The verified executable remained loaded through official
Seamless. The trace is `local/research/hud-loading-state/travel-20260930-2351.csv`.
Before travel: `GameState=30`, `LoadingState=0`. On both travels the loading
flag became `1` and the state passed through `31`, `19`, and `28`, then
returned to `30/0` as the world loaded. The first loading period was roughly
2.6 seconds; the second roughly 3.2 seconds. This validates the DS2S-META
Scholar offsets `BaseA+0x24ac` and `BaseA+0x80 -> +0x8 -> +0xbb4` for these
two transitions. It does not prove exact fade timing, menu state, cutscenes,
death, or every travel path. Preview 0.1.2 hides unless the read-only state
pair is stable and `30/0`, then waits 1.2 seconds before drawing. Unknown or
unreadable state hides the overlay.

## 2026-10-01 user verification and next blocker

The user confirmed preview 0.1.2 disappears for loading screens. Their new
equipment-menu screenshot still shows the custom D-pad and souls plate over
the menu. The gameplay crop shows the bottom item diamond clipped.

The development layout now uses radius48/step64 instead of radius38/step80,
and computes its bottom anchor using step+radius+halo. This enlarges each
frame and reduces spacing while preserving all four input directions. Bounds
and rendering tests pass; in-game placement is not yet verified.

Menu visibility must follow the actual native HUD visibility, potentially
independently for bars, quickmenu and souls. A blanket menu-key toggle is not
an adequate detector. The source lead `isMenuIdle` at PlayerCtrl+B8 -> +4CB
is an animation-state candidate only, not a proved native UI visibility flag.
`native/scripts/probe-hud-menu.ps1` prepares a read-only sample of the scene
leaf, narrow BaseA state range and this candidate. No candidate is published
through the API until live menu transitions and matching native UI behavior
are verified. No menu suppression fix is claimed yet.

## 2026-10-01 native opacity mapping (preview 0.1.4 candidate)

SilkySouls2's MIT source Offsets.cs identifies the frontend branch through
BaseA+22e0. Static/runtime exact-image RTTI verified these fresh pointers:
BaseA+22e0 -> +10 FeOperatorFrontend -> +10 FeSceneHpGuage -> +8
FexLayoutScene -> +28 FexMenuWindow -> +30 FeComponentScene -> +38
FeComponentSprite -> +38 FeComponentObject. The final component's byte+98
is native rendering alpha: RVA b6a08f reads the byte, divides by 255, and
calls b53110 to apply it; b6af7e..b6afc9 writes the animated alpha.
No native calls or memory writes were performed. Current source origin:
https://github.com/borgCode/SilkySouls2/blob/master/SilkySouls2/Memory/Offsets.cs

Read-only snapshots under local/research/hud-menu-state: normal-01,
equipment-01, normal-02, normal-03, inventory-01, settings-01, normal-04.
Equipment, Inventory and Video Settings show alpha 0; gameplay shows 255;
Equipment and Settings close restored 255. The complete pointer chain was
authenticated in the alpha sampler. The timed trace missed the user's close
and contains only alpha 0: it does not prove the intermediate fade samples.
Native FeOperatorFrontend+8 and animation+340 also track hiding, but the
replacement consumes the renderer alpha rather than inferring menu state.

The new separately sized API tail query preserves existing HudState and ABI
prefixes. All links and concrete vtables are reread before publishing. This
is client-local presentation in both Core builds; peer certification remains
pending. FeScenePanelLEquip's resource was persistently disabled in this run,
so it is not used as a quickmenu visibility authority. All replacement
regions follow native health, as requested. Original HUD suppression and
artwork/selected quick-item mapping remain separate unfinished work.

## October 1: narrowed replacement scope

User now requests only health, stamina and covenant/sigil replacement. Native souls and quick slots are retained. This supersedes the full-overlay suppression prerequisite above: no item/spell mapping is required for this narrower replacement. ShowSouls/ShowQuickSlots default off in source, renderer tests pass, complete 0.1.5 source/ZIP archived. Native health scene includes additional component groups; exact bar/frame/covenant draw classification and truthful covenant icon handling require a live read-only tree snapshot. Blanket health-root suppression remains disabled.

