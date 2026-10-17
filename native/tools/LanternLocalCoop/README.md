# Lantern local co-op permissions

This content-only builder preserves the working Lantern item type/animation and
creates an owned non-consuming usage row60425001 from Darksign usage2200.
Only multiplayer and guest permissions become1; only Lantern60425001 is pointed
at the new row. Original Darksign, Estus, events, assets and native adapters are
preserved. Remote Lantern presentation is outside this patch.

Build with .NET8 and the installed SoulsFormats assembly:

```powershell
dotnet build .\LanternLocalCoop.csproj -c Release -p:SoulsFormatsAssembly="<absolute SoulsFormats.dll path>"
dotnet .\bin\Release\net8.0\LanternLocalCoop.dll "<input content pack>" "<matching Scholar definitions>" "<fresh candidate directory>"
```

Inputs are pinned to the five installed October2 Lantern1.0.0 regulation hashes.
Unknown inputs or an owned-row collision are rejected. Output content version
is1.0.1; standalone and all four Modern Souls2 integrations are rebuilt together.
Every output is reread: all existing rows and unrelated binder entries are
compared, then both routing manifests are regenerated. The input pack is never
written. `local-coop.audit.json` records the changes and hashes.

Current local candidate, preflight matrix and guarded stage/rollback script:
`local/candidates/lantern-local-coop-1.0.1-20261002` in the DS2FX workspace.
Run its `Stage-LocalCoop.ps1` only after game exit and successful Root Builder
Clear; the script refuses an active deployment and backs up seven changed files.
Use `-RestoreBackup <recorded backup directory>` for guarded rollback.

These are local game-derived candidates, not a redistributable game archive.
Content checks and launcher preflights do not establish live host/client behavior.
