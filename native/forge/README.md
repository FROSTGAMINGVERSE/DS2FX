# Forge — DS2FX 0.4.1 alpha

Modern Souls 2 0.2.0 adds a Walking Estus control alongside Dedicated Jump. Use Left/Right to select a control and Enter to toggle, or click its button. Both controls apply on the next launch. Walking Estus defaults ON and preserves the exact previously installed gameplay data (Estus movement multiplier 0.5). OFF changes only ItemTypeParam row 450 itemUseMaxSpeed to 0; healing, timings and other items remain unchanged. Two verified OFF variants preserve the independent jump choice. No files from the linked Nexus Walk while drinking Estus mod are included.

Forge is the built-in DS2FX configuration interface. Press **END** while the game has focus. Press END or Escape to close. Select a module with the mouse or Up/Down; Enter changes the selected setting. Tab opens Forge's presentation settings. The game continues while this menu is open.

The panel uses generated forge artwork, privately loaded Cinzel (SIL Open Font License), live text, and a translucent Windows composition layer. It uses the displayed game area, not the internal render resolution. Borderless DS2LE uses its monitor bounds; ordinary windowed mode follows the client area. Placement supports negative monitor coordinates and ultrawide screens. DS2LE's saved `UserEditorSettings.mod_ui_default_font_size` scales the interface relative to its default of 12; Forge additionally offers 70–125% scale and 80–100% opacity. It clamps to the available area. DS2LE changes must be saved through its UI before Forge can read them. Exclusive fullscreen and HDR composition are not yet validated.

Forge owns a separate window and does not patch renderer/input functions or pause gameplay. Keyboard and mouse interaction occurs in that window. Alt-tab/minimize hides it; reopening requires END. Native controller gameplay is not intercepted. Resize/placement checks run while open, while artwork repainting is event-driven. Shutdown joins its UI thread.

## Modern Souls 2

Modern Souls 2 is the user's local DS2FX adaptation of **Fentiman's Balanced DS3 Combat 1.2b, Vanilla ADP variant**, plus the FROST FX dedicated-jump patch. Renaming does not make the original third-party assets FROST FX-owned or grant redistribution permissions.

Source: https://www.nexusmods.com/darksouls2/mods/1235

Only `PlayerCommonParam.jumpTriggerMinSpeed` was changed from 4.4 to 0 for the added jump behavior. The user confirmed jumping without sprint worked before this UI release. Seeker of Fire supplied the behavior reference; no Seeker assets were copied.

Forge's dedicated-jump control selects between two prebuilt, hash-verified regulation files **on the next launch**. It never edits or hot-swaps gameplay data in a live session. The UI distinguishes the current session from the pending preference. Both co-op peers need identical content and the same jump option; DS2FX does not compare peers or certify multiplayer behavior.

Settings live in `%LOCALAPPDATA%/DS2FX/Forge/settings.ini`, outside game saves and Root Builder inputs. Disabling the content mod in MO2 removes its Forge listing. Observer and Player HUD are separate optional modules and are disabled in the configured profiles.

## Adding a content module

Use `Root/Game/DS2FX/Content/<id>/files.tsv`. The first line is:

```
DS2FX-CONTENT-2 your-module-id 1.0.0 shared-content-experimental
```

Subsequent lines are lowercase SHA256, one tab, then a relative path under the module's `assets/` directory. Allowed content is regulation, PARAM, English FMG, model binders, and filter bundles. Root/runtime DLLs and paths outside these directories are rejected. All paths are checked for links and hashes; duplicate asset routes across packs fail instead of silently overwriting. V1 Vanilla ADP packs remain readable for rollback.

Optional `forge.ini` metadata (ASCII, or Windows Unicode UTF-16 for non-ASCII text):

```ini
[Mod]
Name=Your Mod Name
Description=A short explanation of what the mod changes.
Credits=Author and upstream credits.
```

Every validated active content pack and loaded native module is listed. Arbitrary native plugins still follow the existing API 2 and Seamless allowlist; listing a mod does not bypass those rules. The current setting provider is specifically for Modern Souls 2's verified jump variants. Other packs appear with details until they implement an approved configuration provider. The menu is not a downloader and does not replace MO2 installation.

## Remastered title

The independent subtitle texture is installed through `tex_override/h_4244005637484917604h_10781623764489284324.dds`. It targets the installed official Seamless 0.0.1 subtitle texture through DS2LE's texture override system. Seamless's DLL, launcher, locale, and settings are unchanged. The replacement sheet is original text rendering using Cinzel, preserving the texture's two-label geometry. The original DARK SOULS II logo and main-menu controls remain in place. The user verified this replacement in the live Seamless graphics run; this is not an official remaster.

Only the Core/artwork package is distributable here. The local Modern Souls 2 assets must be obtained/imported from their author under applicable permissions. No Seamless or game binaries are included in Core.

## Validation and rollback

Six CTest suites passed, plus an owned-window smoke test covering creation, keyboard navigation, persistence, key-repeat rejection, and shutdown. Eleven auto-launch integration checks passed. The artwork was visually checked using the production renderer at 1600px. The user verified the title and centered Forge with END in the live graphics run. Other display modes and two-player play remain unverified.

For rollback, exit DS2, wait for Root Builder Clear, and close MO2. Restore the Core, previous content mod, and affected profiles from `F:/Modding/DS2FX/rollback/forge-20260924`. Restore the previous profile/mod names together. Do not overwrite an active deployment or delete Root Builder recovery markers.


Live validation: 2026-09-24, game27020. User confirmed both Remastered title and centered Forge with Modern Souls 2 work, including opening/closing with END. Runtime recorded READY modules=0 (Observer/HUD disabled), 379 routed content assets, jump-on regulation SHA256 6f9194cc1e6c439cb5502da17c4ac2f8ffacc5123ee9e6043482179936a3e2e9, Forge open/close, and saved OFF/ON changes ending ON. Official Seamless files hash-verified unchanged. Two-player and exclusive-fullscreen/HDR behavior remain unverified.

Final cleanup: user exited with Alt+F4; Root Builder Sync/Clear completed at 18:34:45 local. No game/loader process or recovery markers remain. Core, Forge artwork and subtitle payloads are absent from the physical game directory after Clear. DedicatedJump remains ON.
Artwork provenance and exact built-in image-generation prompt: artwork-provenance.json. Source assets: forge/assets; editable renderer: src/forge_ui.cpp.
