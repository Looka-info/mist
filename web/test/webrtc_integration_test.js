/**
 * Mist WebRTC Browser-to-Host Integration & Protocol Conformance Tests
 * Validates real signaling messages, ICE candidate queueing, and binary input protocol serialization.
 */
const assert = require('assert');
const { MistWebClient } = require('../js/mist-web-client.js');

console.log('--- Running Mist WebRTC Protocol Integration Tests ---');

// Mock RTCPeerConnection to test negotiation state machine and candidate queueing
class MockPeerConnection {
    constructor(config) {
        this.config = config;
        this.remoteDescription = null;
        this.localDescription = null;
        this.addedIceCandidates = [];
        this.connectionState = 'new';
        this.dataChannels = [];
    }

    addTransceiver(kind, init) {
        return { kind, direction: init ? init.direction : 'sendrecv' };
    }

    createDataChannel(label, options) {
        const dc = {
            label,
            options,
            readyState: 'open',
            sentMessages: [],
            send: function(data) {
                this.sentMessages.push(data);
            },
            close: function() {
                this.readyState = 'closed';
            }
        };
        this.dataChannels.push(dc);
        return dc;
    }

    async setRemoteDescription(desc) {
        this.remoteDescription = desc;
    }

    async createAnswer() {
        return { type: 'answer', sdp: 'v=0\r\no=guest-session 1 1 IN IP4 127.0.0.1\r\ns=Mist\r\n' };
    }

    async setLocalDescription(desc) {
        this.localDescription = desc;
        this.connectionState = 'connected';
    }

    async addIceCandidate(candidate) {
        if (!this.remoteDescription) {
            throw new Error('InvalidStateError: Cannot add ICE candidate without remote description');
        }
        this.addedIceCandidates.push(candidate);
    }

    close() {
        this.connectionState = 'closed';
    }
}

// Global mock for Node testing environment
global.RTCPeerConnection = MockPeerConnection;
global.RTCSessionDescription = function(desc) { return desc; };
global.RTCIceCandidate = function(cand) { return cand; };

async function runTests() {
    // -------------------------------------------------------------
    // Test 1: Signaling Message Dispatch & Session Lifecycle
    // -------------------------------------------------------------
    const client = new MistWebClient();
    client.sessionId = 'stream-session-qa';
    client.guestId = 'guest-node-runner';

    // Mock WebSocket
    const sentWsMessages = [];
    client.ws = {
        readyState: 1, // OPEN
        send: function(msg) {
            sentWsMessages.push(JSON.parse(msg));
        }
    };

    // Initialize mock WebRTC
    await client.initWebRtc('avc1.64002a');
    assert.ok(client.pc instanceof MockPeerConnection, 'RTCPeerConnection must be instantiated');
    assert.strictEqual(client.iceCandidateQueue.length, 0, 'ICE candidate queue must be initially empty');

    // 1.1: Server responds with Registered
    await client.handleSignalingMessage({
        type: 'registered',
        session_id: 'stream-session-qa',
        role: 'guest',
        peer_connected: false,
        guest_id: 'guest-node-runner'
    });

    // 1.2: Host joins room
    await client.handleSignalingMessage({
        type: 'peer_joined',
        session_id: 'stream-session-qa',
        peer_role: 'host',
        guest_id: 'host-pc-01'
    });

    // 1.3: Early trickle ICE candidate arrives BEFORE Offer
    await client.handleSignalingMessage({
        type: 'candidate',
        session_id: 'stream-session-qa',
        candidate: 'candidate:1 1 UDP 2130706431 192.168.1.100 50000 typ host',
        mid: '0',
        guest_id: 'host-pc-01'
    });
    assert.strictEqual(client.iceCandidateQueue.length, 1, 'Early candidate must be enqueued');
    assert.strictEqual(client.pc.addedIceCandidates.length, 0, 'No candidate should be added before remoteDescription');

    // 1.4: SDP Offer arrives from Host
    await client.handleSignalingMessage({
        type: 'offer',
        session_id: 'stream-session-qa',
        sdp: 'v=0\r\no=host-session 1 1 IN IP4 192.168.1.100\r\ns=MistHost\r\n',
        target_guest_id: 'guest-node-runner'
    });

    assert.ok(client.pc.remoteDescription, 'Remote description must be set');
    assert.strictEqual(client.iceCandidateQueue.length, 0, 'Candidate queue must be drained');
    assert.strictEqual(client.pc.addedIceCandidates.length, 1, 'Enqueued candidate must be delivered to RTCPeerConnection');

    // Verify Answer was sent to host via WebSocket
    assert.strictEqual(sentWsMessages.length, 1, 'One answer message should be sent');
    assert.strictEqual(sentWsMessages[0].type, 'answer');
    assert.strictEqual(sentWsMessages[0].session_id, 'stream-session-qa');
    assert.strictEqual(sentWsMessages[0].guest_id, 'guest-node-runner');
    assert.ok(sentWsMessages[0].sdp.includes('Mist'), 'Answer SDP must be populated');

    // 1.5: Subsequent ICE candidate arrives AFTER Offer
    await client.handleSignalingMessage({
        type: 'candidate',
        session_id: 'stream-session-qa',
        candidate: 'candidate:2 1 UDP 1694498815 203.0.113.1 50001 typ srflx',
        mid: '0',
        guest_id: 'host-pc-01'
    });
    assert.strictEqual(client.pc.addedIceCandidates.length, 2, 'Subsequent candidate delivered immediately');
    console.log('✓ Test 1 Passed: Signaling state machine and ICE candidate queueing handle out-of-order negotiation.');

    // -------------------------------------------------------------
    // Test 2: Binary Input Protocol Format & Bit-Exact Layouts
    // -------------------------------------------------------------
    client.isConnected = true;
    client.isPointerLocked = true;

    // 2.1: Key Event (Press & Release)
    client.handleKeyboard({ keyCode: 0x57 /* 'W' */ }, true);
    client.handleKeyboard({ keyCode: 0x57 /* 'W' */ }, false);

    assert.strictEqual(client.inputChannel.sentMessages.length, 2, 'Two keyboard packets sent');

    // Inspect Key Press Packet
    const keyPressBuf = client.inputChannel.sentMessages[0];
    assert.strictEqual(keyPressBuf.byteLength, 60, 'KeyEvent packet must be 48 bytes header + 12 bytes payload');
    const keyView = new DataView(keyPressBuf);

    // Header validation (48 bytes)
    assert.strictEqual(keyView.getUint16(0, true), 1, 'Header version must be 1');
    assert.strictEqual(keyView.getUint8(2), 1, 'InputType::KeyEvent must be 1');
    assert.strictEqual(keyView.getUint8(3), 0, 'Flags must be 0');
    assert.strictEqual(keyView.getUint32(4, true), 1, 'Sequence number must be 1');
    assert.ok(keyView.getBigUint64(8, true) > 0n, 'TimestampNs must be non-zero');

    // SessionId in header
    const sidBytes = new Uint8Array(keyPressBuf, 16, 32);
    const sidStr = String.fromCharCode(...sidBytes).replace(/\0/g, '');
    assert.strictEqual(sidStr, 'stream-session-qa', 'Session ID must be copied to header');

    // KeyPayload validation (12 bytes at offset 48)
    assert.strictEqual(keyView.getUint32(48, true), 0x57, 'Virtual key code must be 0x57');
    assert.strictEqual(keyView.getUint32(52, true), 0, 'ScanCode must be 0');
    assert.strictEqual(keyView.getUint8(56), 0, 'KeyAction::Press must be 0');
    assert.strictEqual(keyView.getUint8(57), 0, 'isExtended must be 0');

    // Key Release validation
    const keyReleaseBuf = client.inputChannel.sentMessages[1];
    const keyRelView = new DataView(keyReleaseBuf);
    assert.strictEqual(keyRelView.getUint32(4, true), 2, 'Sequence number must increment to 2');
    assert.strictEqual(keyRelView.getUint8(56), 1, 'KeyAction::Release must be 1');

    // 2.2: Mouse Move (Relative + Normalized)
    client.canvas = { width: 1920, height: 1080, getBoundingClientRect: () => ({ left: 0, top: 0, width: 1920, height: 1080 }) };
    client.handleMouseMove({ movementX: 15, movementY: -8, clientX: 960, clientY: 540 });

    const mouseBuf = client.inputChannel.sentMessages[2];
    assert.strictEqual(mouseBuf.byteLength, 68, 'MouseMove packet must be 48 bytes header + 20 bytes payload');
    const mouseView = new DataView(mouseBuf);
    assert.strictEqual(mouseView.getUint8(2), 2, 'InputType::MouseMoveEvent must be 2');
    assert.strictEqual(mouseView.getUint32(4, true), 3, 'Sequence number must increment to 3');
    assert.strictEqual(mouseView.getInt32(48, true), 15, 'DeltaX must be 15');
    assert.strictEqual(mouseView.getInt32(52, true), -8, 'DeltaY must be -8');
    assert.strictEqual(mouseView.getUint8(56), 0, 'isAbsolute must be 0');
    assert.strictEqual(mouseView.getFloat32(60, true), 0.5, 'NormalizedX must be 0.5');
    assert.strictEqual(mouseView.getFloat32(64, true), 0.5, 'NormalizedY must be 0.5');

    // 2.3: Mouse Button Mapping
    // Test Left Button (0 -> Mist 1)
    client.handleMouseButton({ button: 0 }, true);
    // Test Right Button (2 -> Mist 2)
    client.handleMouseButton({ button: 2 }, true);
    // Test Middle Button (1 -> Mist 3)
    client.handleMouseButton({ button: 1 }, true);

    const btnLeft = new DataView(client.inputChannel.sentMessages[3]);
    assert.strictEqual(btnLeft.byteLength, 52, 'MouseButton packet must be 48 bytes header + 4 bytes payload');
    assert.strictEqual(btnLeft.getUint8(2), 3, 'InputType::MouseButtonEvent must be 3');
    assert.strictEqual(btnLeft.getUint8(48), 1, 'Left button mapped to 1');
    assert.strictEqual(btnLeft.getUint8(49), 1, 'isDown must be 1');

    const btnRight = new DataView(client.inputChannel.sentMessages[4]);
    assert.strictEqual(btnRight.getUint8(48), 2, 'Right button mapped to 2');

    const btnMiddle = new DataView(client.inputChannel.sentMessages[5]);
    assert.strictEqual(btnMiddle.getUint8(48), 3, 'Middle button mapped to 3');

    // 2.4: Mouse Wheel
    client.handleMouseWheel({ deltaX: -120, deltaY: 240 });
    const wheelBuf = client.inputChannel.sentMessages[6];
    assert.strictEqual(wheelBuf.byteLength, 56, 'MouseWheel packet must be 48 bytes header + 8 bytes payload');
    const wheelView = new DataView(wheelBuf);
    assert.strictEqual(wheelView.getUint8(2), 4, 'InputType::MouseWheelEvent must be 4');
    assert.strictEqual(wheelView.getInt32(48, true), -120, 'wheelDeltaX must be -120');
    assert.strictEqual(wheelView.getInt32(52, true), 240, 'wheelDeltaY must be 240');

    console.log('✓ Test 2 Passed: Binary input packet headers, event IDs, and payload layouts match C++ InputProtocol.');

    // -------------------------------------------------------------
    // Test 3: Teardown & Resource Cleanup
    // -------------------------------------------------------------
    const savedPc = client.pc;
    client.disconnect();
    assert.strictEqual(client.isConnected, false, 'Client must mark disconnected');
    assert.strictEqual(savedPc.connectionState, 'closed', 'RTCPeerConnection must be closed');
    assert.strictEqual(client.pc, null, 'RTCPeerConnection handle must be cleared');
    assert.strictEqual(client.inputChannel, null, 'DataChannel must be cleared');
    assert.strictEqual(client.iceCandidateQueue.length, 0, 'Candidate queue must be empty on teardown');
    console.log('✓ Test 3 Passed: Client gracefully closes DataChannel and RTCPeerConnection on disconnect.');

    console.log('All WebRTC protocol integration tests passed cleanly.');
}

runTests().catch(err => {
    console.error('Integration test failure:', err);
    process.exit(1);
});
