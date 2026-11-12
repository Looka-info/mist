#!/usr/bin/env bash
set -euo pipefail

echo "=================================================="
echo "    Mist Signaling Service - Linux VPS Deployer   "
echo "=================================================="

# Check for Docker first
if command -v docker &> /dev/null && command -v docker-compose &> /dev/null || docker compose version &> /dev/null; then
    echo "[+] Docker detected. Building and starting with Docker Compose..."
    if docker compose version &> /dev/null; then
        docker compose up -d --build
    else
        docker-compose up -d --build
    fi
    echo "[✓] Mist Signaling service running in background on port 8080!"
    exit 0
fi

# Fallback: Native systemd installation
echo "[*] Docker not found. Proceeding with native systemd installation..."

# Install Rust toolchain if not present
if ! command -v cargo &> /dev/null; then
    echo "[+] Installing Rust toolchain..."
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
    source "$HOME/.cargo/env"
fi

# Build native release binary
echo "[+] Compiling mist-signaling binary..."
cd "$(dirname "$0")/signaling"
cargo build --release
cd ..

# Install to /opt/mist-signaling
echo "[+] Installing binary to /opt/mist-signaling/..."
mkdir -p /opt/mist-signaling
cp signaling/target/release/mist-signaling /opt/mist-signaling/mist-signaling
chmod +x /opt/mist-signaling/mist-signaling

# Setup systemd service
echo "[+] Registering systemd service..."
cp mist-signaling.service /etc/systemd/system/mist-signaling.service
systemctl daemon-reload
systemctl enable --now mist-signaling

echo "[✓] Deployment complete! Status:"
systemctl status mist-signaling --no-pager
