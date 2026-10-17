param([string]$Build = "$PSScriptRoot\..\..\local\native-build\Release")
$ErrorActionPreference = 'Stop'
$taskRoot = Join-Path (Split-Path (Split-Path $PSScriptRoot)) ('local\hud-integration-' + [guid]::NewGuid().ToString('N'))
$core = Join-Path $taskRoot 'DS2FX'
New-Item -ItemType Directory -Path "$core\Plugins" -Force | Out-Null
Copy-Item -LiteralPath "$Build\ds2fx_fixture_host.exe" -Destination $taskRoot
Copy-Item -LiteralPath "$Build\ds2fx_fixture_core.dll" -Destination "$core\ds2fx_core.dll"
(Get-FileHash "$core\ds2fx_core.dll").Hash.ToLowerInvariant() | Set-Content "$core\core.sha256" -Encoding ascii
foreach ($item in @(@{id='frost-player-hud';dll='ds2fx_player_hud.dll';version='0.2.0'},@{id='frost-player-observer';dll='ds2fx_observer.dll';version='0.2.0'})) {
    $directory = Join-Path "$core\Plugins" $item.id
    New-Item -ItemType Directory -Path $directory | Out-Null
    Copy-Item -LiteralPath "$Build\$($item.dll)" -Destination "$directory\plugin.dll"
    $hash = (Get-FileHash "$directory\plugin.dll").Hash.ToLowerInvariant()
    @"
id=$($item.id)
version=$($item.version)
api=2
core=0.3.0-alpha.1
game_sha256=0045931b8914504531b7864a9488d396dc50cbaf524964016e1d69c3d1173131
dll_sha256=$hash
classification=blocked-online
depends=
"@ | Set-Content "$directory\module.ini" -Encoding ascii
}
Copy-Item -LiteralPath "$PSScriptRoot\..\mods\player_hud\hud.ini" -Destination "$core\Plugins\frost-player-hud"
$output = (& "$Build\ds2fx_fixture_loader.exe" --game "$taskRoot\ds2fx_fixture_host.exe" --offline-confirmed 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0 -or $output -notmatch 'game pid=(\d+)') { throw "HUD runtime injection failed: $output" }
$testPid = $Matches[1]
$log = Get-Content "$env:LOCALAPPDATA\DS2FX\logs\runtime-$testPid.log" -Raw
foreach ($expected in @('HUD initialized','Loaded frost-player-hud 0.2.0','READY modules=2','health_changed','player_unavailable','Observation thread stopped')) { if (-not $log.Contains($expected)) { throw "Missing $expected in $log" } }
[pscustomobject]@{passed=$true;test='HUD and Observer loaded by real DS2FX fixture bootstrap before owned host entrypoint';output=$output;log=$log;limitation='Hidden synthetic host; not live game rendering proof'} | ConvertTo-Json -Depth 5 | Set-Content "$taskRoot\result.json"
"PASS HUD integration. Evidence: $taskRoot\result.json"
