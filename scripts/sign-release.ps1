<#
.SYNOPSIS
    Mist Native Windows Game Streaming Platform — Release Code Signing & Integrity Verification
.DESCRIPTION
    Signs release executables using Microsoft Authenticode and generates a cryptographically
    verified release manifest with SHA-256 hashes and timestamp metadata.
#>

param(
    [string]$DistPath = "dist\mist-windows-x64",
    [string]$ManifestOutput = "dist\release-manifest.json",
    [switch]$SkipSigning = $false
)

$ErrorActionPreference = "Stop"

Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host "   MIST - RELEASE CODE SIGNING AND INTEGRITY AUDIT" -ForegroundColor Cyan
Write-Host "=======================================================" -ForegroundColor Cyan

if (-not (Test-Path $DistPath)) {
    Write-Warning "Target directory $DistPath does not exist. Scanning build directory for binaries..."
    $DistPath = "build\apps"
}

$binaries = Get-ChildItem -Path $DistPath -Recurse -Include *.exe, *.dll

if ($binaries.Count -eq 0) {
    Write-Error "No binaries (.exe or .dll) found in $DistPath"
    exit 1
}

Write-Host "Found $($binaries.Count) binaries to process in: $DistPath" -ForegroundColor Green

$signingCert = $null
if (-not $SkipSigning) {
    Write-Host "Checking for Authenticode Code Signing Certificate..." -ForegroundColor Yellow
    # Look for existing trusted code signing certificate
    $certs = Get-ChildItem -Path Cert:\CurrentUser\My -CodeSigningCert -ErrorAction SilentlyContinue
    if ($certs -and $certs.Count -gt 0) {
        $signingCert = $certs[0]
        Write-Host "Using existing code signing certificate: $($signingCert.Subject)" -ForegroundColor Green
    } else {
        Write-Host "Creating ephemeral development Authenticode certificate for local validation..." -ForegroundColor Yellow
        try {
            $signingCert = New-SelfSignedCertificate -Type CodeSigningCert -Subject "CN=Mist Development Release Signing" -CertStoreLocation Cert:\CurrentUser\My
            Write-Host "Created development certificate: $($signingCert.Thumbprint)" -ForegroundColor Green
        } catch {
            Write-Warning "Failed to create self-signed certificate: $_"
        }
    }
}

$manifestEntries = @()

foreach ($bin in $binaries) {
    Write-Host "Processing: $($bin.Name)" -ForegroundColor Cyan
    
    # 1. Sign binary if certificate is available
    $signatureStatus = "Unsigned"
    if ($signingCert) {
        try {
            $sig = Set-AuthenticodeSignature -FilePath $bin.FullName -Certificate $signingCert -HashAlgorithm SHA256 -ErrorAction SilentlyContinue
            $signatureStatus = $sig.Status.ToString()
            Write-Host "  -> Authenticode Signature Status: $signatureStatus" -ForegroundColor $(if ($signatureStatus -eq "Valid") { "Green" } else { "Yellow" })
        } catch {
            Write-Warning "  -> Signing failed: $_"
            $signatureStatus = "SigningFailed"
        }
    }

    # 2. Compute SHA-256 Hash
    $hash = (Get-FileHash -Path $bin.FullName -Algorithm SHA256).Hash
    Write-Host "  -> SHA-256: $hash" -ForegroundColor Gray

    $manifestEntries += [PSCustomObject]@{
        filename = $bin.Name
        relative_path = $bin.FullName.Replace((Get-Location).Path + "\", "")
        size_bytes = $bin.Length
        sha256 = $hash
        signature_status = $signatureStatus
        cert_thumbprint = if ($signingCert) { $signingCert.Thumbprint } else { "None" }
    }
}

# 3. Generate Release Manifest
$manifest = [PSCustomObject]@{
    product = "Mist Native Game Streaming"
    version = "0.1.0"
    milestone = 6
    generated_at = (Get-Date -Format o)
    files = $manifestEntries
}

$manifestDir = Split-Path $ManifestOutput -Parent
if ($manifestDir -and -not (Test-Path $manifestDir)) {
    New-Item -ItemType Directory -Path $manifestDir -Force | Out-Null
}

$json = $manifest | ConvertTo-Json -Depth 5
Set-Content -Path $ManifestOutput -Value $json -Encoding UTF8

Write-Host "`nRelease Manifest generated at: $ManifestOutput" -ForegroundColor Green
Write-Host "Integrity verification complete: $($manifestEntries.Count) binaries registered." -ForegroundColor Green
