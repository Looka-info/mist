<#
.SYNOPSIS
    Unified Windows Build Orchestrator for Mist (Rust + Slint + Native Engines)
.DESCRIPTION
    Builds the complete Mist platform:
    1. CMake native media engine targets (mist_host, mist_client, mist_unit_tests).
    2. Cargo Rust workspace (mist-desktop with Slint, mist-signaling, all foundation crates).
    3. Runs automated unit & integration test suites.
    4. Stages binaries and packages production installer.
#>

param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$Clean = $false,
    [switch]$Test = $true,
    [switch]$Package = $false
)

$ErrorActionPreference = "Stop"

Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host "   MIST PLATFORM BUILD - RUST + SLINT + NATIVE ENGINES" -ForegroundColor Cyan
Write-Host "   Configuration: $Configuration" -ForegroundColor Cyan
Write-Host "=======================================================" -ForegroundColor Cyan

# 1. Check tools
Write-Host "`n[1/5] Verifying Development Toolchain..." -ForegroundColor Yellow
$cargo = Get-Command cargo -ErrorAction SilentlyContinue
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cargo -or -not $cmake) {
    Write-Error "Required tools missing. Please ensure cargo and cmake are in PATH."
    exit 1
}
Write-Host "  -> Cargo: $(cargo --version)" -ForegroundColor Gray
Write-Host "  -> CMake: $(cmake --version | Select-Object -First 1)" -ForegroundColor Gray

# 2. Build Native Media Engines via CMake
Write-Host "`n[2/5] Building Native Media Engines via CMake..." -ForegroundColor Yellow
if ($Clean -and (Test-Path "build")) {
    Remove-Item -Path "build" -Recurse -Force
}

if (-not (Test-Path "build")) {
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=$Configuration
}
cmake --build build --config $Configuration -j 8

# 3. Build Rust Workspace via Cargo
Write-Host "`n[3/5] Building Rust Workspace via Cargo..." -ForegroundColor Yellow
$cargoFlags = @("build", "--workspace")
if ($Configuration -eq "Release") {
    $cargoFlags += "--release"
}
& cargo @cargoFlags
if ($LASTEXITCODE -ne 0) {
    Write-Error "Cargo build failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

# 4. Stage Binaries to bin/ directory
Write-Host "`n[4/5] Staging Platform Binaries..." -ForegroundColor Yellow
if (-not (Test-Path "bin")) {
    New-Item -ItemType Directory -Path "bin" -Force | Out-Null
}

$cargoTargetDir = if (Test-Path "D:\rust\target") { "D:\rust\target" } else { "target" }
$cargoSubdir = if ($Configuration -eq "Release") { "release" } else { "debug" }
$desktopExe = Join-Path $cargoTargetDir "$cargoSubdir\mist-desktop.exe"

if (Test-Path $desktopExe) {
    Copy-Item $desktopExe "bin\mist_desktop.exe" -Force
    Copy-Item $desktopExe "bin\mist.exe" -Force
    if (Test-Path "build\apps\desktop") {
        Copy-Item $desktopExe "build\apps\desktop\mist_desktop.exe" -Force
    }
    Write-Host "  -> Staged: bin\mist_desktop.exe (Rust + Slint)" -ForegroundColor Green
    Write-Host "  -> Staged: bin\mist.exe" -ForegroundColor Green
}

$signalingExe = Join-Path $cargoTargetDir "$cargoSubdir\mist-signaling.exe"
if (Test-Path $signalingExe) {
    Copy-Item $signalingExe "bin\mist-signaling.exe" -Force
    Write-Host "  -> Staged: bin\mist-signaling.exe (Rust WebRTC Signaling)" -ForegroundColor Green
}

# Copy native executables
$hostExe = "build\apps\host\mist_host.exe"
$clientExe = "build\apps\client\mist_client.exe"
if (Test-Path $hostExe) { Copy-Item $hostExe "bin\mist_host.exe" -Force; Write-Host "  -> Staged: bin\mist_host.exe" -ForegroundColor Green }
if (Test-Path $clientExe) { Copy-Item $clientExe "bin\mist_client.exe" -Force; Write-Host "  -> Staged: bin\mist_client.exe" -ForegroundColor Green }

# 5. Run Automated Tests
if ($Test) {
    Write-Host "`n[5/5] Executing Test Suites..." -ForegroundColor Yellow
    Write-Host "Running Cargo Unit Tests..." -ForegroundColor Cyan
    cargo test --workspace
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Cargo tests failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }

    $nativeTests = "build\tests\unit\mist_unit_tests.exe"
    if (Test-Path $nativeTests) {
        Write-Host "Running Native Engine Unit Tests..." -ForegroundColor Cyan
        & $nativeTests
    }
}

# 6. Package if requested
if ($Package) {
    Write-Host "`nPackaging Distribution Installer..." -ForegroundColor Yellow
    powershell -ExecutionPolicy Bypass -File scripts\build-installer.ps1
}

Write-Host "`n=======================================================" -ForegroundColor Green
Write-Host "   MIST PLATFORM BUILD COMPLETED SUCCESSFULLY" -ForegroundColor Green
Write-Host "=======================================================" -ForegroundColor Green
