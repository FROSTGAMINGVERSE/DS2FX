param(
    [Parameter(Mandatory)][string]$Build,
    [Parameter(Mandatory)][string]$Runtime,
    [Parameter(Mandatory)][string]$Stage
)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath (Join-Path $Stage 'ds2fx_ingame_hud_runtime.exe')){
    if(Get-Process ds2fx_ingame_hud_runtime -ErrorAction SilentlyContinue){throw 'Owned fixture is still running'}
}
New-Item -ItemType Directory -Path "$Stage\reshade-addons","$Stage\DS2FX\Plugins\ds2-remastered-hud","$Stage\empty-effects" -Force | Out-Null
Copy-Item -LiteralPath "$Build\ds2fx_ingame_hud_runtime.exe" -Destination $Stage -Force
Copy-Item -LiteralPath "$Build\ds2fx_ingame_hud_fixture.dll" -Destination "$Stage\DS2FX\Plugins\ds2-remastered-hud\plugin.dll" -Force
Copy-Item -LiteralPath "$Build\DS2FX-Remastered-HUD.addon64" -Destination "$Stage\reshade-addons" -Force
# Local verification only: never package this third-party runtime with HUD.
Copy-Item -LiteralPath $Runtime -Destination "$Stage\ReShade68.dll" -Force
Set-Content -LiteralPath "$Stage\fixture.ini" -Value @('[ADDON]','AddonPath=.\reshade-addons','[GENERAL]','EffectSearchPaths=.\empty-effects','NoReloadOnInit=1','[INPUT]','KeyOverlay=0,0,0,0')
Copy-Item -LiteralPath "$Stage\fixture.ini" -Destination "$Stage\ReShade.ini" -Force
Push-Location $Stage
try {
    & '.\ds2fx_ingame_hud_runtime.exe'
    if($LASTEXITCODE){throw "Owned ReShade/HUD integration failed: $LASTEXITCODE"}
}finally{Pop-Location}
