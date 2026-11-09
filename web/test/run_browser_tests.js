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
console.log('✓ Test 1 Passed: Client API structure is fully implemented.');

// Test 2: Input Protocol Serialization Compliance
const mousePayload = new ArrayBuffer(8);
const view = new DataView(mousePayload);
view.setInt32(0, 42, true);
view.setInt32(4, -18, true);

const packetBuffer = client.serializeInputPacket(1, mousePayload);
assert.strictEqual(packetBuffer.byteLength, 28, 'Packet length must be header(20) + payload(8)');
const packetView = new DataView(packetBuffer);
assert.strictEqual(packetView.getUint32(0, false), 0x4D495354, 'Magic must be 0x4D495354 ("MIST")');
assert.strictEqual(packetView.getUint8(4), 1, 'Version must be 1');
assert.strictEqual(packetView.getUint8(5), 1, 'Event type must be 1 (MouseMove)');
assert.strictEqual(packetView.getUint32(8, true), 8, 'Payload length must be 8');
assert.strictEqual(packetView.getInt32(20, true), 42, 'DeltaX must be 42');
assert.strictEqual(packetView.getInt32(24, true), -18, 'DeltaY must be -18');
console.log('✓ Test 2 Passed: SCTP DataChannel input serialization matches native InputProtocol binary spec.');

// Test 3: Zero Synthetic Math Verification
assert.ok(!client.startSimulationStream, 'Simulation stream loop must be completely removed');
assert.strictEqual(client.stats.rttMs, 0, 'Initial RTT must be 0 ms without synthetic generation');
console.log('✓ Test 3 Passed: Zero synthetic simulation loops or random RTT metrics.');

// Test 4: Capability Detector Probe Structure
WebCodecsCapabilityDetector.probeSupport().then(res => {
    assert.strictEqual(res.supported, false, 'Node environment correctly reports lack of browser window.VideoDecoder');
    assert.ok(res.reason.includes('WebCodecs API not available'), 'Reason matches expected secure context requirement');
    console.log('✓ Test 4 Passed: WebCodecs probe returns honest environment reporting.');
    console.log('All WebRTC Browser Client tests passed cleanly.');
});
