param([string]$Build = "$PSScriptRoot\..\..\local\native-build\Release")
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot)
$stage = Join-Path $project ('local\remastered-integration-' + [guid]::NewGuid().ToString('N'))
$core = Join-Path $stage 'DS2FX'
$plugin = Join-Path $core 'Plugins\ds2-remastered-hud'
New-Item -ItemType Directory -Path $plugin -Force | Out-Null
Copy-Item -LiteralPath "$Build\ds2fx_fixture_host.exe" -Destination $stage
Copy-Item -LiteralPath "$Build\ds2fx_fixture_core.dll" -Destination "$core\ds2fx_core.dll"
Copy-Item -LiteralPath "$Build\ds2fx_remastered_hud.dll" -Destination "$plugin\plugin.dll"
Copy-Item -LiteralPath "$PSScriptRoot\..\mods\ds2_remastered_hud\hud.ini" -Destination $plugin
Copy-Item -LiteralPath "$PSScriptRoot\..\mods\ds2_remastered_hud\Skins" -Destination $plugin -Recurse
$coreHash = (Get-FileHash -LiteralPath "$core\ds2fx_core.dll" -Algorithm SHA256).Hash.ToLowerInvariant()
$pluginHash = (Get-FileHash -LiteralPath "$plugin\plugin.dll" -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath "$core\core.sha256" -Value $coreHash -Encoding ascii
@"
id=ds2-remastered-hud
version=0.1.25
api=2
core=0.3.0-alpha.1
game_sha256=0045931b8914504531b7864a9488d396dc50cbaf524964016e1d69c3d1173131
dll_sha256=$pluginHash
classification=blocked-online
depends=
"@ | Set-Content -LiteralPath "$plugin\module.ini" -Encoding ascii
$check = (& "$Build\ds2fx_fixture_loader.exe" --check --game "$stage\ds2fx_fixture_host.exe" 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0 -or -not $check.Contains('modules=1')) { throw "Remastered fixture preflight failed: $check" }
$output = (& "$Build\ds2fx_fixture_loader.exe" --game "$stage\ds2fx_fixture_host.exe" --offline-confirmed 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0 -or $output -notmatch 'game pid=(\d+)') { throw "Remastered fixture launch failed: $output" }
$pidValue = $Matches[1]
$logPath = Join-Path $env:LOCALAPPDATA "DS2FX\logs\runtime-$pidValue.log"
$log = Get-Content -LiteralPath $logPath -Raw
foreach ($expected in @('Loaded ds2-remastered-hud 0.1.25','READY modules=1','Observation thread stopped')) {
    if (-not $log.Contains($expected)) { throw "Missing $expected in fixture log: $log" }
}
if ($log.Contains('Selective HUD draw adapter installed')) { throw 'Fixture Core must not install live-game draw hooks' }
[pscustomobject]@{
    passed = $true
    test = 'Remastered HUD 0.1.25 loads with ABI 2 region mask tail and fails closed without native draw hooks'
    preflight = $check.Trim()
    output = $output.Trim()
    log = $log
    limitation = 'Synthetic host; no game scene, covenant mapping, real draw frequency, or visual proof'
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'result.json')
Write-Output "PASS Remastered fixture integration. Evidence: $stage\result.json"
