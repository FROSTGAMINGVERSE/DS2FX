# Changelog

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
