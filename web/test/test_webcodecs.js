/**
 * Automated WebCodecs qualification tests for browser environments
 */

async function runWebCodecsTests() {
    const results = document.getElementById('results');
    results.innerHTML = '';

    function log(msg, pass = true) {
        const div = document.createElement('div');
        div.className = 'log ' + (pass ? 'pass' : 'fail');
        div.textContent = (pass ? '[PASS] ' : '[FAIL] ') + msg;
        results.appendChild(div);
    }

    // Test 1: WebCodecs API availability
    if (typeof VideoDecoder !== 'undefined') {
        log('Test 1: VideoDecoder interface exists in global scope');
    } else {
        log('Test 1: VideoDecoder interface missing (requires secure context)', false);
        return;
    }

    // Test 2: H.264 configuration probe
    try {
        const h264Support = await VideoDecoder.isConfigSupported({
            codec: 'avc1.64002a',
            codedWidth: 1920,
            codedHeight: 1080
        });
        log(`Test 2: H.264 baseline support probe: supported=${h264Support.supported}`);
    } catch (e) {
        log(`Test 2: H.264 probe threw exception: ${e.message}`, false);
    }

    // Test 3: AV1 configuration probe
    try {
        const av1Support = await VideoDecoder.isConfigSupported({
            codec: 'av01.0.08M.10',
            codedWidth: 1920,
            codedHeight: 1080
        });
        log(`Test 3: AV1 support probe: supported=${av1Support.supported}`);
    } catch (e) {
        log(`Test 3: AV1 probe threw exception: ${e.message}`, false);
    }

    // Test 4: Decoder instantiation and state lifecycle
    try {
        let frameCount = 0;
        const decoder = new VideoDecoder({
            output: (frame) => {
                frameCount++;
                frame.close();
            },
            error: (e) => console.error(e)
        });

        if (decoder.state === 'unconfigured') {
            log('Test 4.1: Decoder starts in unconfigured state');
        } else {
            log(`Test 4.1: Unexpected state: ${decoder.state}`, false);
        }

        decoder.configure({
            codec: 'avc1.64002a',
            codedWidth: 1920,
            codedHeight: 1080,
            optimizeForLatency: true
        });

        if (decoder.state === 'configured') {
            log('Test 4.2: Decoder transitioned to configured state');
        } else {
            log(`Test 4.2: Decoder failed to configure: ${decoder.state}`, false);
        }

        decoder.close();
        if (decoder.state === 'closed') {
            log('Test 4.3: Decoder closed cleanly and freed resources');
        } else {
            log(`Test 4.3: Decoder failed to close: ${decoder.state}`, false);
        }
    } catch (e) {
        log(`Test 4: Decoder lifecycle error: ${e.message}`, false);
    }

    log('Summary: Browser WebCodecs qualification tests completed.');
}

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', runWebCodecsTests);
} else {
    runWebCodecsTests();
}
