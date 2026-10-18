param([string]$Build = "$PSScriptRoot\..\..\local\native-build\Release", [string]$FixturePlugins, [ValidateRange(0,30000)][int]$ContentDelayMs=0)
$ErrorActionPreference='Stop'
$stage=Join-Path (Split-Path (Split-Path $PSScriptRoot)) ('local\seamless-test-'+[guid]::NewGuid().ToString('N'))
$root="$stage\DS2FX"
New-Item -ItemType Directory -Path "$root\Plugins","$stage\SeamlessCoop" -Force | Out-Null
New-Item -ItemType Directory -Path "$root\Content\vanilla-adp\assets\Param" -Force | Out-Null
$asset="$root\Content\vanilla-adp\assets\Param\preentry.param"
[IO.File]::WriteAllText($asset,"owned pre-entry content fixture`n",[Text.Encoding]::ASCII)
$assetHash=(Get-FileHash $asset).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$root\Content\vanilla-adp\files.tsv","DS2FX-CONTENT-1 vanilla-adp 1.2b shared-content-experimental`n$assetHash`tParam/preentry.param`n",[Text.Encoding]::ASCII)
[IO.File]::WriteAllText("$stage\fixture.ini","[Fixture]`nContentDelayMs=$ContentDelayMs`n",[Text.Encoding]::ASCII)
Copy-Item "$Build\ds2fx_bridge_fixture_host.exe" $stage
Copy-Item "$Build\ds2fx_bridge_fixture_official.exe" "$stage\ds2sc_launcher.exe"
Copy-Item "$Build\ds2fx_bridge_fixture_vendor.dll" "$stage\SeamlessCoop\ds2sc.dll"
Copy-Item "$Build\ds2fx_bridge_fixture_core.dll" "$root\ds2fx_bridge_core.dll"
(Get-FileHash "$root\ds2fx_bridge_core.dll").Hash.ToLowerInvariant() | Set-Content "$root\bridge.sha256" -Encoding ascii
$mods='F:\Modding\Dark Souls 2\mods'
foreach($name in @('DS2FX - Player HUD','DS2FX - Player Observer')) {
    if($FixturePlugins) {
        $id=if($name -eq 'DS2FX - Player HUD'){'frost-player-hud'}else{'frost-player-observer'}
        Copy-Item "$FixturePlugins\$id" "$root\Plugins" -Recurse
    }else{Copy-Item "$mods\$name\Root\Game\DS2FX\Plugins\*" "$root\Plugins" -Recurse}
}
$hostPath="$stage\ds2fx_bridge_fixture_host.exe"
$loader="$Build\ds2fx_bridge_fixture_loader.exe"
$checks=[System.Collections.Generic.List[string]]::new()
function Reject([string]$expected) {
    $output=(& $loader --game $hostPath --check 2>&1 | Out-String)
    if($LASTEXITCODE -eq 0 -or -not $output.Contains($expected)) { throw "Expected rejection '$expected': $output" }
    $checks.Add($expected)
}
$output=(& $loader --game $hostPath 2>&1 | Out-String)
if($LASTEXITCODE -ne 0 -or $output -notmatch 'READY game pid=(\d+)') { throw "Bridge fixture failed: $output" }
$testPid=$Matches[1]
$log=Get-Content "$env:LOCALAPPDATA\DS2FX\logs\runtime-$testPid.log" -Raw
foreach($expected in @('PRE-ENTRY CONTENT READY','SEAMLESS BRIDGE','gameplay policy API disabled','READY modules=2','health_changed','player_unavailable','Observation thread stopped')) {
    if(-not $log.Contains($expected)) { throw "Missing $expected in log: $log" }
}
if($log.Contains('Stamina call-site hook installed')) { throw 'Bridge installed gameplay hook' }
if($output -notmatch 'Captured official CREATE_SUSPENDED primary thread before executable entry') { throw "Missing startup capture: $output" }
if(([regex]::Matches($log,'Content routing active; assets=1;')).Count -ne 1) { throw "Expected exactly one actual content hook activation: $log" }
if(-not $log.Contains('Verified pre-entry content routing retained for runtime initialization')) { throw "Runtime did not verify and retain the pre-entry hook: $log" }
foreach($receipt in @('content-at-crt.txt','content-after-bootstrap.txt')) {
    if(-not (Test-Path -LiteralPath "$stage\$receipt") -or (Get-Content -LiteralPath "$stage\$receipt" -Raw) -notmatch '^PASS ') { throw "Missing successful actual content route receipt: $receipt" }
}
if($ContentDelayMs -gt 20000) {
    if(-not $output.Contains('Initialization exceeded 20 seconds; retaining ownership until remote initialization completes or game exits')) { throw "Delayed bootstrap did not exercise the loader timeout diagnostic: $output" }
    $checks.Add('Initialization exceeds 20 seconds without allowing host CRT before the actual content hook is ready')
}
$checks.Add('Official-style suspended launch captured; actual content routes during host CRT and after retained runtime initialization; later observations and cleanup pass')
if($FixturePlugins){Copy-Item "$FixturePlugins\frost-training-stamina" "$root\Plugins" -Recurse}
else{Copy-Item "$mods\DS2FX - Training Stamina\Root\Game\DS2FX\Plugins\frost-training-stamina" "$root\Plugins" -Recurse}
Reject 'permits only pinned local display modules or identity-only Lantern'
Move-Item "$root\Plugins\frost-training-stamina" "$stage\rejected-training"
$manifest="$root\Plugins\frost-player-observer\module.ini"
$original=[IO.File]::ReadAllText($manifest)
$dll="$root\Plugins\frost-player-observer\plugin.dll"
Copy-Item $dll "$stage\observer-original.dll"
[IO.File]::AppendAllText($dll,'tamper')
Reject 'Plugin hash mismatch'
$tamperedHash=(Get-FileHash $dll).Hash.ToLowerInvariant()
[IO.File]::WriteAllText($manifest,($original -replace '(?m)^dll_sha256=.*$',"dll_sha256=$tamperedHash"))
Reject 'permits only pinned local display modules or identity-only Lantern'
Copy-Item "$stage\observer-original.dll" $dll -Force
[IO.File]::WriteAllText($manifest,$original)
$seal=[IO.File]::ReadAllText("$root\bridge.sha256")
('0'*64) | Set-Content "$root\bridge.sha256"
Reject 'Bridge core seal mismatch'
[IO.File]::WriteAllText("$root\bridge.sha256",$seal)
Copy-Item "$stage\SeamlessCoop\ds2sc.dll" "$stage\dinput8.dll"
Reject 'Unexpected root loader'
Move-Item "$stage\dinput8.dll" "$stage\rejected-proxy.dll"
$prod=(& "$Build\ds2fx_seamless.exe" --game $hostPath --check 2>&1 | Out-String)
if($LASTEXITCODE -eq 0 -or -not $prod.Contains('Unsupported Scholar executable')) { throw "Production accepted fixture: $prod" }
$checks.Add('Production refuses synthetic executable')
$prod=(& "$Build\ds2fx_seamless.exe" --game 'F:\Games\Steam\steamapps\common\Dark Souls II Scholar of the First Sin\Game\DarkSoulsII.exe' --check --check-core $root --check-official $stage 2>&1 | Out-String)
if($LASTEXITCODE -eq 0 -or -not $prod.Contains('Unsupported official Seamless launcher hash')) { throw "Production accepted fake official launcher: $prod" }
$checks.Add('Production refuses unrecognized official launcher')
$running=Start-Process -FilePath $hostPath -PassThru -WindowStyle Hidden
try {
    $already=(& $loader --game $hostPath 2>&1 | Out-String)
    if($LASTEXITCODE -eq 0 -or -not $already.Contains('already running')) { throw "Bridge accepted preexisting game: $already" }
    $checks.Add('Preexisting game refused; no arbitrary attach')
} finally { $running.WaitForExit() }
[pscustomobject]@{passed=$true;contentDelayMs=$ContentDelayMs;checks=$checks;output=$output;runtimeLog=$log;contentAtCrt=(Get-Content -LiteralPath "$stage\content-at-crt.txt" -Raw);contentAfterBootstrap=(Get-Content -LiteralPath "$stage\content-after-bootstrap.txt" -Raw);limitation='Owned synthetic fixture only; no real Seamless gameplay or networking proof'} | ConvertTo-Json -Depth 5 | Set-Content "$stage\result.json"
Write-Output "PASS $($checks.Count) bridge checks: $stage\result.json"
