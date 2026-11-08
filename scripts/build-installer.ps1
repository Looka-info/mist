<#
.SYNOPSIS
    Mist Native Windows Game Streaming Platform - Production Distribution & Installer Builder
.DESCRIPTION
    Builds the production distribution directory, packages required runtime binaries,
    generates install.ps1, uninstall.ps1, signs binaries, generates release manifests,
    and creates the distribution archive.
#>

param(
    [string]$DistDir = "dist\mist-windows-x64",
    [string]$ZipOutput = "dist\mist-v0.2.0-rc1-windows-x64.zip",
    [switch]$TestInstallUninstall = $true
)

$ErrorActionPreference = "Stop"

Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host "   MIST - PRODUCTION INSTALLER AND RELEASE PACKAGING" -ForegroundColor Cyan
Write-Host "=======================================================" -ForegroundColor Cyan

# 1. Ensure build outputs exist
$binaries = @(
    "build\apps\host\mist_host.exe",
    "build\apps\client\mist_client.exe",
    "build\apps\desktop\mist_desktop.exe",
    "bin\mist-signaling.exe"
)

$installerBinary = "build\apps\installer\MistSetup.exe"

foreach ($b in $binaries) {
    if (-not (Test-Path $b)) {
        Write-Error "Required binary missing: $b. Please build before packaging."
        exit 1
    }
}

# 2. Prepare staging directory
if (Test-Path $DistDir) {
    Remove-Item -Path $DistDir -Recurse -Force
}
New-Item -ItemType Directory -Path "$DistDir\bin" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistDir\config" -Force | Out-Null

Write-Host "Staging release binaries..." -ForegroundColor Green
foreach ($b in $binaries) {
    Copy-Item -Path $b -Destination "$DistDir\bin" -Force
    Write-Host "  -> Staged: $(Split-Path $b -Leaf)" -ForegroundColor Gray
}

if (Test-Path "resources\mist.ico") {
    Copy-Item -Path "resources\mist.ico" -Destination "$DistDir\bin" -Force
    Write-Host "  -> Staged: mist.ico" -ForegroundColor Gray
}

if (Test-Path "resources") {
    New-Item -ItemType Directory -Path "$DistDir\resources" -Force | Out-Null
    Copy-Item -Path "resources\*" -Destination "$DistDir\resources" -Force
    Write-Host "  -> Staged: resources brand assets (logos, icons)" -ForegroundColor Gray
}

if (Test-Path $installerBinary) {
    Copy-Item -Path $installerBinary -Destination "$DistDir\MistSetup.exe" -Force
    Copy-Item -Path $installerBinary -Destination "dist\MistSetup-v0.2.0-rc1.exe" -Force
    Write-Host "  -> Staged standalone installer: MistSetup-v0.2.0-rc1.exe" -ForegroundColor Green
}

# 3. Create default configuration
$defaultConfig = @"
[general]
app_name = Mist Native Streaming
version = 0.2.0-rc1
log_level = info

[video]
width = 1920
height = 1080
target_fps = 60
codec = h264
bitrate_kbps = 8000
vsync = false

[audio]
enabled = true
sample_rate = 48000
channels = 2
bitrate_kbps = 128

[input]
allow_remote_keyboard = false
allow_remote_mouse = false
allow_remote_gamepad = false
"@

Set-Content -Path "$DistDir\config\mist.ini" -Value $defaultConfig -Encoding UTF8

# 4. Generate install.ps1 script
$installLines = @(
    'param(',
    '    [string]$InstallPath = "$env:LOCALAPPDATA\Programs\Mist",',
    '    [switch]$Silent = $false',
    ')',
    '$ErrorActionPreference = "Stop"',
    'if (-not $Silent) { Write-Host "Installing Mist Native Game Streaming Platform v0.2.0-rc1..." -ForegroundColor Cyan }',
    'if (-not (Test-Path $InstallPath)) { New-Item -ItemType Directory -Path $InstallPath -Force | Out-Null }',
    '$scriptRoot = $PSScriptRoot',
    'Copy-Item -Path "$scriptRoot\bin\*" -Destination $InstallPath -Recurse -Force',
    'Copy-Item -Path "$scriptRoot\config" -Destination $InstallPath -Recurse -Force',
    'Copy-Item -Path "$scriptRoot\uninstall.ps1" -Destination "$InstallPath\uninstall.ps1" -Force',
    '$startMenuDir = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Mist"',
    'if (-not (Test-Path $startMenuDir)) { New-Item -ItemType Directory -Path $startMenuDir -Force | Out-Null }',
    'try {',
    '    $wsh = New-Object -ComObject WScript.Shell',
    '    $shortcut = $wsh.CreateShortcut("$startMenuDir\Mist Desktop.lnk")',
    '    $shortcut.TargetPath = "$InstallPath\mist_desktop.exe"',
    '    $shortcut.WorkingDirectory = $InstallPath',
    '    $shortcut.Description = "Mist Native Game Streaming Desktop Application"',
    '    $shortcut.Save()',
    '} catch {}',
    '$regKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Mist"',
    'if (-not (Test-Path $regKey)) { New-Item -Path $regKey -Force | Out-Null }',
    'Set-ItemProperty -Path $regKey -Name "DisplayName" -Value "Mist Native Game Streaming Platform"',
    'Set-ItemProperty -Path $regKey -Name "DisplayVersion" -Value "0.2.0-rc1"',
    'Set-ItemProperty -Path $regKey -Name "Publisher" -Value "Mist Systems Architecture"',
    'Set-ItemProperty -Path $regKey -Name "InstallLocation" -Value $InstallPath',
    'Set-ItemProperty -Path $regKey -Name "UninstallString" -Value "powershell.exe -ExecutionPolicy Bypass -File `"$InstallPath\uninstall.ps1`""',
    'if (-not $Silent) { Write-Host "Mist v0.2.0-rc1 successfully installed to: $InstallPath" -ForegroundColor Green }'
)
$installScript = $installLines -join "`r`n"
Set-Content -Path "$DistDir\install.ps1" -Value $installScript -Encoding UTF8

# 5. Generate uninstall.ps1 script
$uninstallLines = @(
    'param([switch]$Silent = $false)',
    '$InstallPath = $PSScriptRoot',
    'if (-not $Silent) { Write-Host "Uninstalling Mist Native Game Streaming Platform v0.2.0-rc1..." -ForegroundColor Cyan }',
    '$startMenuDir = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Mist"',
    'if (Test-Path $startMenuDir) { Remove-Item -Path $startMenuDir -Recurse -Force -ErrorAction SilentlyContinue }',
    '$regKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Mist"',
    'if (Test-Path $regKey) { Remove-Item -Path $regKey -Force -ErrorAction SilentlyContinue }',
    'if (Test-Path $InstallPath) {',
    '    $files = Get-ChildItem -Path $InstallPath -Recurse | Where-Object { $_.FullName -ne "$PSScriptRoot\uninstall.ps1" }',
    '    foreach ($f in $files) { Remove-Item -Path $f.FullName -Force -Recurse -ErrorAction SilentlyContinue }',
    '}',
    'if (-not $Silent) { Write-Host "Mist v0.2.0-rc1 uninstallation complete." -ForegroundColor Green }'
)
$uninstallScript = $uninstallLines -join "`r`n"
Set-Content -Path "$DistDir\uninstall.ps1" -Value $uninstallScript -Encoding UTF8

# 6. Execute Code Signing and Release Manifest generation
Write-Host "Executing release code signing..." -ForegroundColor Yellow
& powershell -ExecutionPolicy Bypass -File "scripts\sign-release.ps1" -DistPath "$DistDir" -ManifestOutput "$DistDir\release-manifest.json"
if (Test-Path "dist\MistSetup-v0.2.0-rc1.exe") {
    $certs = Get-ChildItem -Path Cert:\CurrentUser\My -CodeSigningCert -ErrorAction SilentlyContinue
    if ($certs -and $certs.Count -gt 0) {
        Set-AuthenticodeSignature -FilePath "dist\MistSetup-v0.2.0-rc1.exe" -Certificate $certs[0] -TimestampServer "http://timestamp.digicert.com" -ErrorAction SilentlyContinue | Out-Null
    }
}

# 7. Package ZIP Distribution Archive
Write-Host "Creating distribution archive: $ZipOutput" -ForegroundColor Cyan
if (Test-Path $ZipOutput) {
    Remove-Item -Path $ZipOutput -Force
}
Compress-Archive -Path "$DistDir\*" -DestinationPath $ZipOutput -CompressionLevel Optimal
$zipSizeMb = [math]::Round((Get-Item $ZipOutput).Length / 1MB, 2)
Write-Host "Package archive created successfully: $zipSizeMb MB" -ForegroundColor Green

# 8. Test Install and Clean Uninstall Lifecycle
if ($TestInstallUninstall) {
    Write-Host "`nTesting automated install lifecycle..." -ForegroundColor Yellow
    $testInstallDir = "$env:TEMP\MistTestInstall"
    if (Test-Path $testInstallDir) {
        Remove-Item -Path $testInstallDir -Recurse -Force
    }

    & powershell -ExecutionPolicy Bypass -File "$DistDir\install.ps1" -InstallPath $testInstallDir -Silent
    if (-not (Test-Path "$testInstallDir\mist_host.exe")) {
        Write-Error "Test install failed: mist_host.exe not found in $testInstallDir"
        exit 1
    }
    Write-Host "  -> Install verification: PASSED" -ForegroundColor Green

    Write-Host "Testing automated uninstall lifecycle..." -ForegroundColor Yellow
    & powershell -ExecutionPolicy Bypass -File "$testInstallDir\uninstall.ps1" -Silent
    if (Test-Path "$testInstallDir\mist_host.exe") {
        Write-Error "Test uninstall failed: files remained after uninstall."
        exit 1
    }
    Remove-Item -Path $testInstallDir -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "  -> Uninstall verification: PASSED" -ForegroundColor Green

    # Test Native Windows Setup Executable Lifecycle
    if (Test-Path "dist\MistSetup-v0.2.0-rc1.exe") {
        Write-Host "`nTesting native MistSetup.exe silent installation..." -ForegroundColor Yellow
        $nativeTestDir = "$env:TEMP\MistNativeSetupTest"
        if (Test-Path $nativeTestDir) { Remove-Item -Path $nativeTestDir -Recurse -Force }

        Start-Process -FilePath "dist\MistSetup-v0.2.0-rc1.exe" -ArgumentList "/S /D=$nativeTestDir" -Wait
        if (-not (Test-Path "$nativeTestDir\mist_desktop.exe")) {
            Write-Error "Native installer test failed: mist_desktop.exe not found in $nativeTestDir"
            exit 1
        }
        Write-Host "  -> Native installer verification: PASSED" -ForegroundColor Green

        Write-Host "Testing native uninstaller lifecycle..." -ForegroundColor Yellow
        Start-Process -FilePath "$nativeTestDir\uninstall.exe" -ArgumentList "--uninstall --silent" -Wait
        Start-Sleep -Seconds 3
        if (Test-Path "$nativeTestDir\mist_desktop.exe") {
            Write-Error "Native uninstaller test failed: files remained after uninstall."
            exit 1
        }
        Remove-Item -Path $nativeTestDir -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "  -> Native uninstaller verification: PASSED" -ForegroundColor Green
    }
}

Write-Host "`nMist Milestone 7 Release Distribution Complete!" -ForegroundColor Green
