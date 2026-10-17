param([string]$Build = "$PSScriptRoot\..\..\local\native-build\Release")
$ErrorActionPreference='Stop'
$stage=Join-Path (Split-Path (Split-Path $PSScriptRoot)) ('local\player-state-api-test-'+[guid]::NewGuid().ToString('N'))
$core=Join-Path $stage 'DS2FX'
$plugin=Join-Path $core 'Plugins\ds2-jump-states'
New-Item -ItemType Directory -Path $plugin -Force | Out-Null
Copy-Item -LiteralPath "$Build\ds2fx_state_api_fixture_host.exe" -Destination $stage
Copy-Item -LiteralPath "$Build\ds2fx_fixture_core.dll" -Destination "$core\ds2fx_core.dll"
Copy-Item -LiteralPath "$Build\ds2fx_state_api_fixture_plugin.dll" -Destination "$plugin\plugin.dll"
$hash=(Get-FileHash -LiteralPath "$plugin\plugin.dll").Hash.ToLowerInvariant()
@"
id=ds2-jump-states
version=0.1.0
api=2
core=0.3.0-alpha.1
game_sha256=0045931b8914504531b7864a9488d396dc50cbaf524964016e1d69c3d1173131
dll_sha256=$hash
classification=blocked-online
depends=
"@ | Set-Content -LiteralPath "$plugin\module.ini" -Encoding ascii
$output=(& "$stage\ds2fx_state_api_fixture_host.exe" "$core\ds2fx_core.dll" 2>&1 | Out-String)
$code=$LASTEXITCODE
[pscustomobject]@{exitCode=$code;output=$output;limitation='Owned process/DLL fixture only; no live jumping or multiplayer proof'} | ConvertTo-Json | Set-Content -LiteralPath "$stage\result.json"
if($code -ne 0){throw "State API fixture failed: $output"}
Write-Output "$($output.Trim()) Evidence: $stage\result.json"
