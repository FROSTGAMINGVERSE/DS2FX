param([string]$Build = "$PSScriptRoot\..\..\local\native-build\Release", [string]$FixturePlugins)
$ErrorActionPreference='Stop'
$stage=Join-Path (Split-Path (Split-Path $PSScriptRoot)) ('local\seamless-test-'+[guid]::NewGuid().ToString('N'))
$root="$stage\DS2FX"
New-Item -ItemType Directory -Path "$root\Plugins","$stage\SeamlessCoop" -Force | Out-Null
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
foreach($expected in @('SEAMLESS BRIDGE','gameplay policy API disabled','READY modules=2','health_changed','player_unavailable','Observation thread stopped')) {
    if(-not $log.Contains($expected)) { throw "Missing $expected in log: $log" }
}
if($log.Contains('Stamina call-site hook installed')) { throw 'Bridge installed gameplay hook' }
$checks.Add('Official-style launcher exits; owned game remains; late injection, observations and cleanup pass')
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
[pscustomobject]@{passed=$true;checks=$checks;output=$output;runtimeLog=$log;limitation='Owned synthetic fixture only; no real Seamless gameplay or networking proof'} | ConvertTo-Json -Depth 5 | Set-Content "$stage\result.json"
Write-Output "PASS $($checks.Count) bridge checks: $stage\result.json"
