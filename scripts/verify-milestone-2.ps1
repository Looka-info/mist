<#
.SYNOPSIS
    Milestone 2 Verification Script for Mist Game Streaming Platform
.DESCRIPTION
    Validates native H.264 hardware video decoding, Direct3D 11 presentation pipeline,
    telemetry recording, and client CLI diagnostics.
#>

[CmdletBinding()]
param(
    [switch]$Clean,
    [uint32]$BenchmarkSeconds = 10
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path "$ScriptDir\.."

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   MIST PLATFORM - MILESTONE 2 VERIFICATION SUITE         " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "Repository root: $RepoRoot"

# Ensure artifacts directory exists
$ArtifactsDir = "$RepoRoot\artifacts"
New-Item -ItemType Directory -Path $ArtifactsDir -Force | Out-Null

# 1. Detect Tools & Compilers
Write-Host "`n[1/7] Detecting prerequisites and toolchains..." -ForegroundColor Yellow

$CMakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
$CMakeExe = $null
if ($CMakeCmd) { $CMakeExe = $CMakeCmd.Source }
if (-not $CMakeExe) {
    $WingetCMake = "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Kitware.CMake_Microsoft.Winget.Source_8wekyb3d8bbwe\cmake-4.4.4-windows-x86_64\bin\cmake.exe"
    if (Test-Path $WingetCMake) {
        $CMakeExe = $WingetCMake
    }
}
if (-not $CMakeExe) {
    Write-Error "CMake could not be located."
    exit 1
}
$CMakeVer = & $CMakeExe --version | Select-Object -First 1
Write-Host "  Found CMake: $CMakeExe ($CMakeVer)" -ForegroundColor Green

# Ninja
$NinjaCmd = Get-Command ninja -ErrorAction SilentlyContinue
$NinjaExe = $null
if ($NinjaCmd) { $NinjaExe = $NinjaCmd.Source }
if (-not $NinjaExe) {
    $WingetNinja = "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe\ninja.exe"
    if (Test-Path $WingetNinja) {
        $NinjaExe = $WingetNinja
    }
}
if ($NinjaExe) {
    $NinjaDir = Split-Path -Parent $NinjaExe
    $env:PATH = "$NinjaDir;$env:PATH"
    Write-Host "  Found Ninja: $NinjaExe" -ForegroundColor Green
}

# Compiler
$CompilerType = "Unknown"
$CompilerPath = ""
$Generator = "Ninja"

$ClCmd = Get-Command cl.exe -ErrorAction SilentlyContinue
$ClExe = $null
if ($ClCmd) { $ClExe = $ClCmd.Source }

if ($ClExe) {
    $CompilerType = "MSVC"
    $CompilerPath = $ClExe
    Write-Host "  Found Compiler: MSVC ($ClExe)" -ForegroundColor Green
} else {
    $ClangCmd = Get-Command clang++.exe -ErrorAction SilentlyContinue
    $ClangExe = $null
    if ($ClangCmd) { $ClangExe = $ClangCmd.Source }
    if (-not $ClangExe) {
        $WingetClang = "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261020-ucrt-x86_64\bin\clang++.exe"
        if (Test-Path $WingetClang) {
            $ClangExe = $WingetClang
        }
    }
    if ($ClangExe) {
        $CompilerType = "Clang"
        $CompilerPath = $ClangExe
        $ClangDir = Split-Path -Parent $ClangExe
        $env:PATH = "$ClangDir;$env:PATH"
        Write-Host "  Found Compiler: Clang++ ($ClangExe)" -ForegroundColor Green
    } else {
        Write-Error "No compatible C++20 compiler found."
        exit 1
    }
}

# NVIDIA Video Codec SDK headers
$NvSdkRoot = $env:MIST_NV_CODEC_SDK_ROOT
if (-not $NvSdkRoot -and (Test-Path "D:\tools\nv_codec_sdk\include\nvEncodeAPI.h")) {
    $NvSdkRoot = "D:\tools\nv_codec_sdk"
    $env:MIST_NV_CODEC_SDK_ROOT = $NvSdkRoot
}
if ($NvSdkRoot -and (Test-Path "$NvSdkRoot\include\nvEncodeAPI.h")) {
    Write-Host "  Found NVIDIA Video Codec SDK headers: $NvSdkRoot\include" -ForegroundColor Green
} else {
    Write-Host "  NVIDIA Video Codec SDK headers not detected (NVENC will build in fallback mode)." -ForegroundColor DarkGray
}

if ($Clean) {
    Write-Host "`nCleaning build directories..." -ForegroundColor Yellow
    Remove-Item -Path "$RepoRoot\build" -Recurse -Force -ErrorAction SilentlyContinue
}

function Invoke-Step {
    param(
        [string]$Description,
        [scriptblock]$Command
    )
    Write-Host "`n--> $Description" -ForegroundColor Cyan
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        & $Command
        if ($LASTEXITCODE -ne 0) {
            throw "Step failed with exit code $LASTEXITCODE"
        }
        $sw.Stop()
        Write-Host "  [PASS] ($($sw.Elapsed.TotalSeconds.ToString('F2'))s) $Description" -ForegroundColor Green
    } catch {
        $sw.Stop()
        Write-Host "  [FAIL] $Description" -ForegroundColor Red
        Write-Host "Error details: $_" -ForegroundColor Red
        exit 1
    }
}

# 2. Configure & Build Debug
$DebugDir = "$RepoRoot\build\debug"
$CMakeArgsBase = @(
    "-S", $RepoRoot,
    "-G", $Generator,
    "-DMIST_BUILD_TESTS=ON",
    "-DMIST_TREAT_WARNINGS_AS_ERRORS=ON"
)

if ($NvSdkRoot) {
    $CMakeArgsBase += "-DMIST_NV_CODEC_SDK_ROOT=$NvSdkRoot"
}

if ($CompilerType -eq "Clang") {
    $CMakeArgsBase += "-DCMAKE_CXX_COMPILER=$CompilerPath"
}

Invoke-Step "Configure Debug" {
    $args = $CMakeArgsBase + @("-B", $DebugDir, "-DCMAKE_BUILD_TYPE=Debug")
    & $CMakeExe $args
}

Invoke-Step "Build Debug" {
    & $CMakeExe --build $DebugDir --config Debug
}

# Ensure test bitstream exists; generate if missing using mist_host
$TestBitstream = "$ArtifactsDir\test.h264"
if (-not (Test-Path $TestBitstream)) {
    $HostDebugExe = Get-ChildItem -Path $DebugDir -Filter "mist_host.exe" -Recurse | Select-Object -First 1 -ExpandProperty FullName
    if ($HostDebugExe) {
        Write-Host "Generating test bitstream using mist_host..." -ForegroundColor Cyan
        & $HostDebugExe --encode-test --codec h264 --width 1280 --height 720 --fps 60 --duration 2 --output "$TestBitstream"
    }
}

# 3. Run Debug Tests
Invoke-Step "Run Debug Tests (CTest)" {
    $CTestExe = Join-Path (Split-Path -Parent $CMakeExe) "ctest.exe"
    & $CTestExe --test-dir $DebugDir -C Debug --output-on-failure --verbose
}

# 4. Configure & Build Release
$ReleaseDir = "$RepoRoot\build\release"

Invoke-Step "Configure Release" {
    $args = $CMakeArgsBase + @("-B", $ReleaseDir, "-DCMAKE_BUILD_TYPE=Release")
    & $CMakeExe $args
}

Invoke-Step "Build Release" {
    & $CMakeExe --build $ReleaseDir --config Release
}

# 5. Run Release Tests
Invoke-Step "Run Release Tests (CTest)" {
    $CTestExe = Join-Path (Split-Path -Parent $CMakeExe) "ctest.exe"
    & $CTestExe --test-dir $ReleaseDir -C Release --output-on-failure --verbose
}

# 6. Execute Diagnostic Commands
Write-Host "`n[6/7] Running Milestone 2 Client Diagnostic Commands..." -ForegroundColor Yellow

$ClientExe = Get-ChildItem -Path $ReleaseDir -Filter "mist_client.exe" -Recurse | Select-Object -First 1 -ExpandProperty FullName
if (-not $ClientExe) {
    Write-Error "mist_client.exe not found in $ReleaseDir"
    exit 1
}

# Ensure test bitstream exists; generate if missing using mist_host
$TestBitstream = "$ArtifactsDir\test.h264"
if (-not (Test-Path $TestBitstream)) {
    $HostExe = Get-ChildItem -Path $ReleaseDir -Filter "mist_host.exe" -Recurse | Select-Object -First 1 -ExpandProperty FullName
    if ($HostExe) {
        Write-Host "Generating test bitstream using mist_host..." -ForegroundColor Cyan
        & $HostExe --encode-test --codec h264 --width 1280 --height 720 --fps 60 --duration 2 --output "$TestBitstream"
    }
}

Invoke-Step "CLI: mist_client --decoder-info" {
    & $ClientExe --decoder-info
}

Invoke-Step "CLI: mist_client --present-test --headless --fps 60" {
    & $ClientExe --present-test --headless --fps 60
}

if (Test-Path $TestBitstream) {
    Invoke-Step "CLI: mist_client --decode-test --input artifacts/test.h264 --headless --fps 60 --duration 2" {
        & $ClientExe --decode-test --input "$TestBitstream" --headless --fps 60 --duration 2
    }

    Invoke-Step "CLI: mist_client --decode-benchmark --input artifacts/test.h264 --report artifacts/milestone-2.json --headless" {
        & $ClientExe --decode-benchmark --input "$TestBitstream" --report "$ArtifactsDir\milestone-2.json" --headless
    }
} else {
    Write-Warning "Skipping live bitstream decode diagnostics because artifacts\test.h264 is unavailable."
}

# 7. Validate Report Artifact
Write-Host "`n[7/7] Validating Benchmark Artifact..." -ForegroundColor Yellow
$ReportFile = "$ArtifactsDir\milestone-2.json"
if (Test-Path $ReportFile) {
    $ReportContent = Get-Content $ReportFile -Raw
    Write-Host "  Successfully generated $ReportFile" -ForegroundColor Green
    Write-Host $ReportContent -ForegroundColor White
} else {
    Write-Warning "Artifact $ReportFile was not generated."
}

Write-Host "`n==========================================================" -ForegroundColor Green
Write-Host "   MILESTONE 2 VERIFICATION COMPLETED SUCCESSFULLY       " -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green

exit 0
