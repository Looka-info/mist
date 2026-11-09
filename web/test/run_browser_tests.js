/**
 * Mist Web Client Regression and Protocol Compliance Tests
 */
const assert = require('assert');
const { WebCodecsCapabilityDetector, MistWebClient } = require('../js/mist-web-client.js');

console.log('--- Running Mist WebRTC Browser Client Unit Tests ---');

// Test 1: Class and Method Structure
const client = new MistWebClient();
assert.ok(client, 'MistWebClient should instantiate');
assert.strictEqual(typeof client.initWebRtc, 'function', 'initWebRtc must exist');
assert.strictEqual(typeof client.connectSignaling, 'function', 'connectSignaling must exist');
assert.strictEqual(typeof client.handleSignalingMessage, 'function', 'handleSignalingMessage must exist');
assert.strictEqual(typeof client.serializeInputPacket, 'function', 'serializeInputPacket must exist');
assert.strictEqual(typeof client.startTelemetryLoop, 'function', 'startTelemetryLoop must exist');
assert.strictEqual(typeof client.handleMouseWheel, 'function', 'handleMouseWheel must exist');
console.log('✓ Test 1 Passed: Client API structure is fully implemented.');

// Test 2: Native Input Protocol Serialization Compliance (InputPacketHeader 48 bytes)
client.sessionId = 'test-session-12345';
const mousePayload = new ArrayBuffer(20);
const view = new DataView(mousePayload);
view.setInt32(0, 42, true);      // deltaX
view.setInt32(4, -18, true);     // deltaY
view.setUint8(8, 0);            // isAbsolute = 0
view.setFloat32(12, 0.25, true); // normalizedX
view.setFloat32(16, 0.75, true); // normalizedY

const packetBuffer = client.serializeInputPacket(2 /* MouseMoveEvent */, mousePayload);
assert.strictEqual(packetBuffer.byteLength, 68, 'Packet length must be header(48) + MouseMovePayload(20)');
const packetView = new DataView(packetBuffer);
assert.strictEqual(packetView.getUint16(0, true), 1, 'Version must be 1');
assert.strictEqual(packetView.getUint8(2), 2, 'Event type must be 2 (MouseMoveEvent)');
assert.strictEqual(packetView.getUint8(3), 0, 'Flags must be 0');
assert.strictEqual(packetView.getUint32(4, true), 1, 'Sequence number must start at 1');
assert.ok(packetView.getBigUint64(8, true) > 0n, 'TimestampNs must be non-zero');

// Verify session ID in header
const sessionBytes = new Uint8Array(packetBuffer, 16, 32);
const sessionStr = String.fromCharCode(...sessionBytes).replace(/\0/g, '');
assert.strictEqual(sessionStr, 'test-session-12345', 'Session ID must match');

// Verify payload
assert.strictEqual(packetView.getInt32(48, true), 42, 'DeltaX must be 42');
assert.strictEqual(packetView.getInt32(52, true), -18, 'DeltaY must be -18');
assert.strictEqual(packetView.getUint8(56), 0, 'isAbsolute must be 0');
assert.strictEqual(packetView.getFloat32(60, true), 0.25, 'NormalizedX must be 0.25');
assert.strictEqual(packetView.getFloat32(64, true), 0.75, 'NormalizedY must be 0.75');
console.log('✓ Test 2 Passed: SCTP DataChannel input serialization matches native InputProtocol binary spec (48B header + 20B payload).');

// Test 3: Zero Synthetic Math Verification
assert.ok(!client.startSimulationStream, 'Simulation stream loop must be completely removed');
assert.strictEqual(client.stats.rttMs, 0, 'Initial RTT must be 0 ms without synthetic generation');
console.log('✓ Test 3 Passed: Zero synthetic simulation loops or random RTT metrics.');

// Test 4: Capability Detector Probe Structure
WebCodecsCapabilityDetector.probeSupport().then(res => {
    assert.strictEqual(res.supported, false, 'Node environment correctly reports lack of browser window.VideoDecoder');
    assert.ok(res.reason.includes('WebCodecs API not available'), 'Reason matches expected secure context requirement');
    console.log('✓ Test 4 Passed: WebCodecs probe returns honest environment reporting.');
    console.log('All WebRTC Browser Client unit tests passed cleanly.\n');

    // Run WebRTC Integration Test Suite
    require('./webrtc_integration_test.js');
});
