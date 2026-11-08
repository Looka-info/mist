param(
    [string]$OutputDir = "$PSScriptRoot\..\resources"
)

Add-Type -AssemblyName System.Drawing

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

function Build-SleekMistLogo([int]$size, [string]$theme = "dark") {
    $bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)

    $scale = $size / 1024.0

    # 1. Background Squircle
    $pad = 48.0 * $scale
    $sqSize = $size - ($pad * 2.0)
    $r = $sqSize * 0.225 # 22.5% superellipse squircle radius
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $rect = New-Object System.Drawing.RectangleF($pad, $pad, $sqSize, $sqSize)
    $path.AddArc($rect.X, $rect.Y, $r * 2.0, $r * 2.0, 180, 90)
    $path.AddArc($rect.Right - $r * 2.0, $rect.Y, $r * 2.0, $r * 2.0, 270, 90)
    $path.AddArc($rect.Right - $r * 2.0, $rect.Bottom - $r * 2.0, $r * 2.0, $r * 2.0, 0, 90)
    $path.AddArc($rect.X, $rect.Bottom - $r * 2.0, $r * 2.0, $r * 2.0, 90, 90)
    $path.CloseFigure()

    if ($theme -eq "dark" -or $theme -eq "app") {
        # Matte Obsidian gradient
        $cTop = [System.Drawing.Color]::FromArgb(255, 14, 19, 16)
        $cBot = [System.Drawing.Color]::FromArgb(255, 22, 30, 25)
        $bgBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
            (New-Object System.Drawing.PointF 0, $pad),
            (New-Object System.Drawing.PointF 0, ($pad + $sqSize)),
            $cTop, $cBot
        )
        $g.FillPath($bgBrush, $path)

        # Subtle emerald edge rim
        $rimPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(100, 35, 75, 55), [float](2.5 * $scale))
        $g.DrawPath($rimPen, $path)

        # Top edge subtle light catch
        $highlightPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(70, 0, 235, 150), [float](1.5 * $scale))
        $g.DrawPath($highlightPen, $path)
    } elseif ($theme -eq "light") {
        # Clean soft mint porcelain surface
        $cTop = [System.Drawing.Color]::FromArgb(255, 245, 250, 247)
        $cBot = [System.Drawing.Color]::FromArgb(255, 230, 242, 236)
        $bgBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
            (New-Object System.Drawing.PointF 0, $pad),
            (New-Object System.Drawing.PointF 0, ($pad + $sqSize)),
            $cTop, $cBot
        )
        $g.FillPath($bgBrush, $path)

        $rimPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(120, 160, 205, 185), [float](2.0 * $scale))
        $g.DrawPath($rimPen, $path)
    } elseif ($theme -eq "mono") {
        # Deep monochrome black
        $bgBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 18, 18, 18))
        $g.FillPath($bgBrush, $path)
        $rimPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(80, 255, 255, 255), [float](2.0 * $scale))
        $g.DrawPath($rimPen, $path)
    }

    # 2. Sleek Continuous Aerodynamic Stream Ribbon "M"
    $mPath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $pts = @(
        New-Object System.Drawing.PointF ([float](272.0 * $scale)), ([float](730.0 * $scale))
        New-Object System.Drawing.PointF ([float](272.0 * $scale)), ([float](390.0 * $scale))
        New-Object System.Drawing.PointF ([float](374.0 * $scale)), ([float](270.0 * $scale))
        New-Object System.Drawing.PointF ([float](512.0 * $scale)), ([float](520.0 * $scale))
        New-Object System.Drawing.PointF ([float](650.0 * $scale)), ([float](270.0 * $scale))
        New-Object System.Drawing.PointF ([float](752.0 * $scale)), ([float](390.0 * $scale))
        New-Object System.Drawing.PointF ([float](752.0 * $scale)), ([float](730.0 * $scale))
    )
    $mPath.AddLines($pts)

    if ($theme -eq "dark" -or $theme -eq "app") {
        # Soft ambient glow under the ribbon
        $glowPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(35, 0, 230, 150), [float](120.0 * $scale))
        $glowPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
        $glowPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
        $glowPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $g.DrawPath($glowPen, $mPath)

        # Primary crisp neon mint stroke (86px)
        $strokePen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 0, 235, 145), [float](86.0 * $scale))
        $strokePen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
        $strokePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
        $strokePen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $g.DrawPath($strokePen, $mPath)

        # Inner laser core highlight (14px)
        $corePen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(180, 225, 255, 242), [float](14.0 * $scale))
        $corePen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
        $corePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
        $corePen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $g.DrawPath($corePen, $mPath)

        # Floating minimalist play triangle in lower bay
        $pPlay = New-Object System.Drawing.Drawing2D.GraphicsPath
        $playPts = @(
            New-Object System.Drawing.PointF ([float](488.0 * $scale)), ([float](630.0 * $scale))
            New-Object System.Drawing.PointF ([float](548.0 * $scale)), ([float](665.0 * $scale))
            New-Object System.Drawing.PointF ([float](488.0 * $scale)), ([float](700.0 * $scale))
        )
        $pPlay.AddPolygon($playPts)
        $bPlay = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 0, 235, 145))
        $g.FillPath($bPlay, $pPlay)

    } elseif ($theme -eq "light") {
        # Deep forest emerald stroke
        $strokePen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 0, 135, 90), [float](86.0 * $scale))
        $strokePen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
        $strokePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
        $strokePen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $g.DrawPath($strokePen, $mPath)

        # Play triangle
        $pPlay = New-Object System.Drawing.Drawing2D.GraphicsPath
        $playPts = @(
            New-Object System.Drawing.PointF ([float](488.0 * $scale)), ([float](630.0 * $scale))
            New-Object System.Drawing.PointF ([float](548.0 * $scale)), ([float](665.0 * $scale))
            New-Object System.Drawing.PointF ([float](488.0 * $scale)), ([float](700.0 * $scale))
        )
        $pPlay.AddPolygon($playPts)
        $bPlay = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 0, 135, 90))
        $g.FillPath($bPlay, $pPlay)

    } elseif ($theme -eq "mono") {
        $strokePen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 240, 240, 240), [float](86.0 * $scale))
        $strokePen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
        $strokePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
        $strokePen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $g.DrawPath($strokePen, $mPath)

        $pPlay = New-Object System.Drawing.Drawing2D.GraphicsPath
        $playPts = @(
            New-Object System.Drawing.PointF ([float](488.0 * $scale)), ([float](630.0 * $scale))
            New-Object System.Drawing.PointF ([float](548.0 * $scale)), ([float](665.0 * $scale))
            New-Object System.Drawing.PointF ([float](488.0 * $scale)), ([float](700.0 * $scale))
        )
        $pPlay.AddPolygon($playPts)
        $bPlay = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 240, 240, 240))
        $g.FillPath($bPlay, $pPlay)
    }

    $g.Dispose()
    return $bmp
}

# 1. Generate master PNG assets
$logoDark  = Build-SleekMistLogo 512 "dark"
$logoLight = Build-SleekMistLogo 512 "light"
$logoMono  = Build-SleekMistLogo 512 "mono"

$logoDark.Save("$OutputDir\mist-logo.png", [System.Drawing.Imaging.ImageFormat]::Png)
$logoDark.Save("$OutputDir\mist-logo-dark.png", [System.Drawing.Imaging.ImageFormat]::Png)
$logoLight.Save("$OutputDir\mist-logo-light.png", [System.Drawing.Imaging.ImageFormat]::Png)
$logoMono.Save("$OutputDir\mist-logo-mono.png", [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "Generated resources\mist-logo.png (512x512 sleek vector)" -ForegroundColor Green

# 2. Generate multi-resolution ICO (256, 128, 64, 48, 32, 16)
$sizes = @(256, 128, 64, 48, 32, 16)
$bitmaps = @()
foreach ($s in $sizes) {
    $bitmaps += Build-SleekMistLogo $s "dark"
}

$icoPath = "$OutputDir\mist.ico"
$fs = [System.IO.File]::Create($icoPath)
$bw = New-Object System.IO.BinaryWriter($fs)

$bw.Write([uint16]0)
$bw.Write([uint16]1) # Icon type
$bw.Write([uint16]$sizes.Count)

$pngBuffers = @()
foreach ($b in $bitmaps) {
    $ms = New-Object System.IO.MemoryStream
    $b.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngBuffers += ,$ms.ToArray()
    $ms.Dispose()
}

$offset = 6 + ($sizes.Count * 16)
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $bytes = $pngBuffers[$i]
    $w = if ($s -eq 256) { [byte]0 } else { [byte]$s }
    $h = if ($s -eq 256) { [byte]0 } else { [byte]$s }

    $bw.Write($w)
    $bw.Write($h)
    $bw.Write([byte]0)
    $bw.Write([byte]0)
    $bw.Write([uint16]1)
    $bw.Write([uint16]32)
    $bw.Write([uint32]$bytes.Length)
    $bw.Write([uint32]$offset)

    $offset += $bytes.Length
}

for ($i = 0; $i -lt $sizes.Count; $i++) {
    $bw.Write($pngBuffers[$i])
}

$bw.Close()
$fs.Close()
Write-Host "Generated resources\mist.ico with 6 mipmap sizes." -ForegroundColor Green

# Cleanup
$logoDark.Dispose()
$logoLight.Dispose()
$logoMono.Dispose()
foreach ($b in $bitmaps) { $b.Dispose() }
