/**
 * Mist Web Guest Client
 * Production WebRTC & WebCodecs Implementation
 */

class WebCodecsCapabilityDetector {
    static async probeSupport() {
        if (!('VideoDecoder' in window)) {
            return {
                supported: false,
                reason: 'WebCodecs API not available in this browser context (requires secure context HTTPS or localhost).'
            };
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
        this.videoElement = document.getElementById('remote-video');
        this.canvas = document.getElementById('viewport');
        this.ctx = this.canvas ? this.canvas.getContext('2d', { alpha: false, desynchronized: true }) : null;
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
        this.statsInterval = null;
        this.isConnected = false;
        this.isPointerLocked = false;
        this.sessionId = 'mist-session-1';
        this.guestId = 'web-guest-1';
        this.inputSequence = 0;

        this.setupEventListeners();
    }

    setupEventListeners() {
        this.btnConnect.addEventListener('click', () => this.showModal(true));
        this.btnDisconnect.addEventListener('click', () => this.disconnect());
        this.btnModalConnect.addEventListener('click', () => this.startSession());
        this.btnFullscreen.addEventListener('click', () => this.toggleFullscreen());
        this.btnMouseLock.addEventListener('click', () => this.requestPointerLock());

        document.addEventListener('pointerlockchange', () => {
            const activeElement = document.pointerLockElement;
            this.isPointerLocked = (activeElement === this.videoElement || activeElement === this.canvas);
            if (this.isPointerLocked) {
                if (this.videoElement) this.videoElement.classList.add('pointer-locked');
                this.btnMouseLock.textContent = 'Unlock Mouse';
            } else {
                if (this.videoElement) this.videoElement.classList.remove('pointer-locked');
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

    updateStatus(text, connected) {
        this.statusBadge.textContent = text;
        if (connected) {
            this.statusBadge.classList.add('connected');
        } else {
            this.statusBadge.classList.remove('connected');
        }
    }

    async startSession() {
        this.showModal(false);

        const signalingUrl = document.getElementById('cfg-signaling').value.trim();
        const token = document.getElementById('cfg-token').value.trim();
        const sessionIdInput = document.getElementById('cfg-session-id');
        const guestIdInput = document.getElementById('cfg-guest-id');

        this.sessionId = (sessionIdInput && sessionIdInput.value.trim()) || 'mist-session-1';
        this.guestId = (guestIdInput && guestIdInput.value.trim()) || ('guest-' + Math.random().toString(36).substring(2, 8));

        this.updateStatus('Connecting to Signaling...', false);

        try {
            await this.connectSignaling(signalingUrl, token);
        } catch (err) {
            console.error('Failed to initiate session:', err);
            this.updateStatus('Connection Failed', false);
        }
    }

    connectSignaling(url, token) {
        return new Promise((resolve, reject) => {
            try {
                this.ws = new WebSocket(url);
            } catch (err) {
                reject(err);
                return;
            }

            this.ws.onopen = () => {
                this.updateStatus('Signaling Connected. Registering...', false);
                const regMsg = {
                    type: 'register',
                    session_id: this.sessionId,
                    role: 'guest',
                    token: token,
                    guest_id: this.guestId
                };
                this.ws.send(JSON.stringify(regMsg));
                this.initPeerConnection();
                resolve();
            };

            this.ws.onmessage = async (event) => {
                let msg;
                try {
                    msg = JSON.parse(event.data);
                } catch (e) {
                    return;
                }
                await this.handleSignalingMessage(msg);
            };

            this.ws.onerror = (err) => {
                console.warn('Signaling WebSocket error:', err);
                if (!this.isConnected) {
                    this.updateStatus('Signaling Error', false);
                    reject(err);
                }
            };

            this.ws.onclose = () => {
                if (this.isConnected) {
                    console.log('Signaling WebSocket closed while streaming');
                } else {
                    this.updateStatus('Signaling Closed', false);
                }
            };
        });
    }

    initPeerConnection() {
        const iceServers = [
            { urls: 'stun:stun.l.google.com:19302' },
            { urls: 'stun:stun1.l.google.com:19302' }
        ];

        this.pc = new RTCPeerConnection({ iceServers });

        this.pc.onicecandidate = (event) => {
            if (event.candidate && this.ws && this.ws.readyState === WebSocket.OPEN) {
                this.ws.send(JSON.stringify({
                    type: 'candidate',
                    session_id: this.sessionId,
                    candidate: event.candidate.candidate,
                    mid: event.candidate.sdpMid,
                    guest_id: this.guestId
                }));
            }
        };

        this.pc.ontrack = (event) => {
            console.log('Received remote media track:', event.track.kind);
            if (event.streams && event.streams[0]) {
                if (this.videoElement) {
                    this.videoElement.style.display = 'block';
                    this.videoElement.srcObject = event.streams[0];
                    this.videoElement.play().catch(e => console.warn('Video play interrupted:', e));
                }
                if (this.canvas) {
                    this.canvas.style.display = 'none';
                }
            }
        };

        this.pc.onconnectionstatechange = () => {
            const state = this.pc.connectionState;
            console.log('PeerConnection connectionState:', state);
            if (state === 'connected') {
                this.isConnected = true;
                this.updateStatus('Connected (Streaming)', true);
                this.btnConnect.classList.add('hidden');
                this.btnDisconnect.classList.remove('hidden');
                this.btnMouseLock.disabled = false;
                this.hud.classList.remove('hidden');
                this.startStatsMonitoring();
            } else if (state === 'connecting') {
                this.updateStatus('Negotiating Media...', false);
            } else if (state === 'disconnected') {
                this.updateStatus('Media Disconnected', false);
            } else if (state === 'failed') {
                this.updateStatus('ICE Connection Failed', false);
                this.disconnect();
            } else if (state === 'closed') {
                this.updateStatus('Disconnected', false);
            }
        };

        // Create unreliable DataChannel for high-frequency input
        try {
            this.inputChannel = this.pc.createDataChannel('mist-input', {
                ordered: false,
                maxRetransmits: 0
            });
            this.inputChannel.binaryType = 'arraybuffer';
            this.inputChannel.onopen = () => console.log('Input DataChannel opened');
            this.inputChannel.onclose = () => console.log('Input DataChannel closed');
        } catch (e) {
            console.warn('Could not create DataChannel:', e);
        }
    }

    async handleSignalingMessage(msg) {
        switch (msg.type) {
            case 'registered':
                this.updateStatus('Registered. Waiting for host offer...', false);
                break;

            case 'offer':
                this.updateStatus('Offer received. Negotiating answer...', false);
                try {
                    await this.pc.setRemoteDescription(new RTCSessionDescription({
                        type: 'offer',
                        sdp: msg.sdp
                    }));
                    const answer = await this.pc.createAnswer();
                    await this.pc.setLocalDescription(answer);

                    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
                        this.ws.send(JSON.stringify({
                            type: 'answer',
                            session_id: this.sessionId,
                            sdp: answer.sdp,
                            guest_id: this.guestId
                        }));
                    }
                } catch (err) {
                    console.error('Error during offer/answer exchange:', err);
                    this.updateStatus('Negotiation Failed', false);
                }
                break;

            case 'candidate':
                if (msg.candidate && this.pc) {
                    try {
                        await this.pc.addIceCandidate(new RTCIceCandidate({
                            candidate: msg.candidate,
                            sdpMid: msg.mid
                        }));
                    } catch (err) {
                        console.warn('Failed to add received ICE candidate:', err);
                    }
                }
                break;

            case 'peer_left':
            case 'bye':
                console.log('Host disconnected from session');
                this.updateStatus('Host Disconnected', false);
                this.disconnect();
                break;

            case 'error':
                console.error('Signaling error:', msg.message);
                this.updateStatus('Signaling: ' + msg.message, false);
                break;
        }
    }

    startStatsMonitoring() {
        if (this.statsInterval) clearInterval(this.statsInterval);

        this.statsInterval = setInterval(async () => {
            if (!this.pc || this.pc.connectionState !== 'connected') return;

            try {
                const stats = await this.pc.getStats();
                let rttMs = 0;
                let fps = 0;
                let packetsLost = 0;
                let totalPackets = 0;
                let jitterMs = 0;
                let codecName = 'H.264';

                stats.forEach(report => {
                    if (report.type === 'candidate-pair' && report.state === 'succeeded') {
                        if (report.currentRoundTripTime !== undefined) {
                            rttMs = report.currentRoundTripTime * 1000.0;
                        }
                    }
                    if (report.type === 'inbound-rtp' && report.kind === 'video') {
                        if (report.framesPerSecond !== undefined) {
                            fps = Math.round(report.framesPerSecond);
                        }
                        if (report.packetsLost !== undefined) {
                            packetsLost = report.packetsLost;
                        }
                        if (report.packetsReceived !== undefined) {
                            totalPackets = report.packetsReceived;
                        }
                        if (report.jitter !== undefined) {
                            jitterMs = report.jitter * 1000.0;
                        }
                        if (report.decoderImplementation) {
                            codecName = report.decoderImplementation;
                        }
                    }
                });

                const lossPct = (totalPackets > 0)
                    ? ((packetsLost / (totalPackets + packetsLost)) * 100.0).toFixed(1)
                    : '0.0';

                const elFps = document.getElementById('hud-fps');
                const elLat = document.getElementById('hud-latency');
                const elRtt = document.getElementById('hud-rtt');
                const elLoss = document.getElementById('hud-loss');
                const elCodec = document.getElementById('hud-codec');

                if (elFps) elFps.textContent = fps;
                if (elLat) elLat.textContent = jitterMs.toFixed(1);
                if (elRtt) elRtt.textContent = rttMs.toFixed(1);
                if (elLoss) elLoss.textContent = lossPct + '%';
                if (elCodec) elCodec.textContent = codecName;
            } catch (err) {
                console.warn('Error reading WebRTC statistics:', err);
            }
        }, 1000);
    }

    disconnect() {
        this.isConnected = false;

        if (this.statsInterval) {
            clearInterval(this.statsInterval);
            this.statsInterval = null;
        }

        if (this.inputChannel) {
            try { this.inputChannel.close(); } catch (e) {}
            this.inputChannel = null;
        }

        if (this.pc) {
            try { this.pc.close(); } catch (e) {}
            this.pc = null;
        }

        if (this.ws) {
            if (this.ws.readyState === WebSocket.OPEN) {
                try {
                    this.ws.send(JSON.stringify({
                        type: 'bye',
                        session_id: this.sessionId,
                        guest_id: this.guestId
                    }));
                } catch (e) {}
            }
            try { this.ws.close(); } catch (e) {}
            this.ws = null;
        }

        if (this.decoder) {
            try { this.decoder.close(); } catch (e) {}
            this.decoder = null;
        }

        if (this.videoElement) {
            this.videoElement.srcObject = null;
        }

        if (document.pointerLockElement) {
            try { document.exitPointerLock(); } catch (e) {}
        }

        this.updateStatus('Disconnected', false);
        this.btnConnect.classList.remove('hidden');
        this.btnDisconnect.classList.add('hidden');
        this.btnMouseLock.disabled = true;
        this.hud.classList.add('hidden');
    }

    requestPointerLock() {
        const target = this.videoElement || this.canvas;
        if (!this.isPointerLocked && target) {
            target.requestPointerLock();
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

    sendInputPacket(type, payload) {
        if (!this.inputChannel || this.inputChannel.readyState !== 'open') return;

        try {
            const data = JSON.stringify({
                type: type,
                seq: ++this.inputSequence,
                ts: performance.now(),
                ...payload
            });
            this.inputChannel.send(data);
        } catch (err) {
            console.warn('Failed to transmit input packet:', err);
        }
    }

    handleMouseMove(e) {
        if (!this.isConnected || !this.isPointerLocked) return;
        this.sendInputPacket('mouse_move', {
            dx: e.movementX,
            dy: e.movementY,
            isAbsolute: false
        });
    }

    handleMouseButton(e, isDown) {
        if (!this.isConnected || !this.isPointerLocked) return;
        this.sendInputPacket('mouse_button', {
            button: e.button,
            isDown: isDown
        });
    }

    handleKeyboard(e, isDown) {
        if (!this.isConnected || !this.isPointerLocked) return;
        this.sendInputPacket('keyboard', {
            key: e.key,
            code: e.code,
            isDown: isDown
        });
    }
}

// Auto-initialize when DOM ready
document.addEventListener('DOMContentLoaded', () => {
    window.mistClient = new MistWebClient();
    WebCodecsCapabilityDetector.probeSupport().then(res => {
        console.log('Mist WebCodecs Capability Probe:', res);
    });
});
