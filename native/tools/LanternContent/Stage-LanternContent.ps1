param(
    [Parameter(Mandatory=$true)][string]$SourcePack,
    [Parameter(Mandatory=$true)][string]$StagingDirectory,
    [switch]$TestStart,
    [switch]$TestPrecreation,
    [switch]$OilAwardHook,
    [switch]$NativeLantern,
    [switch]$TorchFlame,
    [string]$OilIconTpf,
    [string]$LanternBodyBnd,
    [string]$CharacterAnchorBnd,
    [string]$CompactFlameCandidateBnd,
    [string]$ToolDll = (Join-Path $PSScriptRoot 'bin\Release\net8.0\LanternContent.dll')
)
$ErrorActionPreference='Stop'
$source=[IO.Path]::GetFullPath($SourcePack)
$stage=[IO.Path]::GetFullPath($StagingDirectory)
$research='F:\Modding\Dark Souls 2\research\lantern-20260925\'
if(-not $stage.StartsWith($research,[StringComparison]::OrdinalIgnoreCase)){throw 'Staging is restricted to the external lantern research directory.'}
if(Test-Path -LiteralPath $stage){throw 'Staging directory must be new; no existing files are replaced.'}
if(-not (Test-Path -LiteralPath $ToolDll)){throw 'Build LanternContent.csproj with SoulsFormatsAssembly before staging.'}
$manifest=Get-Content -LiteralPath (Join-Path $source 'files.tsv')
if($manifest[0] -ne 'DS2FX-CONTENT-2 modern-souls-2 0.2.0 shared-content-experimental'){throw 'Unsupported source content identity.'}
function HashFile([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
$pivotCandidate=$false
$baseModelCandidate=$false
$flameCandidateRole=''
$flameMotionUnchanged=$true
$flameCandidateEditCount=0
$centeredEmitterCandidate=$false
if([bool]$TorchFlame -ne [bool]$CharacterAnchorBnd){throw 'TorchFlame requires its matching CharacterAnchorBnd, and vice versa.'}
if($TorchFlame){
    $anchorAudit=Get-Content -LiteralPath ($CharacterAnchorBnd+'.audit.json') -Raw | ConvertFrom-Json
    if($anchorAudit.dummy -ne 247 -or (HashFile $CharacterAnchorBnd) -ne $anchorAudit.output_sha256.ToLowerInvariant()){throw 'Flame anchor audit mismatch.'}
    if($anchorAudit.role -eq 'base-model-lantern-offline-candidate-not-live-verified'){
        if($LanternBodyBnd){throw 'Base-model lamp must not be combined with a body-specific lamp override.'}
        if($anchorAudit.source_sha256 -ne 'D8851F86E33673DC92408BF25AD55EAEF085EF09A79C08DB142B33BB48FF08EA' -or $anchorAudit.pivot.name -ne 'DS2FX_LanternPivot' -or $anchorAudit.pivot.index -ne 73 -or $anchorAudit.pivot.bone_count -ne 74 -or $anchorAudit.owned_meshes -ne 4 -or $anchorAudit.owned_textures -ne 12 -or -not $anchorAudit.original_flver_restored_exactly -or $anchorAudit.armor_override_required){throw 'Base-model lamp audit mismatch.'}
        $baseModelCandidate=$true;$pivotCandidate=$true
    }else{
        if(-not $LanternBodyBnd){throw 'TorchFlame requires the matching hip model.'}
        if((HashFile $LanternBodyBnd) -ne $anchorAudit.matching_model_sha256.ToLowerInvariant()){throw 'Flame model audit mismatch.'}
    }
    if($anchorAudit.role -eq 'shared-pivot-offline-candidate-not-live-verified'){
        $bodyAudit=Get-Content -LiteralPath ($LanternBodyBnd+'.audit.json') -Raw | ConvertFrom-Json
        if($anchorAudit.pivot.name -ne 'DS2FX_LanternPivot' -or $anchorAudit.pivot.index -ne 73 -or $anchorAudit.pivot.bone_count -ne 74 -or $bodyAudit.pivot.index -ne 74 -or $bodyAudit.pivot.bone_count -ne 75 -or $bodyAudit.pivot.name -ne $anchorAudit.pivot.name -or -not $anchorAudit.original_flver_restored_exactly -or -not $bodyAudit.original_flver_restored_exactly -or (HashFile $LanternBodyBnd) -ne $bodyAudit.output_sha256.ToLowerInvariant()){throw 'Shared-pivot candidate audit mismatch.'}
        $pivotCandidate=$true
    }
    if(Test-Path -LiteralPath (Join-Path $source 'assets\model\chr\c0001.bnd')){throw 'Existing c0001 override requires an explicit merge; refusing to replace it.'}
}
if($CompactFlameCandidateBnd){
    if(-not $TorchFlame){throw 'Compact flame candidate requires TorchFlame and its model/anchor.'}
    $flameAudit=Get-Content -LiteralPath ($CompactFlameCandidateBnd+'.audit.json') -Raw | ConvertFrom-Json
    if($flameAudit.EffectId -ne 6042 -or $flameAudit.InputSHA256 -ne '27871CA29E6A0E9C35F6CC13E2B5E1F1A2FD683A0AD5BE8724D02B78C24A9245' -or (HashFile $CompactFlameCandidateBnd) -ne $flameAudit.OutputSHA256.ToLowerInvariant() -or -not $flameAudit.Torch230ByteIdentical -or -not $flameAudit.UnrelatedSemanticsPreserved -or -not $flameAudit.SoundUnchanged -or $flameAudit.Scale -ne 0.16 -or $flameAudit.OriginalEntriesPreserved -ne 1778 -or $flameAudit.MotionUnchanged -isnot [bool]){throw 'Compact flame candidate audit mismatch.'}
    $flameCandidateRole=$flameAudit.Role;$flameMotionUnchanged=$flameAudit.MotionUnchanged;$flameCandidateEditCount=@($flameAudit.Changes).Count
    switch($flameCandidateRole){
        'empirical-billboard-extent-candidate-not-verified' {if(-not $flameMotionUnchanged -or $flameCandidateEditCount -ne 8){throw 'Extent-only candidate must preserve motion and contain exactly eight edits.'}}
        'empirical-compact-emitter-candidate-not-verified' {if($flameMotionUnchanged -or $flameCandidateEditCount -ne 12){throw 'Compact-emitter candidate must declare changed motion and exactly twelve edits.'};$centeredEmitterCandidate=$true}
        default {throw 'Unsupported compact flame candidate role.'}
    }
    if(Test-Path -LiteralPath (Join-Path $source 'assets\sfx\sfx9999.ffxbnd.dcx')){throw 'Existing source SFX override requires explicit merge.'}
}
foreach($line in $manifest | Select-Object -Skip 1){
    if(-not $line.Trim()){continue};$parts=$line -split "`t",2
    if($parts.Length -ne 2 -or $parts[0].Length -ne 64 -or $parts[1].Contains('..')){throw 'Malformed source manifest.'}
    if((HashFile (Join-Path $source ('assets\'+$parts[1]))) -ne $parts[0]){throw ('Source manifest mismatch: '+$parts[1])}
}
$pack=Join-Path $stage 'modern-souls-2';$work=Join-Path $stage '_build';New-Item -ItemType Directory -Path $pack,$work -Force | Out-Null
Get-ChildItem -LiteralPath $source -Force | Copy-Item -Destination $pack -Recurse
$variants=@('assets\enc_regulation.bnd.dcx','variants\standard-jump.bnd.dcx','variants\estus-off-jump-on.bnd.dcx','variants\estus-off-jump-off.bnd.dcx')
$outputs=@()
for($i=0;$i -lt $variants.Count;$i++){
    $output=Join-Path $work ('variant-'+$i+'.bnd.dcx')
    if($NativeLantern){
        $args=@($ToolDll,'native-lantern',(Join-Path $source $variants[$i]),(Join-Path $PSScriptRoot 'Schemas'),$output)
        if($TestStart){$args+='--test-start'};if($TestPrecreation){$args+='--test-precreation'}
    }else{
        $args=@($ToolDll,'build',(Join-Path $source $variants[$i]),(Join-Path $PSScriptRoot 'Schemas'),$output)
        if($TestStart){$args+='--test-start'};if($TestPrecreation){$args+='--test-precreation'};if($OilIconTpf){$args+='--custom-icon'};if($OilAwardHook){$args+='--oil-award-hook'};if($TorchFlame){$args+='--torch-flame'};if($CompactFlameCandidateBnd){$args+='--compact-flame-candidate'}
    }
    & dotnet @args;if($LASTEXITCODE -ne 0){throw ('Variant build failed: '+$variants[$i])}
    Copy-Item -LiteralPath $output -Destination (Join-Path $pack $variants[$i])
    $outputs += [pscustomobject]@{Variant=$variants[$i];SHA256=HashFile (Join-Path $pack $variants[$i]);Audit=($output+'.audit.json')}
}
$textArgs=if($NativeLantern){@($ToolDll,'native-lantern-text',(Join-Path $source 'assets\menu\text\english'),(Join-Path $work 'text'))}else{@($ToolDll,'text',(Join-Path $source 'assets\menu\text\english'),(Join-Path $work 'text'))}
& dotnet @textArgs
if($LASTEXITCODE -ne 0){throw 'FMG build failed.'}
foreach($n in @('itemname','simpleexplanation','detailedexplanation')){Copy-Item -LiteralPath (Join-Path $work ('text\'+$n+'.fmg')) -Destination (Join-Path $pack ('assets\menu\text\english\'+$n+'.fmg'))}
if($OilIconTpf){$dest=Join-Path $pack 'assets\menu\tex\icon\ic_0060425000.tpf';New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($dest)) -Force | Out-Null;Copy-Item -LiteralPath $OilIconTpf -Destination $dest}
if($LanternBodyBnd){$dest=Join-Path $pack 'assets\model\parts\body\bd_1030_m.bnd';New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($dest)) -Force | Out-Null;Copy-Item -LiteralPath $LanternBodyBnd -Destination $dest}
if($CharacterAnchorBnd){$dest=Join-Path $pack 'assets\model\chr\c0001.bnd';New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($dest)) -Force | Out-Null;Copy-Item -LiteralPath $CharacterAnchorBnd -Destination $dest}
if($CompactFlameCandidateBnd){$dest=Join-Path $pack 'assets\sfx\sfx9999.ffxbnd.dcx';New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($dest)) -Force | Out-Null;Copy-Item -LiteralPath $CompactFlameCandidateBnd -Destination $dest}
$assetRoot=Join-Path $pack 'assets';$newManifest=[Collections.Generic.List[string]]::new();$newManifest.Add($manifest[0])
foreach($f in Get-ChildItem -LiteralPath $assetRoot -File -Recurse | Sort-Object FullName){$relative=[IO.Path]::GetRelativePath($assetRoot,$f.FullName).Replace('\','/');$newManifest.Add((HashFile $f.FullName)+"`t"+$relative)}
[IO.File]::WriteAllLines((Join-Path $pack 'files.tsv'),$newManifest,[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $pack 'jump-off.tsv'),($outputs[1].SHA256+"`tvariants/standard-jump.bnd.dcx`n"),[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $pack 'estus-off.tsv'),('jump-on'+"`t"+$outputs[2].SHA256+"`tvariants/estus-off-jump-on.bnd.dcx`njump-off`t"+$outputs[3].SHA256+"`tvariants/estus-off-jump-off.bnd.dcx`n"),[Text.UTF8Encoding]::new($false))
# Preserve Modern Souls identity and settings metadata. Extra section is explicit offline test provenance.
$role=if($TestPrecreation){if($TestStart){'test-precreation-and-Swordsman-not-for-shipping'}else{'test-precreation-Default-only-not-for-shipping'}}elseif($TestStart){'test-start-Swordsman-only-not-for-shipping'}else{'content-prototype'}
$testOil=if($TestStart){10}else{0}
$precreationOil=if($TestPrecreation){10}else{0}
Add-Content -LiteralPath (Join-Path $pack 'forge.ini') -Value "`n[LanternFixture]`nRole=$role`nOfflineOnly=true`nSwordsmanOil=$testOil`nPrecreationOil=$precreationOil`nPrecreationWandererCoat=$([bool]$TestPrecreation)`nOilItem=60425000`nNativeLanternItem=$(if($NativeLantern){60425001}else{-1})`nOilAwardHookRequired=$([bool]$OilAwardHook)`nRandomTorchLotsPaired=false"
if($TorchFlame){
    $flameId=if($CompactFlameCandidateBnd){6042}else{230}
    $flameSize=if($centeredEmitterCandidate){'empirical-extent-curves-0.16-emitter-and-travel-compressed-needs-visual-test'}elseif($CompactFlameCandidateBnd){'empirical-extent-curves-0.16-motion-unchanged-needs-visual-test'}else{'native-torch-unscaled-needs-visual-test'}
    Add-Content -LiteralPath (Join-Path $pack 'forge.ini') -Value "FlameSfx=$flameId`nFlameDummy=247`nFlameSize=$flameSize`nFlameContainmentVerified=false`nWorldLightHipAnchorVerified=false"
    if($CompactFlameCandidateBnd){Add-Content -LiteralPath (Join-Path $pack 'forge.ini') -Value "FlameCandidateRole=$flameCandidateRole`nFlameMotionUnchanged=$flameMotionUnchanged`nFlameCandidateEdits=$flameCandidateEditCount`nFlameSpatialCandidate=$centeredEmitterCandidate"}
}
if($pivotCandidate){Add-Content -LiteralPath (Join-Path $pack 'forge.ini') -Value "SharedPivotCandidate=true`nSharedPivotName=DS2FX_LanternPivot`nSharedPivotBaseBone=73`nSharedPivotCoatBone=$(if($baseModelCandidate){-1}else{74})`nSharedPivotMotionLiveVerified=false"}
if($baseModelCandidate){Add-Content -LiteralPath (Join-Path $pack 'forge.ini') -Value "LanternMeshOwner=base-character-c0001`nArmorLampOverrideRequired=false`nAllArmorGenderCoverageLiveVerified=false"}
$changed=@('assets\enc_regulation.bnd.dcx','variants\standard-jump.bnd.dcx','variants\estus-off-jump-on.bnd.dcx','variants\estus-off-jump-off.bnd.dcx','assets\menu\text\english\itemname.fmg','assets\menu\text\english\simpleexplanation.fmg','assets\menu\text\english\detailedexplanation.fmg','files.tsv','jump-off.tsv','estus-off.tsv','forge.ini')
$preserved=0
foreach($file in Get-ChildItem -LiteralPath $source -File -Recurse){$relative=[IO.Path]::GetRelativePath($source,$file.FullName);if($relative -in $changed){continue};if((HashFile $file.FullName) -ne (HashFile (Join-Path $pack $relative))){throw ('Unexpected original asset difference: '+$relative)};$preserved++}
$audit=[pscustomobject]@{Role=$role;Source=$source;Pack=$pack;Variants=$outputs;PreservedSourceFiles=$preserved;OilIcon=($OilIconTpf -ne '');NativeLantern=[bool]$NativeLantern;NativeLanternItem=$(if($NativeLantern){60425001}else{-1});HipModel=($LanternBodyBnd -ne '' -or $baseModelCandidate);TorchFlame=[bool]$TorchFlame;CharacterAnchor=($CharacterAnchorBnd -ne '');CompactFlameEmpiricalCandidate=($CompactFlameCandidateBnd -ne '');FlameCandidateRole=$flameCandidateRole;FlameMotionUnchanged=$flameMotionUnchanged;FlameCandidateEditCount=$flameCandidateEditCount;FlameSpatialCandidate=$centeredEmitterCandidate;FlameArchiveSHA256=$(if($CompactFlameCandidateBnd){HashFile $CompactFlameCandidateBnd}else{''});SharedPivotCandidate=$pivotCandidate;BaseModelLampCandidate=$baseModelCandidate;SharedPivotMotionLiveVerified=$false;OfflineOnly=$true;OilAwardHookRequired=[bool]$OilAwardHook;RandomTorchLotsPaired=$false;FlameContainmentLiveVerified=$false;Deployed=$false}
$audit | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $stage 'staging.audit.json') -Encoding utf8
$audit | ConvertTo-Json -Depth 6
