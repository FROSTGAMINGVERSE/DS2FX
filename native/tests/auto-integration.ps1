param([string]$Build="$PSScriptRoot\..\..\local\native-build\Release", [string]$FixturePlugins)
$ErrorActionPreference='Stop'
$stage=Join-Path (Split-Path (Split-Path $PSScriptRoot)) ('local\auto-test-'+[guid]::NewGuid().ToString('N'))
$root="$stage\DS2FX"
New-Item -ItemType Directory -Path "$root\Plugins","$stage\SeamlessCoop\crashdumps" -Force | Out-Null
Copy-Item "$Build\ds2fx_fixture_host.exe","$Build\ds2fx_bridge_fixture_host.exe" $stage
Copy-Item "$Build\ds2fx_fixture_core.dll" "$root\ds2fx_core.dll"
Copy-Item "$Build\ds2fx_bridge_fixture_core.dll" "$root\ds2fx_bridge_core.dll"
foreach($item in @(@('ds2fx_core.dll','core.sha256'),@('ds2fx_bridge_core.dll','bridge.sha256'))){(Get-FileHash "$root\$($item[0])").Hash.ToLowerInvariant() | Set-Content "$root\$($item[1])" -Encoding ascii}
$mods='F:\Modding\Dark Souls 2\mods'
if($FixturePlugins){Copy-Item "$FixturePlugins\*" "$root\Plugins" -Recurse}else{
foreach($name in @('DS2FX - Player HUD','DS2FX - Player Observer','DS2FX - Training Stamina')) {Copy-Item "$mods\$name\Root\Game\DS2FX\Plugins\*" "$root\Plugins" -Recurse}
}
Copy-Item "$PSScriptRoot\..\mods\training_stamina\training.ini" "$root\Plugins\frost-training-stamina\training.ini" -Force
'old crash metadata' | Set-Content "$stage\SeamlessCoop\crashdumps\settings.dat"
$loader="$Build\ds2fx_auto_fixture.exe"
$checks=[Collections.Generic.List[string]]::new()
$logs=[Collections.Generic.List[object]]::new()
function Run-Mode([bool]$seamless) {
    $hostName=if($seamless){'ds2fx_bridge_fixture_host.exe'}else{'ds2fx_fixture_host.exe'}
    $arguments=@('--game',"$stage\$hostName")
    if(-not $seamless){$arguments+='--offline-confirmed'}
    $output=(& $loader @arguments 2>&1 | Out-String)
    if($LASTEXITCODE -ne 0 -or $output -notmatch 'game pid=(\d+)'){throw "Auto launch failed: $output"}
    $log=Get-Content "$env:LOCALAPPDATA\DS2FX\logs\runtime-$($Matches[1]).log" -Raw
    $mode=if($seamless){'Seamless'}else{'Offline'}
    if(-not $output.Contains("Mode: $mode (automatic)")){throw "Wrong auto mode: $output"}
    if($seamless){
        foreach($s in @('READY modules=2','Seamless mode skipped offline-only module: frost-training-stamina','gameplay policy API disabled')) {if(-not $log.Contains($s)){throw "Missing $s"}}
        if($log.Contains('Loaded frost-training-stamina') -or $log.Contains('Stamina call-site hook installed')){throw 'Seamless loaded offline gameplay hook'}
    }else{
        foreach($s in @('READY modules=3','Loaded frost-training-stamina','Stamina call-site hook installed')){if(-not $log.Contains($s)){throw "Missing $s"}}
    }
    $checks.Add("$mode launch, real owned-host injection and expected module policy")
    $logs.Add(@{output=$output;runtime=$log})
}
function Check-Reject([string]$expected) {
    $output=(& $loader --game "$stage\ds2fx_bridge_fixture_host.exe" --check 2>&1 | Out-String)
    if($LASTEXITCODE -eq 0 -or -not $output.Contains($expected)){throw "Expected $expected : $output"}
    $checks.Add($expected)
}
Run-Mode $false
Copy-Item "$Build\ds2fx_bridge_fixture_official.exe" "$stage\ds2sc_launcher.exe"
Check-Reject 'Incomplete Seamless deployment'
Copy-Item "$Build\ds2fx_bridge_fixture_vendor.dll" "$stage\SeamlessCoop\ds2sc.dll"
Run-Mode $true
Move-Item "$stage\ds2sc_launcher.exe" "$stage\launcher.saved"
Check-Reject 'Incomplete Seamless deployment'
Move-Item "$stage\SeamlessCoop\ds2sc.dll" "$stage\vendor.saved"
Run-Mode $false
Copy-Item "$stage\vendor.saved" "$stage\SeamlessCoop\orphan.ASI"
Check-Reject 'Incomplete Seamless deployment'
Move-Item "$stage\SeamlessCoop\orphan.ASI" "$stage\orphan.saved"
Move-Item "$stage\launcher.saved" "$stage\ds2sc_launcher.exe"
Move-Item "$stage\vendor.saved" "$stage\SeamlessCoop\ds2sc.dll"
$manifest="$root\Plugins\frost-player-observer\module.ini"
$original=Get-Content $manifest -Raw
Set-Content $manifest ($original -replace '(?m)^depends=.*$','depends=frost-training-stamina@0.1.0')
Check-Reject 'depends on an inactive offline-only module'
Set-Content $manifest $original
$dll="$root\Plugins\frost-player-observer\plugin.dll"
$originalBytes=[IO.File]::ReadAllBytes($dll)
[IO.File]::AppendAllText($dll,'changed known module')
$newHash=(Get-FileHash $dll).Hash.ToLowerInvariant()
Set-Content $manifest ($original -replace '(?m)^dll_sha256=.*$',"dll_sha256=$newHash")
Check-Reject 'permits only pinned local display modules'
[IO.File]::WriteAllBytes($dll,$originalBytes); Set-Content $manifest $original
Move-Item "$root\Plugins" "$stage\plugins.saved"
$empty=(& $loader --game "$stage\ds2fx_bridge_fixture_host.exe" --check 2>&1 | Out-String)
if($LASTEXITCODE){throw "Core alone should work: $empty"}
$checks.Add('Core alone supported without optional plugins')
Move-Item "$stage\plugins.saved" "$root\Plugins"
$production=(& "$Build\ds2fx_auto.exe" --game "$stage\ds2fx_bridge_fixture_host.exe" --check 2>&1 | Out-String)
if($LASTEXITCODE -eq 0 -or -not $production.Contains('Unsupported Scholar')){throw 'Production accepted synthetic game'}
$checks.Add('Production refuses synthetic game')
$invalid=(& $loader --game "$stage\ds2fx_bridge_fixture_host.exe" --check-official $stage 2>&1 | Out-String)
if($LASTEXITCODE -eq 0 -or -not $invalid.Contains('read-only')){throw 'Launch accepted dependency path override'}
$checks.Add('Real launch cannot override detection folder')
[ordered]@{passed=$true;checks=$checks;runs=$logs;limitation='Synthetic fixtures; live mode switching still requires game validation'} | ConvertTo-Json -Depth 7 | Set-Content "$stage\result.json"
"PASS $($checks.Count) unified checks; evidence $stage\result.json"
