param(
    [string]$SignalingUrl = "ws://127.0.0.1:8089",
    [string]$SessionId = "live-session-m29",
    [string]$AuthToken = "token-m29"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path "artifacts")) {
    New-Item -ItemType Directory -Path "artifacts" -Force | Out-Null
}

Write-Host "Starting Host streaming session ($SessionId)..." -ForegroundColor Cyan
$hostProc = Start-Process -FilePath "build\apps\host\mist_host.exe" `
    -ArgumentList "--stream-test", "--signaling-url", $SignalingUrl, "--session-id", $SessionId, "--auth-token", $AuthToken, "--duration", "7", "--allow-remote-input" `
    -PassThru -RedirectStandardOutput "artifacts\host_stream.log" -RedirectStandardError "artifacts\host_stream_err.log"

Start-Sleep -Milliseconds 1500

Write-Host "Starting Client receive session ($SessionId)..." -ForegroundColor Cyan
$clientProc = Start-Process -FilePath "build\apps\client\mist_client.exe" `
    -ArgumentList "--receive-test", "--signaling-url", $SignalingUrl, "--session-id", $SessionId, "--auth-token", $AuthToken, "--duration", "5", "--headless", "--enable-input", "--report", "artifacts\client_stream_report.json" `
    -PassThru -RedirectStandardOutput "artifacts\client_stream.log" -RedirectStandardError "artifacts\client_stream_err.log"

$clientExited = $clientProc.WaitForExit(10000)
$hostExited = $hostProc.WaitForExit(10000)

Write-Host "Client Process Exited: $clientExited (ExitCode: $($clientProc.ExitCode))"
Write-Host "Host Process Exited: $hostExited (ExitCode: $($hostProc.ExitCode))"

if (Test-Path "artifacts\client_stream.log") {
    Write-Host "`n--- Client Log ---" -ForegroundColor Green
    Get-Content "artifacts\client_stream.log"
}

if (Test-Path "artifacts\host_stream.log") {
    Write-Host "`n--- Host Log ---" -ForegroundColor Green
    Get-Content "artifacts\host_stream.log"
}

if (Test-Path "artifacts\client_stream_report.json") {
    Write-Host "`n--- Client Benchmark Report ---" -ForegroundColor Yellow
    Get-Content "artifacts\client_stream_report.json"
}
