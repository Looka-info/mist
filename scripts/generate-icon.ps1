param(
    [string]$OutputDir = "$PSScriptRoot\..\resources"
)

Add-Type -AssemblyName System.Drawing

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# Color Definitions
$PixelMint     = [System.Drawing.Color]::FromArgb(255, 184, 235, 213) # #B8EBD5
$FreshMint     = [System.Drawing.Color]::FromArgb(255, 142, 216, 186) # #8ED8BA
$ForestGreen   = [System.Drawing.Color]::FromArgb(255, 23, 75, 56)    # #174B38
$DeepEvergreen = [System.Drawing.Color]::FromArgb(255, 13, 48, 36)    # #0D3024
$CloudBg       = [System.Drawing.Color]::FromArgb(255, 242, 248, 244) # #F2F8F4
$SoftSage      = [System.Drawing.Color]::FromArgb(255, 221, 239, 228) # #DDEFE4
$DarkBg        = [System.Drawing.Color]::FromArgb(255, 16, 26, 21)    # #101A15
$DarkSurface   = [System.Drawing.Color]::FromArgb(255, 24, 35, 28)    # #18231C
$PureWhite     = [System.Drawing.Color]::FromArgb(255, 255, 255, 255)

# Function to draw the Mist Geometric M with Twin Connected Streaming Paths
function Draw-MistLogo {
    param(
        [System.Drawing.Graphics]$g,
        [int]$width,
        [int]$height,
        [string]$variant # "app", "light", "dark", "mono"
    )

    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)

    $scale = $width / 256.0

    # 1. Background Rounded Squircle (24px radius at 256px scale)
    $squirclePath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $rect = New-Object System.Drawing.RectangleF (12 * $scale), (12 * $scale), (232 * $scale), (232 * $scale)
    $radius = 56 * $scale
    $squirclePath.AddArc($rect.X, $rect.Y, $radius, $radius, 180, 90)
    $squirclePath.AddArc($rect.Right - $radius, $rect.Y, $radius, $radius, 270, 90)
    $squirclePath.AddArc($rect.Right - $radius, $rect.Bottom - $radius, $radius, $radius, 0, 90)
    $squirclePath.AddArc($rect.X, $rect.Bottom - $radius, $radius, $radius, 90, 90)
    $squirclePath.CloseFigure()

    if ($variant -eq "app" -or $variant -eq "dark") {
        # Deep Evergreen to Dark Surface gradient
        $bgBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
            (New-Object System.Drawing.PointF 0, 0),
            (New-Object System.Drawing.PointF ($width), ($height)),
            $DeepEvergreen,
            $DarkBg
        )
        $g.FillPath($bgBrush, $squirclePath)

        # Subtle Mint Outline (1.5px)
        $borderPen = New-Object System.Drawing.Pen($ForestGreen, 3.0 * $scale)
        $g.DrawPath($borderPen, $squirclePath)
        $innerBorderPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(60, 184, 235, 213), 1.5 * $scale)
        $g.DrawPath($innerBorderPen, $squirclePath)
    } elseif ($variant -eq "light") {
        $bgBrush = New-Object System.Drawing.SolidBrush($CloudBg)
        $g.FillPath($bgBrush, $squirclePath)
        $borderPen = New-Object System.Drawing.Pen($SoftSage, 3.0 * $scale)
        $g.DrawPath($borderPen, $squirclePath)
    } elseif ($variant -eq "mono") {
        $bgBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 30, 30, 30))
        $g.FillPath($bgBrush, $squirclePath)
        $borderPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 200, 200, 200), 2.0 * $scale)
        $g.DrawPath($borderPen, $squirclePath)
    }

    # 2. Host and Client Path Colors
    $hostColor   = if ($variant -eq "mono") { $PureWhite } elseif ($variant -eq "light") { $ForestGreen } else { $PixelMint }
    $clientColor = if ($variant -eq "mono") { [System.Drawing.Color]::FromArgb(255, 210, 210, 210) } elseif ($variant -eq "light") { $ForestGreen } else { $FreshMint }
    $nexusColor  = if ($variant -eq "mono") { $PureWhite } elseif ($variant -eq "light") { $ForestGreen } else { $PureWhite }

    # 3. Geometric "M" — Twin Connected Streaming Paths
    # Left Vertical Host Pillar (Ascending Stream Path)
    $hostPillar = New-Object System.Drawing.Drawing2D.GraphicsPath
    $hostPts = @(
        New-Object System.Drawing.PointF (52 * $scale), (196 * $scale)
        New-Object System.Drawing.PointF (52 * $scale), (80 * $scale)
        New-Object System.Drawing.PointF (82 * $scale), (62 * $scale)
        New-Object System.Drawing.PointF (82 * $scale), (196 * $scale)
    )
    $hostPillar.AddPolygon($hostPts)
    $hostBrush = New-Object System.Drawing.SolidBrush($hostColor)
    $g.FillPath($hostBrush, $hostPillar)

    # Right Vertical Client Pillar (Receiving Stream Path)
    $clientPillar = New-Object System.Drawing.Drawing2D.GraphicsPath
    $clientPts = @(
        New-Object System.Drawing.PointF (174 * $scale), (196 * $scale)
        New-Object System.Drawing.PointF (174 * $scale), (62 * $scale)
        New-Object System.Drawing.PointF (204 * $scale), (80 * $scale)
        New-Object System.Drawing.PointF (204 * $scale), (196 * $scale)
    )
    $clientPillar.AddPolygon($clientPts)
    $clientBrush = New-Object System.Drawing.SolidBrush($clientColor)
    $g.FillPath($clientBrush, $clientPillar)

    # Host Forward Streaming Diagonal Vector (Left to Center Down)
    $hostDiag = New-Object System.Drawing.Drawing2D.GraphicsPath
    $hostDiagPts = @(
        New-Object System.Drawing.PointF (82 * $scale), (62 * $scale)
        New-Object System.Drawing.PointF (128 * $scale), (140 * $scale)
        New-Object System.Drawing.PointF (128 * $scale), (170 * $scale)
        New-Object System.Drawing.PointF (82 * $scale), (94 * $scale)
    )
    $hostDiag.AddPolygon($hostDiagPts)
    $hostDiagBrush = New-Object System.Drawing.SolidBrush($FreshMint)
    if ($variant -eq "light") {
        $hostDiagBrush = New-Object System.Drawing.SolidBrush($ForestGreen)
    } elseif ($variant -eq "mono") {
        $hostDiagBrush = New-Object System.Drawing.SolidBrush($PureWhite)
    }
    $g.FillPath($hostDiagBrush, $hostDiag)

    # Client Return Feedback Diagonal Vector (Right to Center Down)
    $clientDiag = New-Object System.Drawing.Drawing2D.GraphicsPath
    $clientDiagPts = @(
        New-Object System.Drawing.PointF (174 * $scale), (62 * $scale)
        New-Object System.Drawing.PointF (174 * $scale), (94 * $scale)
        New-Object System.Drawing.PointF (128 * $scale), (170 * $scale)
        New-Object System.Drawing.PointF (128 * $scale), (140 * $scale)
    )
    $clientDiag.AddPolygon($clientDiagPts)
    $clientDiagBrush = New-Object System.Drawing.SolidBrush($PixelMint)
    if ($variant -eq "light") {
        $clientDiagBrush = New-Object System.Drawing.SolidBrush($ForestGreen)
    } elseif ($variant -eq "mono") {
        $clientDiagBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 220, 220, 220))
    }
    $g.FillPath($clientDiagBrush, $clientDiag)

    # Twin Interlocking Stream Bridge (Horizontal Connection Arc at Upper Valley)
    $bridgePen = New-Object System.Drawing.Pen($PixelMint, 4.0 * $scale)
    if ($variant -eq "light") {
        $bridgePen = New-Object System.Drawing.Pen($ForestGreen, 4.0 * $scale)
    } elseif ($variant -eq "mono") {
        $bridgePen = New-Object System.Drawing.Pen($PureWhite, 4.0 * $scale)
    }
    $bridgePen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $bridgePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $g.DrawLine($bridgePen, (82 * $scale), (62 * $scale), (174 * $scale), (62 * $scale))

    # Center Sync Nexus Node (Pristine diamond node representing real-time WebRTC sync)
    $nexus = New-Object System.Drawing.Drawing2D.GraphicsPath
    $nexusPts = @(
        New-Object System.Drawing.PointF (128 * $scale), (124 * $scale)
        New-Object System.Drawing.PointF (140 * $scale), (144 * $scale)
        New-Object System.Drawing.PointF (128 * $scale), (164 * $scale)
        New-Object System.Drawing.PointF (116 * $scale), (144 * $scale)
    )
    $nexus.AddPolygon($nexusPts)
    $nexusBrush = New-Object System.Drawing.SolidBrush($nexusColor)
    $g.FillPath($nexusBrush, $nexus)

    # Central Core Point (Ultra-low latency indicator)
    $dotBrush = if ($variant -eq "light") { New-Object System.Drawing.SolidBrush($CloudBg) } else { New-Object System.Drawing.SolidBrush($DeepEvergreen) }
    $g.FillEllipse($dotBrush, (125 * $scale), (141 * $scale), (6 * $scale), (6 * $scale))
}

# 1. Generate Primary High-Res Logo (mist-logo.png)
$Size = 256
$bmpPrimary = New-Object System.Drawing.Bitmap $Size, $Size
$gPrimary = [System.Drawing.Graphics]::FromImage($bmpPrimary)
Draw-MistLogo -g $gPrimary -width $Size -height $Size -variant "app"
$gPrimary.Dispose()
$primaryPng = Join-Path $OutputDir "mist-logo.png"
$bmpPrimary.Save($primaryPng, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "Generated primary logo: $primaryPng"

# 2. Generate Light Theme Logo (mist-logo-light.png)
$bmpLight = New-Object System.Drawing.Bitmap $Size, $Size
$gLight = [System.Drawing.Graphics]::FromImage($bmpLight)
Draw-MistLogo -g $gLight -width $Size -height $Size -variant "light"
$gLight.Dispose()
$lightPng = Join-Path $OutputDir "mist-logo-light.png"
$bmpLight.Save($lightPng, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "Generated light theme logo: $lightPng"
$bmpLight.Dispose()

# 3. Generate Dark Theme Logo (mist-logo-dark.png)
$bmpDark = New-Object System.Drawing.Bitmap $Size, $Size
$gDark = [System.Drawing.Graphics]::FromImage($bmpDark)
Draw-MistLogo -g $gDark -width $Size -height $Size -variant "dark"
$gDark.Dispose()
$darkPng = Join-Path $OutputDir "mist-logo-dark.png"
$bmpDark.Save($darkPng, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "Generated dark theme logo: $darkPng"
$bmpDark.Dispose()

# 4. Generate Monochrome Logo (mist-logo-mono.png)
$bmpMono = New-Object System.Drawing.Bitmap $Size, $Size
$gMono = [System.Drawing.Graphics]::FromImage($bmpMono)
Draw-MistLogo -g $gMono -width $Size -height $Size -variant "mono"
$gMono.Dispose()
$monoPng = Join-Path $OutputDir "mist-logo-mono.png"
$bmpMono.Save($monoPng, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "Generated monochrome logo: $monoPng"
$bmpMono.Dispose()

# Helper function to generate resized PNG byte arrays
function Get-PngBytes($sourceBmp, $w, $h) {
    $resized = New-Object System.Drawing.Bitmap $w, $h
    $rg = [System.Drawing.Graphics]::FromImage($resized)
    $rg.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $rg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $rg.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $rg.Clear([System.Drawing.Color]::Transparent)
    $rg.DrawImage($sourceBmp, 0, 0, $w, $h)
    $rg.Dispose()

    $ms = New-Object System.IO.MemoryStream
    $resized.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bytes = $ms.ToArray()
    $ms.Dispose()
    $resized.Dispose()
    return $bytes
}

# 5. Generate Multi-Resolution Windows ICO (256, 64, 48, 32, 16)
$sizes = @(256, 64, 48, 32, 16)
$entries = New-Object System.Collections.ArrayList
foreach ($s in $sizes) {
    [byte[]]$bytes = Get-PngBytes $bmpPrimary $s $s
    [void]$entries.Add(@{ Dim = $s; Data = $bytes })
}

$IcoPath = Join-Path $OutputDir "mist.ico"
$icoStream = [System.IO.File]::Create($IcoPath)
$bw = New-Object System.IO.BinaryWriter $icoStream

# ICONDIR header (6 bytes)
$bw.Write([uint16]0) # Reserved
$bw.Write([uint16]1) # Type: 1 for ICO
$bw.Write([uint16]$entries.Count) # Number of images

# Calculate offset for first image
$offset = 6 + 16 * $entries.Count

# Write ICONDIRENTRY for each image (16 bytes each)
foreach ($entry in $entries) {
    $dim = $entry.Dim
    [byte[]]$data = $entry.Data

    $wByte = if ($dim -ge 256) { [byte]0 } else { [byte]$dim }
    $hByte = if ($dim -ge 256) { [byte]0 } else { [byte]$dim }

    $bw.Write($wByte)            # Width
    $bw.Write($hByte)            # Height
    $bw.Write([byte]0)           # Color count (0 for 32-bit)
    $bw.Write([byte]0)           # Reserved
    $bw.Write([uint16]1)         # Color planes
    $bw.Write([uint16]32)        # Bits per pixel
    $bw.Write([uint32]$data.Length) # Image size in bytes
    $bw.Write([uint32]$offset)   # Image offset in file

    $offset += $data.Length
}

# Write PNG image data
foreach ($entry in $entries) {
    [byte[]]$data = $entry.Data
    $bw.Write($data, 0, $data.Length)
}

$bw.Close()
$icoStream.Close()
$bmpPrimary.Dispose()

Write-Host "Generated multi-resolution Windows ICO: $IcoPath"
