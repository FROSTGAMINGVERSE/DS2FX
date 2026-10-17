# Run with Windows PowerShell 5.1 for its Windows System.Drawing implementation.
param([Parameter(Mandatory=$true)][string]$Png,[Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$Texconv)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputDirectory){throw 'Use a new output directory'}
if((Get-FileHash -LiteralPath $Texconv -Algorithm SHA256).Hash -ne 'DCFDEC10244E02CF5037FBA089C55FB7E1326B1C8181742D77D15FA5CB5EEF06'){throw 'Unsupported texconv binary; expected official Microsoft may2026 release digest'}
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$inputBitmap=[Drawing.Bitmap]::FromFile($Png)
try {
    $left=$inputBitmap.Width;$top=$inputBitmap.Height;$right=-1;$bottom=-1
    for($y=0;$y-lt$inputBitmap.Height;$y++){for($x=0;$x-lt$inputBitmap.Width;$x++){if($inputBitmap.GetPixel($x,$y).A -gt 8){$left=[Math]::Min($left,$x);$right=[Math]::Max($right,$x);$top=[Math]::Min($top,$y);$bottom=[Math]::Max($bottom,$y)}}}
    if($right-lt$left){throw 'Source icon has no visible pixels'}
    $sourceRect=New-Object Drawing.Rectangle($left,$top,($right-$left+1),($bottom-$top+1))
    $factor=[Math]::Min(60.0/$sourceRect.Width,124.0/$sourceRect.Height)
    $width=[int][Math]::Round($sourceRect.Width*$factor);$height=[int][Math]::Round($sourceRect.Height*$factor)
    $bitmap=New-Object Drawing.Bitmap(64,128,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics=[Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([Drawing.Color]::Transparent)
        $graphics.CompositingMode=[Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $target=New-Object Drawing.Rectangle(([int]((64-$width)/2)),([int]((128-$height)/2)),$width,$height)
        $graphics.DrawImage($inputBitmap,$target,$sourceRect,[Drawing.GraphicsUnit]::Pixel)
        $bitmap.Save((Join-Path $OutputDirectory 'oil-icon.png'),[Drawing.Imaging.ImageFormat]::Png)
    } finally {$graphics.Dispose();$bitmap.Dispose()}
} finally {$inputBitmap.Dispose()}
& $Texconv -f BC3_UNORM -m 1 -nologo -o $OutputDirectory (Join-Path $OutputDirectory 'oil-icon.png')
if($LASTEXITCODE-ne0){throw 'BC3 encoding failed'}
[pscustomobject]@{Input=$Png;InputSHA256=(Get-FileHash -LiteralPath $Png -Algorithm SHA256).Hash;Width=64;Height=128;SourceAlphaBounds=@($left,$top,$right,$bottom);Format='BC3_UNORM/DXT5';Mipmaps=1;TexconvSHA256=(Get-FileHash -LiteralPath $Texconv -Algorithm SHA256).Hash} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory 'icon.audit.json')
