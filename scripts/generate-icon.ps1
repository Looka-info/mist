param(
    [string]$OutputDir = "$PSScriptRoot\..\resources"
)

Add-Type -AssemblyName System.Drawing

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

$Size = 256
$bmp = New-Object System.Drawing.Bitmap $Size, $Size
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.Clear([System.Drawing.Color]::Transparent)

# 1. Base Squircle / Rounded Shield
$rect = New-Object System.Drawing.Rectangle 12, 12, 232, 232
$radius = 52
$path = New-Object System.Drawing.Drawing2D.GraphicsPath
$path.AddArc($rect.X, $rect.Y, $radius, $radius, 180, 90)
$path.AddArc($rect.Right - $radius, $rect.Y, $radius, $radius, 270, 90)
$path.AddArc($rect.Right - $radius, $rect.Bottom - $radius, $radius, $radius, 0, 90)
$path.AddArc($rect.X, $rect.Bottom - $radius, $radius, $radius, 90, 90)
$path.CloseFigure()

# Background Gradient: Dark Slate (#0F172A) to Deep Emerald (#064E3B)
$bgBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.PointF 12, 12),
    (New-Object System.Drawing.PointF 244, 244),
    [System.Drawing.Color]::FromArgb(255, 15, 23, 42),
    [System.Drawing.Color]::FromArgb(255, 6, 78, 59)
)
$g.FillPath($bgBrush, $path)

# Subtle Inset Radial / Glow
$glowPath = New-Object System.Drawing.Drawing2D.GraphicsPath
$glowPath.AddEllipse(64, 40, 128, 128)
$glowBrush = New-Object System.Drawing.Drawing2D.PathGradientBrush $glowPath
$glowBrush.CenterColor = [System.Drawing.Color]::FromArgb(70, 16, 185, 129)
$glowBrush.SurroundColors = @([System.Drawing.Color]::FromArgb(0, 16, 185, 129))
$g.FillEllipse($glowBrush, 32, 20, 192, 192)

# Outer Border: Crisp Vibrant Mint (#10B981)
$borderPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 16, 185, 129), 4.5)
$g.DrawPath($borderPen, $path)

# Soft Inset Border: Bright Mint (#34D399)
$insetPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(90, 52, 211, 153), 1.5)
$g.DrawPath($insetPen, $path)

# 2. Sleek Streaming Broadcast Waves at the top
$wavePen1 = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(180, 110, 231, 183), 3.0)
$wavePen2 = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(120, 16, 185, 129), 2.5)
$wavePen1.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
$wavePen1.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
$wavePen2.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
$wavePen2.EndCap = [System.Drawing.Drawing2D.LineCap]::Round

$g.DrawArc($wavePen1, 96, 44, 64, 40, 200, 140)
$g.DrawArc($wavePen2, 80, 32, 96, 56, 205, 130)

# 3. Futuristic Sleek "M" Wings (Left Wing, Right Wing, Core Node)
# Left Wing
$leftWing = New-Object System.Drawing.Drawing2D.GraphicsPath
$leftWingPts = @(
    New-Object System.Drawing.PointF 54, 188
    New-Object System.Drawing.PointF 54, 96
    New-Object System.Drawing.PointF 88, 80
    New-Object System.Drawing.PointF 116, 132
    New-Object System.Drawing.PointF 98, 152
    New-Object System.Drawing.PointF 88, 144
    New-Object System.Drawing.PointF 80, 188
)
$leftWing.AddPolygon($leftWingPts)
$leftBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.PointF 54, 80),
    (New-Object System.Drawing.PointF 116, 188),
    [System.Drawing.Color]::FromArgb(255, 52, 211, 153), # #34D399
    [System.Drawing.Color]::FromArgb(255, 16, 185, 129)  # #10B981
)
$g.FillPath($leftBrush, $leftWing)

# Right Wing
$rightWing = New-Object System.Drawing.Drawing2D.GraphicsPath
$rightWingPts = @(
    New-Object System.Drawing.PointF 202, 188
    New-Object System.Drawing.PointF 202, 96
    New-Object System.Drawing.PointF 168, 80
    New-Object System.Drawing.PointF 140, 132
    New-Object System.Drawing.PointF 158, 152
    New-Object System.Drawing.PointF 168, 144
    New-Object System.Drawing.PointF 176, 188
)
$rightWing.AddPolygon($rightWingPts)
$rightBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.PointF 140, 80),
    (New-Object System.Drawing.PointF 202, 188),
    [System.Drawing.Color]::FromArgb(255, 16, 185, 129), # #10B981
    [System.Drawing.Color]::FromArgb(255, 5, 150, 105)   # #059669
)
$g.FillPath($rightBrush, $rightWing)

# Center Core Node: Sleek Glowing Diamond Crest
$centerDiamond = New-Object System.Drawing.Drawing2D.GraphicsPath
$centerDiamondPts = @(
    New-Object System.Drawing.PointF 128, 98
    New-Object System.Drawing.PointF 148, 138
    New-Object System.Drawing.PointF 128, 168
    New-Object System.Drawing.PointF 108, 138
)
$centerDiamond.AddPolygon($centerDiamondPts)
$centerBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.PointF 128, 98),
    (New-Object System.Drawing.PointF 128, 168),
    [System.Drawing.Color]::FromArgb(255, 167, 243, 208), # #A7F3D0 (High Mint)
    [System.Drawing.Color]::FromArgb(255, 16, 185, 129)   # #10B981
)
$g.FillPath($centerBrush, $centerDiamond)

# High-Velocity Core Spark (White highlight in the center)
$sparkBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(240, 255, 255, 255))
$g.FillEllipse($sparkBrush, 123, 128, 10, 10)

# Speed Aero Cuts across wings
$cutPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(200, 15, 23, 42), 3.0)
$g.DrawLine($cutPen, 50, 124, 78, 124)
$g.DrawLine($cutPen, 50, 150, 74, 150)
$g.DrawLine($cutPen, 178, 124, 206, 124)
$g.DrawLine($cutPen, 182, 150, 206, 150)

# Clean up graphics
$g.Dispose()

# Save High-Res PNG
$PngPath = Join-Path $OutputDir "mist-logo.png"
$bmp.Save($PngPath, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "Saved high-resolution logo PNG to: $PngPath"

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

# Generate multi-resolution icons (256, 64, 48, 32, 16)
$sizes = @(256, 64, 48, 32, 16)
$entries = New-Object System.Collections.ArrayList
foreach ($s in $sizes) {
    [byte[]]$bytes = Get-PngBytes $bmp $s $s
    [void]$entries.Add(@{ Dim = $s; Data = $bytes })
}

# Construct standard Windows .ICO file with PNG streams
$IcoPath = Join-Path $OutputDir "mist.ico"
$icoStream = [System.IO.File]::Create($IcoPath)
$bw = New-Object System.IO.BinaryWriter $icoStream

# ICONDIR header (6 bytes)
$bw.Write([uint16]0) # Reserved
$bw.Write([uint16]1) # Type: 1 for ICO
$bw.Write([uint16]$entries.Count) # Number of images

# Calculate offset for first image: 6 (ICONDIR) + 16 * count (ICONDIRENTRY)
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
$bmp.Dispose()

Write-Host "Saved multi-resolution Windows ICO to: $IcoPath"

# Generate Windows Resource Script (.rc)
$RcPath = Join-Path $OutputDir "mist.rc"
$rcContent = @"
// Mist Windows Application Resource Script
#include <windows.h>

IDI_MIST_ICON ICON "mist.ico"

VS_VERSION_INFO VERSIONINFO
FILEVERSION     0,2,0,1
PRODUCTVERSION  0,2,0,1
FILEFLAGSMASK   VS_FFI_FILEFLAGSMASK
FILEFLAGS       0
FILEOS          VOS_NT_WINDOWS32
FILETYPE        VFT_APP
FILESUBTYPE     VFT2_UNKNOWN
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "CompanyName",      "Mist Project"
            VALUE "FileDescription",  "Mist Native Low-Latency Game Streaming Studio"
            VALUE "FileVersion",      "0.2.0-rc1"
            VALUE "InternalName",     "mist_desktop"
            VALUE "OriginalFilename", "mist_desktop.exe"
            VALUE "ProductName",      "Mist Game Streaming"
            VALUE "ProductVersion",   "0.2.0-rc1"
            VALUE "LegalCopyright",   "Copyright (C) 2026 Mist Project Contributors"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
"@

Set-Content -Path $RcPath -Value $rcContent -Encoding ASCII
Write-Host "Saved Windows resource script to: $RcPath"
