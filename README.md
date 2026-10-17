# DS2FX - Dark Souls II Forge Extension

By FROST FX. A native framework for the supported Windows x64 Dark Souls II: Scholar of the First Sin 1.0.3.0 executable.

## Downloads

- [DS2FX Core 0.5.41-alpha.1](https://github.com/FROSTGAMINGVERSE/DS2FX/releases/tag/core-0.5.41-alpha.1): shared loader, Forge menu, public D3D11 graphics callbacks and SDK.
- [DS2 Wearable Lantern 1.0.8](https://www.nexusmods.com/darksouls2/mods/1773): separate optional right-hip light.
- [DS2 Remastered HUD 0.1.28](https://www.nexusmods.com/darksouls2/mods/1774): separate optional player HUD.

The Remastered modlist is optional. Follow the README in each download. Core alone enables neither Lantern nor replacement HUD. Feature preparation reconstructs necessary assets from your own supported game installation; never redistribute the generated cache or LOCAL-INSTALL archives.

## Build native Core and modules

Install Visual Studio 2022 C++ desktop tools, Windows SDK and CMake 3.24+. In an x64 developer prompt:

```bat
cmake -S native -B build -A x64
cmake --build build --config Release --target ds2fx_core ds2fx_bridge_core ds2fx_auto
cmake --build build --config Release
```

The public graphics callback contract is documented in native/docs/GRAPHICS-BRIDGE.md and native/sdk. The separately licensed asset utility's complete corresponding source, dependency source and build instructions are in Asset-Tool-Source.zip and the Core release. No game data or NVIDIA/Lighting Engine/Seamless binaries are included in this source repository.

## Status and licences

Experimental release. Native/GPU fixtures and loader preflights are separate from in-game lifecycle and multiplayer certification. Original-server online play is unsupported. Optional official Seamless is separately downloaded and version gated; remote Lantern replication remains unproven.

FROST FX authored code is source available; no blanket third-party licence is granted. See native/THIRD-PARTY.md, native/licenses, vendor licence files and Asset-Tool-Source.zip for the separately licensed components. Preserve their notices and terms.

## A.I. Use

AI tools assisted with development, documentation and some visual artwork. FROST FX directs and reviews the work; testing and compatibility limits are stated above.

