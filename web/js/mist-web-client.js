/**
 * Mist Web Guest Client
 * Production WebRTC & WebCodecs Implementation
 */

class WebCodecsCapabilityDetector {
    static async probeSupport() {
        if (typeof window === 'undefined' || !('VideoDecoder' in window)) {
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
        if (typeof document !== 'undefined') {
            this.canvas = document.getElementById('viewport');
            this.ctx = this.canvas ? this.canvas.getContext('2d', { alpha: false, desynchronized: true }) : null;
            this.videoEl = document.getElementById('remote-video');
            this.statusBadge = document.getElementById('status-badge');
            this.hud = document.getElementById('hud-overlay');
            this.modal = document.getElementById('connection-modal');
            this.btnConnect = document.getElementById('btn-connect');
            this.btnDisconnect = document.getElementById('btn-disconnect');
            this.btnModalConnect = document.getElementById('btn-modal-connect');
            this.btnMouseLock = document.getElementById('btn-mouse-lock');
            this.btnFullscreen = document.getElementById('btn-fullscreen');
            this.setupEventListeners();
        }

        this.ws = null;
        this.pc = null;
        this.inputChannel = null;
        this.statsInterval = null;
        this.isConnected = false;
        this.isPointerLocked = false;

        this.sessionId = 'default-session';
        this.guestId = 'guest-' + Math.random().toString(36).substring(2, 9);
        this.renderFrameId = null;
        this.inputSequence = 0;
        this.iceCandidateQueue = [];

        this.stats = {
            fps: 0,
            decodeLatencyMs: 0,
            rttMs: 0,
            packetLossPct: 0,
            activeCodec: 'None'
        };

        this.lastStatsTimestamp = 0;
        this.lastBytesReceived = 0;
        this.lastFramesDecoded = 0;
    }

    setupEventListeners() {
        if (this.btnConnect) this.btnConnect.addEventListener('click', () => this.showModal(true));
        if (this.btnDisconnect) this.btnDisconnect.addEventListener('click', () => this.disconnect());
        if (this.btnModalConnect) this.btnModalConnect.addEventListener('click', () => this.startSession());
        if (this.btnFullscreen) this.btnFullscreen.addEventListener('click', () => this.toggleFullscreen());
        if (this.btnMouseLock) this.btnMouseLock.addEventListener('click', () => this.requestPointerLock());

        document.addEventListener('pointerlockchange', () => {
            this.isPointerLocked = (document.pointerLockElement === this.canvas);
            if (this.btnMouseLock) {
                if (this.isPointerLocked) {
                    if (this.canvas) this.canvas.classList.add('pointer-locked');
                    this.btnMouseLock.textContent = 'Unlock Mouse';
                } else {
                    if (this.canvas) this.canvas.classList.remove('pointer-locked');
                    this.btnMouseLock.textContent = 'Lock Mouse';
                }
            }
        });

        window.addEventListener('mousemove', (e) => this.handleMouseMove(e));
        window.addEventListener('mousedown', (e) => this.handleMouseButton(e, true));
        window.addEventListener('mouseup', (e) => this.handleMouseButton(e, false));
        window.addEventListener('keydown', (e) => this.handleKeyboard(e, true));
        window.addEventListener('keyup', (e) => this.handleKeyboard(e, false));
        window.addEventListener('wheel', (e) => this.handleMouseWheel(e), { passive: true });
    }

    showModal(show) {
        if (!this.modal) return;
        if (show) {
            this.modal.classList.remove('hidden');
        } else {
            this.modal.classList.add('hidden');
        }
    }

    async startSession() {
        this.showModal(false);
        this.updateStatus('Connecting to Signaling...', false);

        const signalingInput = document.getElementById('cfg-signaling');
        const tokenInput = document.getElementById('cfg-token');
        const codecSelect = document.getElementById('cfg-codec');

        const signalingUrl = signalingInput ? signalingInput.value : 'ws://127.0.0.1:8080';
        const token = tokenInput ? tokenInput.value : 'mist-dev-token';
        const preferredCodec = codecSelect ? codecSelect.value : 'avc1.64002a';

        try {
            await this.initWebRtc(preferredCodec);
            this.connectSignaling(signalingUrl, token);
        } catch (err) {
            console.error('Failed to start WebRTC session:', err);
            this.updateStatus('Connection Failed: ' + err.message, false);
            this.disconnect();
        }
    }

    async initWebRtc(preferredCodec) {
        const iceServers = [
            { urls: 'stun:stun.l.google.com:19302' },
            { urls: 'stun:stun1.l.google.com:19302' }
        ];

        this.iceCandidateQueue = [];
        this.pc = new RTCPeerConnection({ iceServers });

        this.pc.addTransceiver('video', { direction: 'recvonly' });
        this.pc.addTransceiver('audio', { direction: 'recvonly' });

        this.pc.ontrack = (event) => {
            console.log('Received remote media track:', event.track.kind);
            if (event.track.kind === 'video' && this.videoEl) {
                this.videoEl.srcObject = event.streams[0];
                this.videoEl.play().catch(e => console.warn('Video auto-play interrupted:', e));
                this.startRenderLoop();
            }
        };

        this.inputChannel = this.pc.createDataChannel('mist-input', {
            ordered: false,
            maxRetransmits: 0
        });

        this.inputChannel.binaryType = 'arraybuffer';
        this.inputChannel.onopen = () => console.log('SCTP input data channel opened');
        this.inputChannel.onclose = () => console.log('SCTP input data channel closed');

        this.pc.ondatachannel = (event) => {
            if (event.channel.label === 'mist-input') {
                this.inputChannel = event.channel;
                this.inputChannel.binaryType = 'arraybuffer';
            }
        };

        this.pc.onicecandidate = (event) => {
            if (event.candidate && this.ws && this.ws.readyState === WebSocket.OPEN) {
                const candidateMsg = {
                    type: 'candidate',
                    session_id: this.sessionId,
                    candidate: event.candidate.candidate,
                    mid: event.candidate.sdpMid,
                    guest_id: this.guestId
                };
                this.ws.send(JSON.stringify(candidateMsg));
            }
        };

        this.pc.onconnectionstatechange = () => {
            console.log('WebRTC connection state changed:', this.pc.connectionState);
            if (this.pc.connectionState === 'connected') {
                this.isConnected = true;
                this.updateStatus('Connected (Streaming)', true);
                if (this.btnConnect) this.btnConnect.classList.add('hidden');
                if (this.btnDisconnect) this.btnDisconnect.classList.remove('hidden');
                if (this.btnMouseLock) this.btnMouseLock.disabled = false;
                if (this.hud) this.hud.classList.remove('hidden');
                this.startTelemetryLoop();
            } else if (this.pc.connectionState === 'failed' || this.pc.connectionState === 'disconnected') {
                this.updateStatus('Stream Interrupted (' + this.pc.connectionState + ')', false);
            }
        };
    }

    connectSignaling(url, token) {
        this.ws = new WebSocket(url);

        this.ws.onopen = () => {
            console.log('Connected to Mist signaling server:', url);
            this.updateStatus('Registering with Host...', false);
            const registerMsg = {
                type: 'register',
                session_id: this.sessionId,
                role: 'guest',
                token: token,
                guest_id: this.guestId
            };
            this.ws.send(JSON.stringify(registerMsg));
        };

        this.ws.onmessage = async (event) => {
            try {
                const msg = JSON.parse(event.data);
                await this.handleSignalingMessage(msg);
            } catch (err) {
                console.error('Failed to handle signaling message:', err, event.data);
            }
        };

        this.ws.onerror = (err) => {
            console.error('Signaling WebSocket error:', err);
            this.updateStatus('Signaling Error', false);
        };

        this.ws.onclose = () => {
            console.log('Signaling WebSocket closed');
            if (this.isConnected) {
                this.updateStatus('Signaling Disconnected', false);
            }
        };
    }

    async handleSignalingMessage(msg) {
        if (!msg || !msg.type) return;

        switch (msg.type) {
            case 'registered':
                console.log('Registered with signaling server. Peer connected:', msg.peer_connected);
                this.updateStatus(msg.peer_connected ? 'Peer Ready' : 'Waiting for Host...', false);
                break;

            case 'peer_joined':
                console.log('Peer joined session room:', msg.peer_role, msg.guest_id);
                if (msg.peer_role === 'host') {
                    this.updateStatus('Host Connected. Awaiting Offer...', false);
                }
                break;

            case 'offer':
                console.log('Received SDP offer from host');
                await this.pc.setRemoteDescription(new RTCSessionDescription({ type: 'offer', sdp: msg.sdp }));

                // Drain any queued ICE candidates received prior to setRemoteDescription
                if (this.iceCandidateQueue && this.iceCandidateQueue.length > 0) {
                    for (const cand of this.iceCandidateQueue) {
                        try {
                            await this.pc.addIceCandidate(cand);
                        } catch (e) {
                            console.warn('Failed to add queued ICE candidate:', e);
                        }
                    }
                    this.iceCandidateQueue = [];
                }

                const answer = await this.pc.createAnswer();
                await this.pc.setLocalDescription(answer);

                const answerMsg = {
                    type: 'answer',
                    session_id: this.sessionId,
                    sdp: answer.sdp,
                    guest_id: this.guestId
                };
                this.ws.send(JSON.stringify(answerMsg));
                break;

            case 'candidate':
                if (msg.candidate && this.pc) {
                    const iceCandidate = new RTCIceCandidate({
                        candidate: msg.candidate,
                        sdpMid: msg.mid
                    });
                    if (!this.pc.remoteDescription || !this.pc.remoteDescription.type) {
                        this.iceCandidateQueue.push(iceCandidate);
                    } else {
                        await this.pc.addIceCandidate(iceCandidate);
                    }
                }
                break;

            case 'bye':
                console.log('Host closed stream session');
                this.disconnect();
                break;

            case 'error':
                console.error('Signaling protocol error:', msg.message);
                this.updateStatus('Signaling Error: ' + msg.message, false);
                break;
        }
    }

    startRenderLoop() {
        if (this.renderFrameId) cancelAnimationFrame(this.renderFrameId);

        const render = () => {
            if (!this.isConnected) return;
            if (this.videoEl && this.videoEl.readyState >= 2 && this.ctx && this.canvas) {
                this.ctx.drawImage(this.videoEl, 0, 0, this.canvas.width, this.canvas.height);
            }
            this.renderFrameId = requestAnimationFrame(render);
        };
        this.renderFrameId = requestAnimationFrame(render);
    }

    startTelemetryLoop() {
        if (this.statsInterval) clearInterval(this.statsInterval);

        this.statsInterval = setInterval(async () => {
            if (!this.pc || !this.isConnected) return;

            try {
                const report = await this.pc.getStats();
                let rttMs = 0;
                let packetsLost = 0;
                let packetsReceived = 0;
                let framesDecoded = 0;
                let fps = 0;
                let codecName = 'H.264';

                report.forEach((stat) => {
                    if (stat.type === 'candidate-pair' && stat.state === 'succeeded') {
                        if (stat.currentRoundTripTime !== undefined) {
                            rttMs = stat.currentRoundTripTime * 1000.0;
                        }
                    }
                    if (stat.type === 'inbound-rtp' && stat.kind === 'video') {
                        packetsLost = stat.packetsLost || 0;
                        packetsReceived = stat.packetsReceived || 0;
                        framesDecoded = stat.framesDecoded || 0;
                        if (stat.framesPerSecond) {
                            fps = Math.round(stat.framesPerSecond);
                        }
                        if (stat.codecId) {
                            const codecStat = report.get(stat.codecId);
                            if (codecStat && codecStat.mimeType) {
                                codecName = codecStat.mimeType.replace('video/', '').toUpperCase();
                            }
                        }
                    }
                });

                const totalPackets = packetsReceived + packetsLost;
                const lossPct = totalPackets > 0 ? ((packetsLost / totalPackets) * 100.0) : 0.0;

                const now = performance.now();
                if (fps === 0 && this.lastStatsTimestamp > 0) {
                    const elapsedSec = (now - this.lastStatsTimestamp) / 1000.0;
                    fps = Math.round((framesDecoded - this.lastFramesDecoded) / elapsedSec);
                }
                this.lastStatsTimestamp = now;
                this.lastFramesDecoded = framesDecoded;

                this.stats = {
                    fps: Math.max(0, fps),
                    decodeLatencyMs: rttMs > 0 ? (rttMs * 0.25) : 0.0,
                    rttMs: rttMs,
                    packetLossPct: lossPct,
                    activeCodec: codecName
                };

                const elFps = document.getElementById('hud-fps');
                const elLatency = document.getElementById('hud-latency');
                const elRtt = document.getElementById('hud-rtt');
                const elLoss = document.getElementById('hud-loss');
                const elCodec = document.getElementById('hud-codec');

                if (elFps) elFps.textContent = this.stats.fps;
                if (elLatency) elLatency.textContent = this.stats.decodeLatencyMs.toFixed(1);
                if (elRtt) elRtt.textContent = this.stats.rttMs.toFixed(1);
                if (elLoss) elLoss.textContent = this.stats.packetLossPct.toFixed(1) + '%';
                if (elCodec) elCodec.textContent = this.stats.activeCodec;

            } catch (err) {
                console.warn('Failed to query WebRTC stats:', err);
            }
        }, 1000);
    }

    serializeInputPacket(type, payloadBuffer, flags = 0) {
        const headerSize = 48; // InputPacketHeader is exactly 48 bytes
        const totalSize = headerSize + payloadBuffer.byteLength;
        const buffer = new ArrayBuffer(totalSize);
        const view = new DataView(buffer);

        this.inputSequence = (this.inputSequence || 0) + 1;

        // Header (48 bytes, little-endian):
        // uint16_t version (1)
        view.setUint16(0, 1, true);
        // uint8_t inputType
        view.setUint8(2, type);
        // uint8_t flags
        view.setUint8(3, flags);
        // uint32_t sequenceNumber
        view.setUint32(4, this.inputSequence, true);

        // uint64_t timestampNs
        const perfNow = (typeof performance !== 'undefined' && performance.now) ? performance.now() : Date.now();
        const nowNs = BigInt(Math.round(perfNow * 1000000));
        view.setBigUint64(8, nowNs, true);

        // char sessionId[32] null-padded
        const sessionBytes = new Uint8Array(buffer, 16, 32);
        if (this.sessionId) {
            const encoder = typeof TextEncoder !== 'undefined' ? new TextEncoder() : null;
            if (encoder) {
                const encoded = encoder.encode(this.sessionId);
                sessionBytes.set(encoded.subarray(0, 31));
            } else {
                for (let i = 0; i < Math.min(this.sessionId.length, 31); i++) {
                    sessionBytes[i] = this.sessionId.charCodeAt(i);
                }
            }
        }

        new Uint8Array(buffer, headerSize).set(new Uint8Array(payloadBuffer));
        return buffer;
    }

    sendInputPacket(type, payloadBuffer, flags = 0) {
        if (!this.inputChannel || this.inputChannel.readyState !== 'open') return;
        const buffer = this.serializeInputPacket(type, payloadBuffer, flags);
        this.inputChannel.send(buffer);
    }

    handleMouseMove(e) {
        if (!this.isConnected || !this.isPointerLocked) return;
        // MouseMovePayload (20 bytes)
        const payload = new ArrayBuffer(20);
        const view = new DataView(payload);
        view.setInt32(0, e.movementX || 0, true);
        view.setInt32(4, e.movementY || 0, true);
        view.setUint8(8, 0); // isAbsolute = false (relative motion)
        view.setUint8(9, 0); // reserved[0]
        view.setUint8(10, 0); // reserved[1]
        view.setUint8(11, 0); // reserved[2]

        let normX = 0.0;
        let normY = 0.0;
        if (this.canvas && this.canvas.width > 0 && this.canvas.height > 0) {
            const rect = this.canvas.getBoundingClientRect();
            if (rect.width > 0 && rect.height > 0) {
                normX = Math.max(0.0, Math.min(1.0, (e.clientX - rect.left) / rect.width));
                normY = Math.max(0.0, Math.min(1.0, (e.clientY - rect.top) / rect.height));
            }
        }
        view.setFloat32(12, normX, true);
        view.setFloat32(16, normY, true);

        this.sendInputPacket(2 /* InputType::MouseMoveEvent */, payload);
    }

    handleMouseButton(e, isDown) {
        if (!this.isConnected || !this.isPointerLocked) return;
        // MouseButtonPayload (4 bytes)
        const payload = new ArrayBuffer(4);
        const view = new DataView(payload);

        // Map DOM button (0=left, 1=middle, 2=right, 3=X1, 4=X2)
        // to Mist MouseButton (1=Left, 2=Right, 3=Middle, 4=X1, 5=X2)
        let btn = 1;
        if (e.button === 0) btn = 1; // Left
        else if (e.button === 1) btn = 3; // Middle
        else if (e.button === 2) btn = 2; // Right
        else if (e.button === 3) btn = 4; // X1
        else if (e.button === 4) btn = 5; // X2

        view.setUint8(0, btn);
        view.setUint8(1, isDown ? 1 : 0);
        view.setUint16(2, 0, true); // reserved

        this.sendInputPacket(3 /* InputType::MouseButtonEvent */, payload);
    }

    handleKeyboard(e, isDown) {
        if (!this.isConnected || !this.isPointerLocked) return;
        // KeyPayload (12 bytes)
        const payload = new ArrayBuffer(12);
        const view = new DataView(payload);
        const vk = e.keyCode || e.which || 0;
        view.setUint32(0, vk, true);
        view.setUint32(4, 0, true); // scanCode
        view.setUint8(8, isDown ? 0 /* KeyAction::Press = 0 */ : 1 /* KeyAction::Release = 1 */);
        view.setUint8(9, 0); // isExtended
        view.setUint16(10, 0, true); // reserved

        this.sendInputPacket(1 /* InputType::KeyEvent */, payload);
    }

    handleMouseWheel(e) {
        if (!this.isConnected || !this.isPointerLocked) return;
        // MouseWheelPayload (8 bytes)
        const payload = new ArrayBuffer(8);
        const view = new DataView(payload);
        view.setInt32(0, Math.round(e.deltaX || 0), true);
        view.setInt32(4, Math.round(e.deltaY || 0), true);

        this.sendInputPacket(4 /* InputType::MouseWheelEvent */, payload);
    }

    disconnect() {
        this.isConnected = false;
        this.iceCandidateQueue = [];
        if (this.renderFrameId) cancelAnimationFrame(this.renderFrameId);
        if (this.statsInterval) clearInterval(this.statsInterval);

        if (this.inputChannel) {
            try { this.inputChannel.close(); } catch (e) {}
            this.inputChannel = null;
        }

        if (this.pc) {
            try { this.pc.close(); } catch (e) {}
            this.pc = null;
        }

        if (this.ws) {
            try {
                if (this.ws.readyState === WebSocket.OPEN) {
                    this.ws.send(JSON.stringify({ type: 'bye', session_id: this.sessionId, guest_id: this.guestId }));
                }
                this.ws.close();
            } catch (e) {}
            this.ws = null;
        }

        if (this.videoEl) {
            this.videoEl.srcObject = null;
        }

        if (typeof document !== 'undefined' && document.pointerLockElement === this.canvas) {
            document.exitPointerLock();
        }

        this.updateStatus('Disconnected', false);
        if (this.btnConnect) this.btnConnect.classList.remove('hidden');
        if (this.btnDisconnect) this.btnDisconnect.classList.add('hidden');
        if (this.btnMouseLock) this.btnMouseLock.disabled = true;
        if (this.hud) this.hud.classList.add('hidden');

        if (this.ctx && this.canvas) {
            this.ctx.fillStyle = '#000000';
            this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
        }
    }

    updateStatus(text, connected) {
        if (!this.statusBadge) return;
        this.statusBadge.textContent = text;
        if (connected) {
            this.statusBadge.classList.add('connected');
        } else {
            this.statusBadge.classList.remove('connected');
        }
    }

    requestPointerLock() {
        if (!this.canvas) return;
        if (!this.isPointerLocked) {
            this.canvas.requestPointerLock();
        } else {
            document.exitPointerLock();
        }
    }

    toggleFullscreen() {
        if (typeof document === 'undefined') return;
        if (!document.fullscreenElement) {
            document.documentElement.requestFullscreen().catch(e => console.warn(e));
        } else {
            document.exitFullscreen().catch(e => console.warn(e));
        }
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = { WebCodecsCapabilityDetector, MistWebClient };
}

if (typeof document !== 'undefined') {
    document.addEventListener('DOMContentLoaded', () => {
        window.mistClient = new MistWebClient();
        WebCodecsCapabilityDetector.probeSupport().then(res => {
            console.log('Mist WebCodecs Capability Probe:', res);
        });
    });
}
