param([string]$Build = "$PSScriptRoot\..\..\local\native-build\Release")
$ErrorActionPreference = 'Stop'
$testRoot = Join-Path (Split-Path (Split-Path $PSScriptRoot)) ('local\runtime-test-' + [guid]::NewGuid().ToString('N'))
$core = Join-Path $testRoot 'DS2FX'
$plugins = Join-Path $core 'Plugins'
$module = Join-Path $plugins 'frost-player-observer'
New-Item -ItemType Directory -Path $module -Force | Out-Null
Copy-Item -LiteralPath "$Build\ds2fx_fixture_host.exe" -Destination $testRoot
Copy-Item -LiteralPath "$Build\ds2fx_fixture_core.dll" -Destination "$core\ds2fx_core.dll"
Copy-Item -LiteralPath "$Build\ds2fx_observer.dll" -Destination "$module\plugin.dll"
$coreHash = (Get-FileHash -LiteralPath "$core\ds2fx_core.dll").Hash.ToLowerInvariant()
Set-Content -LiteralPath "$core\core.sha256" -Value $coreHash -Encoding ascii
$dllHash = (Get-FileHash -LiteralPath "$module\plugin.dll").Hash.ToLowerInvariant()
$manifest = @"
id=frost-player-observer
version=0.2.0
api=2
core=0.3.0-alpha.1
game_sha256=0045931b8914504531b7864a9488d396dc50cbaf524964016e1d69c3d1173131
dll_sha256=$dllHash
classification=blocked-online
depends=
"@
function Set-Manifest([string]$Text) { Set-Content -LiteralPath "$module\module.ini" -Value $Text -Encoding ascii }
Set-Manifest $manifest
$results = [System.Collections.Generic.List[object]]::new()
function Check([string]$Name,[bool]$Success,[string]$Expected) {
    $output = (& "$Build\ds2fx_fixture_loader.exe" --check --game "$testRoot\ds2fx_fixture_host.exe" 2>&1 | Out-String)
    $passed = (($LASTEXITCODE -eq 0) -eq $Success) -and ($output -match [regex]::Escape($Expected))
    $results.Add([pscustomobject]@{test=$Name;passed=$passed;output=$output.Trim()})
    if (-not $passed) { throw "Failed $Name : $output" }
}
Check 'valid module preflight' $true 'modules=1'
Set-Manifest ($manifest.Replace('api=2','api=1'))
Check 'old ABI 1 refused' $false 'Unsupported plugin API'
Set-Manifest ($manifest.Replace('api=2','api=99'))
Check 'unsupported API rejected' $false 'Unsupported plugin API'
Set-Manifest ($manifest.Replace('core=0.3.0-alpha.1','core=9.0.0'))
Check 'core version rejected' $false 'Core version mismatch'
Set-Manifest ($manifest.Replace('depends=','depends=missing@1.0.0'))
Check 'missing dependency rejected' $false 'Missing dependency'
Set-Manifest ($manifest.Replace('depends=','depends=frost-player-observer@0.2.0'))
Check 'dependency cycle rejected' $false 'dependency cycle'
Set-Manifest ($manifest.Replace('depends=','depends=frost-player-observer@9.0.0'))
Check 'dependency version rejected' $false 'Dependency version mismatch'
Set-Manifest ($manifest + "`napi=2")
Check 'duplicate keys rejected' $false 'Duplicate manifest key'
Set-Manifest ($manifest.Replace('id=frost-player-observer','id=../escape'))
Check 'path traversal rejected' $false 'Invalid module id'
Set-Manifest ($manifest.Replace('classification=blocked-online','classification=online'))
Check 'online classification rejected' $false 'blocked-online'
Set-Manifest ($manifest.Replace("dll_sha256=$dllHash",'dll_sha256=' + ('0' * 64)))
Check 'tampered plugin rejected' $false 'Plugin hash mismatch'
Set-Manifest $manifest
Set-Content -LiteralPath "$core\core.sha256" -Value ('0' * 64) -Encoding ascii
Check 'tampered core rejected' $false 'Core payload missing or hash mismatch'
Set-Content -LiteralPath "$core\core.sha256" -Value $coreHash -Encoding ascii
$productionOutput = (& "$Build\ds2fx_loader.exe" --check --game "$testRoot\ds2fx_fixture_host.exe" 2>&1 | Out-String)
if ($LASTEXITCODE -eq 0 -or $productionOutput -notmatch 'Unsupported Scholar') { throw 'Production game gate failed' }
$results.Add([pscustomobject]@{test='production launcher refuses synthetic host';passed=$true;output=$productionOutput.Trim()})
$run = (& "$Build\ds2fx_fixture_loader.exe" --game "$testRoot\ds2fx_fixture_host.exe" --offline-confirmed 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) { throw "Fixture injection failed: $run" }
if ($run -notmatch 'game pid=(\d+)') { throw "Missing fixture pid: $run" }
$fixturePid = $Matches[1]
$log = Get-Content -LiteralPath "$env:LOCALAPPDATA\DS2FX\logs\runtime-$fixturePid.log" -Raw
foreach ($expected in @('FIXTURE BUILD','READY modules=1','player_available','health_changed','health=75','stamina_changed','stamina=40.25/100.00','player_died','player_revived','player_unavailable','Observation thread stopped')) {
    if ($log -notmatch [regex]::Escape($expected)) { throw "Missing runtime evidence: $expected`n$log" }
}
$results.Add([pscustomobject]@{test='owned process injection, dependent plugin, state transitions and shutdown';passed=$true;output=$run.Trim();log=$log})
function Expect-BootFailure([string]$Name) {
    if (Test-Path -LiteralPath "$testRoot\entrypoint-ran.txt") { Remove-Item -LiteralPath "$testRoot\entrypoint-ran.txt" }
    $output = (& "$Build\ds2fx_fixture_loader.exe" --game "$testRoot\ds2fx_fixture_host.exe" --offline-confirmed 2>&1 | Out-String)
    if ($LASTEXITCODE -eq 0 -or $output -notmatch 'DS2FX initialization failed' -or (Test-Path -LiteralPath "$testRoot\entrypoint-ran.txt")) { throw "Failed boot refusal $Name : $output" }
    $results.Add([pscustomobject]@{test=$Name;passed=$true;output=$output.Trim()})
}
# A valid PE and hash do not make a DLL a valid plugin.
Copy-Item -LiteralPath "$Build\ds2fx_fixture_core.dll" -Destination "$module\plugin.dll" -Force
$wrongHash = (Get-FileHash -LiteralPath "$module\plugin.dll").Hash.ToLowerInvariant()
Set-Manifest ($manifest.Replace($dllHash,$wrongHash))
Expect-BootFailure 'missing plugin export aborts before host entrypoint'
Copy-Item -LiteralPath "$Build\ds2fx_observer.dll" -Destination "$module\plugin.dll" -Force
Set-Manifest ($manifest.Replace('version=0.2.0','version=9.0.0'))
Expect-BootFailure 'plugin identity mismatch aborts before host entrypoint'
Set-Manifest $manifest
Copy-Item -LiteralPath "$Build\ds2fx_core.dll" -Destination "$core\ds2fx_core.dll" -Force
(Get-FileHash -LiteralPath "$core\ds2fx_core.dll").Hash.ToLowerInvariant() | Set-Content -LiteralPath "$core\core.sha256" -Encoding ascii
Expect-BootFailure 'production core independently refuses synthetic host'
$results | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$testRoot\results.json" -Encoding utf8
Write-Output "PASS: $($results.Count) integration checks. Evidence: $testRoot\results.json"

