# DS2 Visual Overhaul

An independent DS2FX environment content pack, code name **DS2 Visual Overhaul**, version 0.1.3 local prototype. Requires Core 0.5.37-alpha.1 and the supported Scholar executable. Source input is the user's Seeker of Fire Seamless 2.1.5 archive; its exact SHA256 is pinned for building.

The author page requires permission for modification/asset reuse and has distribution restrictions. This local prototype is not a public or Patreon release. Resolve permission and any other contributor asset rights before distribution. An original alternative can use this vanilla-based composition tool with newly authored placements and assets. Author reference: https://www.nexusmods.com/darksouls2/mods/978?tab=description

## Tool commands

```text
VisualOverhaul audit <Seeker ZIP> <Scholar Game directory> <fresh output>
VisualOverhaul resources <Seeker ZIP> <Scholar Game directory> <fresh output>
VisualOverhaul build <Seeker ZIP> <Scholar Game directory> <fresh output>
VisualOverhaul classify <Seeker ZIP> <Scholar Game directory> <fresh output>
VisualOverhaul billboards <Seeker ZIP> <Scholar Game directory> <fresh output>
VisualOverhaul types <effective regulation> <Scholar Game directory> <fresh output>
```

Build the C# tool against the existing reviewed SoulsFormats and BouncyCastle assemblies, using `-p:SoulsFormatsAssembly=... -p:BouncyCastleAssembly=...`. It needs the .NET 8 runtime. Keep all inputs, temporary data and output outside the game directory; game archives are read-only. Do not unpack or execute Seeker's loader.

Audit hashes every archive file, compares all 28 MSBs with vanilla, inventories every placement category, and verifies semantic vanilla roundtrip. Resources inspects Seeker object binder entries and vanilla map geometry headers. Build uses the canonical vanilla MSB as its starting point.

Classify is a read-only evidence export. It compares source instance rows with vanilla, records available original type rows, parses binder geometry and render configurations, and retains unknown dependencies. It exports sampled mesh point clouds for shape review. Candidate labels are not gameplay certification: source scripts, original global type contracts and live behavior still require verification. It does not relax the builder policy or alter deployed content. Definitions are loaded from the source tree's vendor/Paramdex/DS2S directory and hashed in the report.

The full-mod classifier also reads the original loose regulation in memory when its type PARAM is absent from GameData, records its hash, audits modified existing Objects and compares the six LQ/GI plus three changed main-map container pairs. Missing original container paths remain explicit. ACB Havok references count as physical dependencies even without a packaged HKX file; unresolved General/PWV references remain ineligible. Giant destruction types are distinct from non-destructive types. Matching PARAM data and mesh-only payloads remain evidence, not proof of safe script identities, walkable-surface alignment or runtime performance.

## Composition policy

- Preserve original events, regions, poses, collision, navigation, map connections, existing object gameplay metadata and existing geometry transforms. The only existing-object visual exception is the exact Heide backdrop described below.
- Add static render pieces only when their mesh exists in the original map container.
- Union visibility masks on existing render pieces; retain original visibility bits and never set every group globally.
- Add only zero-instance objects backed by mesh/texture-only binders. Reject collision, animation/configuration, effect and unknown dependencies. A nonzero `MapObjectInstanceParamID` is excluded.
- Version 0.1.1 also admits exactly 14 named additions through `SafeScenery.cs`. Require exact original non-destructive instance/type rows, zero unknown object flag, no pose, new hash-pinned mesh/texture-only binders and an original inert template. Copy only model, transforms and visibility from the source onto that template; retain every unknown non-visual template field. No source PARAM or event data is imported. A literal original-script name check supplements these contracts but does not prove the absence of computed references.
- Never replace an existing global object model with a changed Seeker binder. New model names are packaged independently.
- Version 0.1.2 admits the exact Majula Heide landmark change in `HeideBackdrop.cs`: preserve object o00_9150_0000 and its original inert metadata, while applying the source billboard model and visual placement. Require the hash-pinned new o00_9155 binder, a static four-vertex alpha billboard, one texture, the byte-identical original empty material-timing table, and original render configuration differing only in its mesh path. No collision/motion dependency or gameplay-data change is admitted. This is a distant textured backdrop, not navigable tower geometry.
- Version 0.1.3 appends exactly ten named zero-instance billboards through `SafeBillboards.cs`, using five reviewed models (four newly packaged). Match every non-visual source field to an original zero-instance, zero-flag, unposed template after excluding the six source area flags. Clone that original metadata and copy only source model/transforms/visibility. Require the same original empty timing and render-configuration contract as the Heide card, pinned binders, new global model names and no literal original-script name. No source area flag or gameplay parameter is imported; there is no blanket billboard admission.
- Give additions unique on-disk names; SoulsFormats display suffixes such as ` {2}` are not persistent identities.
- Re-read every output and require gameplay-section equality and candidate roundtrip equality before emitting its manifest entry.

Version 0.1.2 contains 26 changed maps, 1,933 static pieces, 360 decoration objects, 158 visibility extensions and 51 new object binders (77 assets). The 14 additions from 0.1.1 affect Majula (4), Heide (7), No-man's Wharf (1), Drangleic Castle (1) and Old Iron King DLC (1). Existing prototype placement names/order are preserved. Version 0.1.2 changes one Majula billboard and adds its 2.8 MB binder; all other 0.1.1 assets remain identical. The missing Gutter mesh and other candidates remain excluded pending their separate reviews. It does not include regulation, events, enemy layouts, navigation files, modified physical collision, lighting filters, sounds, interface, Mod Engine or Seamless settings.

Version 0.1.3 retains those assets and placements, adds ten billboard placements across seven maps and four binders (81 total assets). Only those seven map payloads change; Majula's 0.1.2 backdrop remains identical. The all-supplied-binder audit found eight four-vertex unlit models; three have no source-map placements and are not installed. Their images include a snowbound castle skyline, coastal village panorama, cliffside fortress and mountains. These are unlit placeholders with fixed texture lighting; 3D remodeling is deferred. Per-viewpoint appearance, texture coexistence under the renderer, travel, frame times and multiplayer remain separate checks.

## Loading and validation

Install the generated `DS2FX - Visual Overhaul` folder as a separate MO2 mod through Root Builder, alongside the existing Core/HUD/Modern Souls/Lantern stack. The actual runtime content resolver consumes its files.tsv; no separate injector or per-frame placement scan is needed. Restart the game when changing enabled content. Duplicate DS2FX asset keys remain fatal; map-overhaul conflicts need an explicit merge rather than priority.

Map layouts are classified as shared experimental content. Use identical pack versions with peers; local data preservation is not multiplayer certification. Loading both Core build modes, two-player join/leave/travel, broad area coverage and measured frame times are distinct checks. The source archive being Seamless-compatible does not automatically certify this derived pack.
