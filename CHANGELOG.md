# Changelog

## 2026-10-18 - Core 0.5.46-alpha.1

- Install manifest-audited content routes before the verified official Seamless child enters its executable. The unified launcher owns one additional suspension of the official launcher's already suspended primary thread, initializes content only, then releases exactly that suspension. Official Seamless files remain unchanged.
- Retain the same hook and re-audit the effective content selection during full runtime initialization. Reject changed roots, routes or hashes; use bounded game-relative targets and preserve the original resolver's result. Optional bounded request diagnostics remain off by default.
- Keep the startup hold and Root Builder lifetime while remote initialization is pending or fails. A slow initializer crossing 20 seconds is awaited instead of allowing partial startup; full runtime readiness receives a fresh timeout after early content completes.
- Add owned fixtures for content routing during host CRT and after full bootstrap, selection changes, both suspension release orders and delayed initialization. These checks do not use game assets or vendor binaries.
- Public source builds passed for both Core modes, unified/Seamless launchers and bridge fixtures on Windows x64. The content-routing, initial-thread-hold and welcome-adapter fixtures passed (3/3); the last remains synthetic evidence only.
- Retain the compiled experimental welcome adapter for source/release reproducibility. It defaults to off, is not active in the reference repair, and has no successful live popup-suppression validation.
- Live reference evidence: the older FROST character loaded with Core, Remastered HUD, Wearable Lantern, Seeker of Fire, official Seamless, DS2LE, OptiScaler and ReShade enabled. The user confirmed the custom HUD, Lantern lighting/stowing and title-screen reload of the same older character. After the game ended, the bridge verified idle processes and clean Root Builder deployment.
- The launcher recorded exit code `3221225477` (`0xC0000005`) after the user requested normal exit. This exit crash remains under investigation; successful cleanup does not establish clean shutdown. The load/reload and local Lantern evidence remain valid, while long-session stability, broader world transitions and remote-peer multiplayer remain unproved. No general freeze/stutter or welcome-popup fix is claimed.

## 2026-10-17 - Public Core 0.5.41-alpha.1

- Publish the existing standalone Core and SDK through GitHub; Lantern 1.0.8 and HUD 0.1.28 remain on Nexus.
- Include native framework/module source and the separately licensed managed asset-tool corresponding source.
- Exclude local game-derived caches, full game assets, private runtimes, logs and backups.
- Existing release evidence: 53 native/GPU tests and package/loader preflights. Fresh game lifecycle and two-player verification remain outstanding.
# Core 0.5.42-alpha.1 - 2026-10-18

- Skip the auxiliary hardware swap-chain probe when an existing manual ReShade68 backend is loaded. Return unavailable without side effects; the HUD uses its authenticated manual adapter. Standalone public graphics callbacks remain available without ReShade.
- Include an identity-only backend fixture that checks registration refusal and subsequent standalone callback operation. Both offline and Seamless Core include the change.
- Three focused graphics/HUD fixtures passed in the development tree, and one reference character/HUD load was verified. Intermittent in-world stalls, broader lifecycle checks and multiplayer remain unverified.
- Companion Lantern 1.0.9 restores its required bonfire-delivery metadata. The modlist installer handles local gameplay-overhaul compatibility separately; no third-party assets are added to this repository.
