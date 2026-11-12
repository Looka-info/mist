# Mist Signaling Service - Deployment for akat-suki.site

This package is pre-configured for your domain: **`signaling.akat-suki.site`**.

---

## 1. Add DNS Record
In your domain registrar (Cloudflare, Namecheap, GoDaddy, Hostinger, etc.):
- **Type:** `A`
- **Name / Subdomain:** `signaling`
- **Value / IP Address:** `YOUR_GERMAN_VPS_IP`
- **Proxy Status:** If using Cloudflare, set to **DNS Only** (Grey Cloud) or ensure WebSockets are enabled.

---

## 2. Deploy on Your VPS

Upload `mist-signaling-vps.zip` to your VPS, extract, and run:
```bash
unzip mist-signaling-vps.zip -d mist-signaling
cd mist-signaling
sudo bash deploy.sh
```

---

## 3. Enable Free SSL via Caddy (1 Minute)

```bash
# Install Caddy
sudo apt install -y caddy

# Apply pre-configured Caddyfile
sudo cp Caddyfile /etc/caddy/Caddyfile
sudo systemctl reload caddy
```
Caddy will automatically generate a free Let's Encrypt SSL certificate for `signaling.akat-suki.site`.

---

## 4. Connect in Mist Desktop

In your Mist app settings, enter:
```
wss://signaling.akat-suki.site
```
Both you and your friends can now connect worldwide!
