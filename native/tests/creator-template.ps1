param([string]$Build="$PSScriptRoot\..\..\local\native-build\Release")
$ErrorActionPreference='Stop'
$root=Join-Path (Split-Path (Split-Path $PSScriptRoot)) ('local\creator-test-'+[guid]::NewGuid().ToString('N'))
& "$PSScriptRoot\..\scripts\New-DS2FXModule.ps1" -Id 'creator-test' -OutputDirectory "$root\project" | Out-Null
& "$root\project\Build.ps1" | Out-Null
$zip=Get-ChildItem "$root\project\dist" -Filter *.zip | Select-Object -First 1
Expand-Archive -LiteralPath $zip.FullName -DestinationPath "$root\package"
$stage=Join-Path $root 'host'
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -LiteralPath "$root\package\Root\Game\DS2FX" -Destination $stage -Recurse
Copy-Item -LiteralPath "$Build\ds2fx_fixture_host.exe" -Destination $stage
Copy-Item -LiteralPath "$Build\ds2fx_fixture_core.dll" -Destination "$stage\DS2FX\ds2fx_core.dll"
(Get-FileHash "$stage\DS2FX\ds2fx_core.dll").Hash.ToLowerInvariant() | Set-Content "$stage\DS2FX\core.sha256" -Encoding ascii
$output=(& "$Build\ds2fx_fixture_loader.exe" --game "$stage\ds2fx_fixture_host.exe" --offline-confirmed 2>&1 | Out-String)
if($LASTEXITCODE -ne 0 -or $output -notmatch 'game pid=(\d+)'){throw "Creator template failed: $output"}
$log=Get-Content "$env:LOCALAPPDATA\DS2FX\logs\runtime-$($Matches[1]).log" -Raw
foreach($expected in @('Creator module initialized (API 2)','Loaded creator-test 0.1.0','event=6','event=7','Observation thread stopped')){if(!$log.Contains($expected)){throw "Missing $expected"}}
if(!(Test-Path "$stage\DS2FX\Plugins\creator-test\settings.ini")){throw 'Configuration missing from package'}
[pscustomobject]@{passed=$true;steps='Generate, compile standalone project, package config, load through actual fixture core, observe death/revival';log=$log} | ConvertTo-Json | Set-Content "$root\result.json"
"PASS creator template end-to-end. Evidence: $root\result.json"
