param([string]$Build="$PSScriptRoot\..\..\local\native-build\Release")
$ErrorActionPreference='Stop'
$root=Join-Path (Split-Path (Split-Path $PSScriptRoot)) ('local\training-integration-'+[guid]::NewGuid().ToString('N'))
$results=@()
foreach($mode in @('observe','training')) {
    $stage=Join-Path $root $mode
    $core=Join-Path $stage 'DS2FX'
    New-Item -ItemType Directory -Path "$core\Plugins" -Force | Out-Null
    Copy-Item -LiteralPath "$Build\ds2fx_fixture_host.exe" -Destination $stage
    Copy-Item -LiteralPath "$Build\ds2fx_fixture_core.dll" -Destination "$core\ds2fx_core.dll"
    (Get-FileHash "$core\ds2fx_core.dll").Hash.ToLowerInvariant() | Set-Content "$core\core.sha256" -Encoding ascii
    foreach($item in @(@{id='frost-player-observer';dll='ds2fx_observer.dll';version='0.2.0'},@{id='frost-training-stamina';dll='ds2fx_training_stamina.dll';version='0.1.0'})) {
        & "$PSScriptRoot\..\scripts\Package-Module.ps1" -Id $item.id -Version $item.version -Dll "$Build\$($item.dll)" -OutputZip "$stage\$($item.id).zip" | Out-Null
        Expand-Archive -LiteralPath "$stage\$($item.id).zip" -DestinationPath "$stage\unpacked-$($item.id)"
        Copy-Item -LiteralPath "$stage\unpacked-$($item.id)\Root\Game\DS2FX\Plugins\$($item.id)" -Destination "$core\Plugins" -Recurse
    }
    "[Training]`nMode=$mode`nCostPercent=50" | Set-Content "$core\Plugins\frost-training-stamina\training.ini"
    $output=(& "$Build\ds2fx_fixture_loader.exe" --game "$stage\ds2fx_fixture_host.exe" --offline-confirmed 2>&1 | Out-String)
    if($LASTEXITCODE -ne 0 -or $output -notmatch 'game pid=(\d+)'){throw "Training fixture failed $output"}
    $log=Get-Content "$env:LOCALAPPDATA\DS2FX\logs\runtime-$($Matches[1]).log" -Raw
    $stamina=if($mode -eq 'training'){'stamina=30.25/100.00'}else{'stamina=20.25/100.00'}
    foreach($expected in @('Stamina call-site hook installed','READY modules=2',$stamina,'thread_fault=0','Observation thread stopped')){if(!$log.Contains($expected)){throw "Missing $expected in $mode log: $log"}}
    $count=if($mode -eq 'training'){'modified=1'}else{'modified=0'}
    if(!$log.Contains($count)){throw "Missing $count"}
    $results+=[pscustomobject]@{mode=$mode;passed=$true;output=$output;log=$log;limitation='Owned host, not DS2 gameplay proof'}
}
$results | ConvertTo-Json -Depth 5 | Set-Content "$root\results.json"
"PASS observation and gameplay-changing training modes. Evidence: $root\results.json"
