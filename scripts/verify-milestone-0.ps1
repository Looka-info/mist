<#
.SYNOPSIS
    Milestone 0 Verification Script for Mist Game Streaming Platform
.DESCRIPTION
    Validates prerequisites, compiles Debug and Release configurations,
    runs automated unit tests, and verifies host/client startup smoke tests.
#>

[CmdletBinding()]
param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path "$ScriptDir\.."

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   MIST PLATFORM - MILESTONE 0 VERIFICATION SUITE         " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "Repository root: $RepoRoot"

# 1. Detect Tools & Compilers
Write-Host "`n[1/6] Detecting prerequisites and toolchains..." -ForegroundColor Yellow

# CMake detection
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
    Write-Error "CMake could not be located. Please install CMake 3.25+."
    exit 1
}
$CMakeVer = & $CMakeExe --version | Select-Object -First 1
Write-Host "  Found CMake: $CMakeExe ($CMakeVer)" -ForegroundColor Green

# Ninja detection
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

# Compiler detection (Prefer MSVC cl.exe, fallback to Clang++)
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
        Write-Error "No compatible C++20 compiler (MSVC or Clang) found."
        exit 1
    }
}

# Check vcpkg
$VcpkgCmd = Get-Command vcpkg -ErrorAction SilentlyContinue
$VcpkgExe = $null
if ($VcpkgCmd) { $VcpkgExe = $VcpkgCmd.Source }
if (-not $VcpkgExe) {
    if (Test-Path "D:\tools\vcpkg\vcpkg.exe") {
        $VcpkgExe = "D:\tools\vcpkg\vcpkg.exe"
    } elseif (Test-Path "C:\vcpkg\vcpkg.exe") {
        $VcpkgExe = "C:\vcpkg\vcpkg.exe"
    }
}
if ($VcpkgExe) {
    Write-Host "  Found vcpkg: $VcpkgExe" -ForegroundColor Green
} else {
    Write-Host "  vcpkg not detected in standard paths; CMake will use FetchContent fallback for dependencies." -ForegroundColor DarkGray
}

if ($Clean) {
    Write-Host "`nCleaning build directories..." -ForegroundColor Yellow
    Remove-Item -Path "$RepoRoot\build" -Recurse -Force -ErrorAction SilentlyContinue
}

# Helper for executing commands
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

# 6. Application Startup Smoke Tests
Write-Host "`n[6/6] Executing Host and Client Application Smoke Tests..." -ForegroundColor Yellow

$HostExe = Get-ChildItem -Path $ReleaseDir -Filter "mist_host.exe" -Recurse | Select-Object -First 1 -ExpandProperty FullName
$ClientExe = Get-ChildItem -Path $ReleaseDir -Filter "mist_client.exe" -Recurse | Select-Object -First 1 -ExpandProperty FullName

if (-not $HostExe -or -not (Test-Path $HostExe)) {
    Write-Error "Could not locate mist_host.exe in $ReleaseDir"
    exit 1
}
if (-not $ClientExe -or -not (Test-Path $ClientExe)) {
    Write-Error "Could not locate mist_client.exe in $ReleaseDir"
    exit 1
}

Invoke-Step "Smoke Test: mist_host --version" {
    & $HostExe --version
}

Invoke-Step "Smoke Test: mist_host --self-test" {
    & $HostExe --self-test
}

Invoke-Step "Smoke Test: mist_client --version" {
    & $ClientExe --version
}

Invoke-Step "Smoke Test: mist_client --self-test" {
    & $ClientExe --self-test
}

Write-Host "`n==========================================================" -ForegroundColor Green
Write-Host "   MILESTONE 0 VERIFICATION COMPLETED SUCCESSFULLY       " -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
Write-Host "  Compiler:       $CompilerType ($CompilerPath)" -ForegroundColor White
Write-Host "  Debug Build:    PASSED" -ForegroundColor White
Write-Host "  Debug Tests:    PASSED" -ForegroundColor White
Write-Host "  Release Build:  PASSED" -ForegroundColor White
Write-Host "  Release Tests:  PASSED" -ForegroundColor White
Write-Host "  Host Smoke:     PASSED" -ForegroundColor White
Write-Host "  Client Smoke:   PASSED" -ForegroundColor White
Write-Host "==========================================================" -ForegroundColor Green

exit 0
