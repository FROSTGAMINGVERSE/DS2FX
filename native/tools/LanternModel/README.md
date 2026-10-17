# LanternModel

Source-only local asset builder for the owned Wayfarer's Lantern geometry.
Requires exact user-owned Scholar inputs and the configured SoulsFormats project.
Generated game archives remain external research artifacts, not redistributable
payloads. Every command refuses an existing output and writes a hash audit.

```powershell
dotnet build .\LanternModel.csproj -c Release
dotnet .\bin\Release\net8.0\LanternModel.dll inspect <body.bnd>
dotnet .\bin\Release\net8.0\LanternModel.dll build <vanilla-coat.bnd> <lantern-mesh.json> <new-body.bnd>
dotnet .\bin\Release\net8.0\LanternModel.dll flame-anchor <vanilla-c0001.bnd> <owned-body.bnd> <new-c0001.bnd>
dotnet .\bin\Release\net8.0\LanternModel.dll pivot-candidate <v7-c0001.bnd> <v7-body.bnd> <new-directory>
dotnet .\bin\Release\net8.0\LanternModel.dll base-lamp-candidate <v9-c0001.bnd> <v9-body.bnd> <new-c0001.bnd>
```

The v9 pivot command appends `DS2FX_LanternPivot` to both skeletons, reweights
only owned mesh palettes, and reparents the owned dummy247 while preserving its
rest world position. Original hierarchy indices stay fixed; one tail-sibling
link changes to include the appended child. Undoing declared edits must restore
the prior FLVER serialization exactly. Every binder companion stays byte-equal.

The v10 base-lamp command places the four owned meshes and twelve textures in
the base character resource, so a fresh content pack needs no coat override.
The shared pivot then directly drives the lamp and dummy. It preserves all
existing base data and tests exact restoration after removing its draw data.
This is a static candidate until base-model visibility, armor swaps, gender,
and physical motion have passed live checks.

See `../../docs/LANTERN-CONTENT-EVIDENCE.md` for exact hashes and native callgraphs.
