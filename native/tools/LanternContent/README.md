# LanternContent

Local content builder for the DS2FX Wayfarer's Lantern prototype. Source-only;
requires the user's own Scholar/Modern Souls2 content and SoulsFormats library.
Generated full game/mod archives are local research fixtures, not distributable
assets. See `../../docs/LANTERN-CONTENT-EVIDENCE.md` for hashes and exact evidence.

`lantern-flame-audio <reviewed 1.0.3 SFX binder> <new external scratch binder> <gain>`
changes only owned flame6042's sound75 gain. The 1.0.4 candidate uses `0.15`.
The tool verifies exact source/effect hashes, sound157/repeat layout, semantic
round-trip and all 1778 unrelated entries. It writes an audit; it does not deploy.

Build:

```powershell
dotnet build .\LanternContent.csproj -c Release `
  '-p:SoulsFormatsAssembly=F:\Modding\Dark Souls 2\research\SoulsInspector\bin\Release\net8.0\SoulsFormats.dll'
```

Stage a new complete pack (paths below are the verified local workspace):

```powershell
.\Stage-LanternContent.ps1 `
  -SourcePack 'F:\Modding\Dark Souls 2\mods\Modern Souls 2\Root\Game\DS2FX\Content\modern-souls-2' `
  -StagingDirectory 'F:\Modding\Dark Souls 2\research\lantern-20260925\new-test-stage' `
  -TestStart `
  -OilIconTpf 'F:\Modding\Dark Souls 2\research\lantern-20260925\oil-icon-v1\ic_0060425000.tpf' `
  -LanternBodyBnd 'F:\Modding\Dark Souls 2\research\lantern-20260925\model-fixture\bd_1030_m.bnd'
```

The destination must not already exist. The complete pack is the `modern-souls-2`
child. `_build` and `staging.audit.json` are research evidence, not game payload.
All four jump/Estus variants are rebuilt;375 original files are hash-preserved.

`-TestStart` gives only new Swordsmen ten oil in the previously empty quickslot2.
Swordsman already starts in the Wanderer Coat; the current model is male-only.
This switch is explicitly a test fixture, not a shipping gameplay change.

`-TestPrecreation` is a separate QA fixture for a **fresh** new game before the
witches' character creator. It changes only PlayerStatusParam row10 `bodyArmor`
from21100101 to21030101 and empty quickslot2 to ten oil. The row already specifies
male gender0; all other stats/gear/fields are verified unchanged. It does not
implicitly enable `-TestStart` or retrofit an existing character save. Each
variant audit records these exact source/target values and the QA-only role.

Default loot policy appends oil to36 guaranteed torch lots and leaves7 enemy
lotteries unchanged. If a validated native successful-award hook supplies oil,
add `-OilAwardHook`: this leaves **all** lot tables untouched to prevent double oil.
The hook requirement is marked in every audit. This flag does not enable a hook.

`Convert-OilIcon.ps1` uses Windows PowerShell/System.Drawing for alpha-preserving
64x128 icon import, and a hash-pinned official Microsoft DirectXTex texconv for
BC3 compression. `LanternContent icon` wraps that DDS using checked DS2 PC TPF
metadata. Texconv and the game reference TPF are intentionally not bundled.

The current effect is a300-second native light top-up using consumable60425000.
This builder alone does not implement off/on, canonical remaining-oil observation,
partially burned save persistence, an interaction hold, or all-torch runtime pairing.
The v4 light/SFX position and model need a visible in-game test.
# Orange-flame candidate (v6)

`--torch-flame` replaces the oil effect's borrowed Cast Light5186 with verified
native TorchCtrlParam1 SFX230 and binds it to owned base-character dummy247.
PlayerLight51 becomes amber RGB255/158/64; intensity134 and radius6 are unchanged.
This is a visual-test candidate: the torch particle itself retains its native
size, and flame containment and world-light hip positioning are not certified.

Build the anchor using `LanternModel flame-anchor <exact c0001.bnd> <new output>`.
`Stage-LanternContent.ps1 -TorchFlame -CharacterAnchorBnd <output>` requires its
audit plus the matching `-LanternBodyBnd`; it refuses an existing c0001 override
that would need a merge. The anchor is at the model's wick, world bind position
(.27,.822,-.055), on Pelvis3. It adds one unused dummy, preserves the53 original
dummies and all other character data, and supplies ordinary pelvis motion only.
No stock flame, stock light row, existing dummy, cloth, or animation is edited.

## Compact flame and base-character candidates

`LanternContent compact-flame-candidate <exact sfx9999 archive> <new output>`
clones torch230 as owned6042 and scales eight extent-curve values. All1778
original archive entries stay byte-identical. Supply its audited archive with
`-CompactFlameCandidateBnd` and the usual `-TorchFlame`/anchor. The v8 candidate
keeps original particle motion: live evidence shows smaller orange flame but
does not establish containment within the cage.

The later compact-emitter candidate uses the same staging parameter with audit
role `empirical-compact-emitter-candidate-not-verified`. Staging requires twelve
declared changes and `MotionUnchanged=false`; the original extent-only role
requires eight changes and unchanged motion. Metadata records the exact role,
edit count, spatial-candidate status and archive hash. Both require visual QA.

The `LanternModel` v9 builder adds an exact shared pivot. Its v10 builder moves
the owned lamp into c0001, allowing a fresh pack without a coat override. For
v10, pass `-CharacterAnchorBnd <audited base-lamp-v10 c0001.bnd>` and **omit**
`-LanternBodyBnd`. Staging validates the base-model audit and refuses combining
both owners. It marks coverage/motion as unverified until live tests pass.
The four variants and all unrelated Modern assets remain hash-checked.

Never install by blindly copying over an older coat-based experiment: the
deployment owner must back up and remove that old owned body asset after the
game exits and Root Builder clears, or it remains as a duplicate lamp.
