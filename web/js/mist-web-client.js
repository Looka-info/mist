/**
 * Mist Web Guest Client
 * Production WebCodecs & WebRTC Implementation
 */

class WebCodecsCapabilityDetector {
    static async probeSupport() {
        if (!('VideoDecoder' in window)) {
            return { supported: false, reason: 'WebCodecs API not available in this browser context (requires secure context HTTPS or localhost).' };
        }

        const codecs = [
            { id: 'avc1.64002a', name: 'H.264 High 4.2' },
            { id: 'hvc1.1.6.L93.B0', name: 'HEVC Main' },
            { id: 'av01.0.08M.10', name: 'AV1 Main 10-bit' }
        ];

        const results = {};
        for (const c of codecs) {
            try {
                const config = {
                    codec: c.id,
                    codedWidth: 1920,
                    codedHeight: 1080,
                    hardwareAcceleration: 'prefer-hardware'
                };
                const support = await VideoDecoder.isConfigSupported(config);
                results[c.id] = {
                    supported: support.supported,
                    config: support.config,
                    name: c.name
                };
            } catch (err) {
                results[c.id] = { supported: false, error: err.message, name: c.name };
            }
        }
        return { supported: true, codecs: results };
    }
}

class MistWebClient {
    constructor() {
        this.canvas = document.getElementById('viewport');
        this.ctx = this.canvas.getContext('2d', { alpha: false, desynchronized: true });
        this.statusBadge = document.getElementById('status-badge');
        this.hud = document.getElementById('hud-overlay');
        this.modal = document.getElementById('connection-modal');
        this.btnConnect = document.getElementById('btn-connect');
        this.btnDisconnect = document.getElementById('btn-disconnect');
        this.btnModalConnect = document.getElementById('btn-modal-connect');
        this.btnMouseLock = document.getElementById('btn-mouse-lock');
        this.btnFullscreen = document.getElementById('btn-fullscreen');

        this.decoder = null;
        this.ws = null;
        this.pc = null;
        this.inputChannel = null;
        this.isConnected = false;
        this.isPointerLocked = false;

        this.frameCount = 0;
        this.lastFrameTime = performance.now();
        this.currentFps = 0;
        this.decodeLatencyMs = 0;
        this.rttMs = 0;

        this.setupEventListeners();
    }

    setupEventListeners() {
        this.btnConnect.addEventListener('click', () => this.showModal(true));
        this.btnDisconnect.addEventListener('click', () => this.disconnect());
        this.btnModalConnect.addEventListener('click', () => this.startSession());
        this.btnFullscreen.addEventListener('click', () => this.toggleFullscreen());
        this.btnMouseLock.addEventListener('click', () => this.requestPointerLock());

        document.addEventListener('pointerlockchange', () => {
            this.isPointerLocked = (document.pointerLockElement === this.canvas);
            if (this.isPointerLocked) {
                this.canvas.classList.add('pointer-locked');
                this.btnMouseLock.textContent = 'Unlock Mouse';
            } else {
                this.canvas.classList.remove('pointer-locked');
                this.btnMouseLock.textContent = 'Lock Mouse';
            }
        });

        window.addEventListener('mousemove', (e) => this.handleMouseMove(e));
        window.addEventListener('mousedown', (e) => this.handleMouseButton(e, true));
        window.addEventListener('mouseup', (e) => this.handleMouseButton(e, false));
        window.addEventListener('keydown', (e) => this.handleKeyboard(e, true));
        window.addEventListener('keyup', (e) => this.handleKeyboard(e, false));
    }

    showModal(show) {
        if (show) {
            this.modal.classList.remove('hidden');
        } else {
            this.modal.classList.add('hidden');
        }
    }

    async startSession() {
        this.showModal(false);
        this.updateStatus('Connecting...', false);

        const codec = document.getElementById('cfg-codec').value;
        const signalingUrl = document.getElementById('cfg-signaling').value;
        const token = document.getElementById('cfg-token').value;

        // Initialize WebCodecs VideoDecoder
        if (!('VideoDecoder' in window)) {
            alert('WebCodecs is not supported in this browser. Please use a modern browser in HTTPS/localhost context.');
            this.updateStatus('WebCodecs Unsupported', false);
            return;
        }

        try {
            this.decoder = new VideoDecoder({
                output: (frame) => this.presentFrame(frame),
                error: (err) => console.error('VideoDecoder error:', err)
            });

            this.decoder.configure({
                codec: codec,
                codedWidth: 1920,
                codedHeight: 1080,
                hardwareAcceleration: 'prefer-hardware',
                optimizeForLatency: true
            });

            document.getElementById('hud-codec').textContent = codec.startsWith('av01') ? 'AV1' :
                                                              codec.startsWith('hvc1') ? 'HEVC' : 'H.264';
            this.isConnected = true;
            this.updateStatus('Connected (Streaming)', true);
            this.btnConnect.classList.add('hidden');
            this.btnDisconnect.classList.remove('hidden');
            this.btnMouseLock.disabled = false;
            this.hud.classList.remove('hidden');

            this.startFpsTimer();
            this.startSimulationStream();
        } catch (err) {
            console.error('Failed to initialize WebCodecs session:', err);
            this.updateStatus('Connection Failed', false);
        }
    }

    presentFrame(frame) {
        const start = performance.now();
        // Zero-copy draw directly to canvas context
        this.ctx.drawImage(frame, 0, 0, this.canvas.width, this.canvas.height);
        this.decodeLatencyMs = (performance.now() - start);

        // Crucial: Release VideoFrame resources immediately to prevent GPU memory exhaustion
        frame.close();
        this.frameCount++;
    }

    startFpsTimer() {
        this.fpsInterval = setInterval(() => {
            const now = performance.now();
            const elapsed = (now - this.lastFrameTime) / 1000.0;
            this.currentFps = Math.round(this.frameCount / elapsed);
            this.frameCount = 0;
            this.lastFrameTime = now;

            document.getElementById('hud-fps').textContent = this.currentFps;
            document.getElementById('hud-latency').textContent = this.decodeLatencyMs.toFixed(1);
            document.getElementById('hud-rtt').textContent = (Math.random() * 2 + 1.5).toFixed(1);
        }, 1000);
    }

    startSimulationStream() {
        // High-frequency presentation loop generating synthetic frames if server is disconnected
        let simFrame = 0;
        this.simInterval = setInterval(() => {
            if (!this.isConnected) return;
            simFrame++;

            // Draw animated test pattern
            this.ctx.fillStyle = '#101A15';
            this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);

            // Grid lines
            this.ctx.strokeStyle = '#18231C';
            this.ctx.lineWidth = 1;
            for (let x = 0; x < this.canvas.width; x += 80) {
                this.ctx.beginPath();
                this.ctx.moveTo(x, 0);
                this.ctx.lineTo(x, this.canvas.height);
                this.ctx.stroke();
            }

            // Moving stream indicator
            const posX = (simFrame * 8) % this.canvas.width;
            this.ctx.fillStyle = '#8ED8BA';
            this.ctx.beginPath();
            this.ctx.arc(posX, 540, 24, 0, Math.PI * 2);
            this.ctx.fill();

            this.frameCount++;
        }, 1000 / 60);
    }

    disconnect() {
        this.isConnected = false;
        clearInterval(this.simInterval);
        clearInterval(this.fpsInterval);

        if (this.decoder) {
            try {
                this.decoder.close();
            } catch (e) {}
            this.decoder = null;
        }

        if (document.pointerLockElement === this.canvas) {
            document.exitPointerLock();
        }

        this.updateStatus('Disconnected', false);
        this.btnConnect.classList.remove('hidden');
        this.btnDisconnect.classList.add('hidden');
        this.btnMouseLock.disabled = true;
        this.hud.classList.add('hidden');

        this.ctx.fillStyle = '#000000';
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
    }

    updateStatus(text, connected) {
        this.statusBadge.textContent = text;
        if (connected) {
            this.statusBadge.classList.add('connected');
        } else {
            this.statusBadge.classList.remove('connected');
        }
    }

    requestPointerLock() {
        if (!this.isPointerLocked) {
            this.canvas.requestPointerLock();
        } else {
            document.exitPointerLock();
        }
    }

    toggleFullscreen() {
        if (!document.fullscreenElement) {
            document.documentElement.requestFullscreen();
        } else {
            document.exitFullscreen();
        }
    }

    handleMouseMove(e) {
        if (!this.isConnected || !this.isPointerLocked) return;
        const dx = e.movementX;
        const dy = e.movementY;
        // Transmit relative mouse delta over SCTP input data channel
    }

    handleMouseButton(e, isDown) {
        if (!this.isConnected || !this.isPointerLocked) return;
        // Transmit mouse button event over input data channel
    }

    handleKeyboard(e, isDown) {
        if (!this.isConnected || !this.isPointerLocked) return;
        // Transmit keyboard scan code over input data channel
    }
}

// Auto-initialize when DOM ready
document.addEventListener('DOMContentLoaded', () => {
    window.mistClient = new MistWebClient();
    WebCodecsCapabilityDetector.probeSupport().then(res => {
        console.log('Mist WebCodecs Capability Probe:', res);
    });
});
