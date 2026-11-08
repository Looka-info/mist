# ADR 0018: Audio Resampling and Channel Downmixing Architecture

## Status
Accepted

## Context
Host Windows gaming PCs exhibit varied audio hardware configurations, including 44.1 kHz CD audio, 96 kHz / 192 kHz high-definition studio interfaces, and multi-channel 5.1 / 7.1 surround speaker configurations. Opus encoding operates natively at 48,000 Hz stereo (2-channel). A robust conversion layer is necessary to normalize arbitrary host formats into standardized 48 kHz stereo PCM before compression.

## Decision
1. **Multi-Channel Downmixing (ITU-R BS.775):**
   - For mono sources: duplicate the mono channel across Left and Right.
   - For 5.1 surround sources: apply the ITU-R BS.775 standard downmix matrix:
     - `Left  = FrontLeft  + 0.7071 * Center + 0.7071 * RearLeft`
     - `Right = FrontRight + 0.7071 * Center + 0.7071 * RearRight`
   - For 7.1 surround sources: incorporate side surround speakers with proportional scaling:
     - `Left  = FrontLeft  + 0.7071 * Center + 0.7071 * RearLeft  + 0.7071 * SideLeft`
     - `Right = FrontRight + 0.7071 * Center + 0.7071 * RearRight + 0.7071 * SideRight`
   - For sample formats in 16-bit signed integer (`WAVE_FORMAT_PCM`), normalize to IEEE 32-bit floating point `[-1.0, 1.0]` before mixing to eliminate clipping and precision loss.
2. **Fractional Linear Resampler:**
   - Implement a low-latency fractional resampler with continuous phase accumulator tracking.
   - Convert arbitrary input rates (44,100 Hz, 88,200 Hz, 96,000 Hz, 192,000 Hz) to exactly 48,000 Hz with sub-sample linear interpolation.
   - Avoid heavyweight multi-stage polyphase FIR filter banks that introduce unacceptable group delay (> 10 ms).
3. **Internal Ring Buffer / FIFO Framing:**
   - Store resampled 48 kHz stereo samples in an internal contiguous circular buffer.
   - Decouple variable WASAPI capture packet sizes (which vary by Windows timer resolution) from strict 10 ms (480-sample) or 20 ms (960-sample) Opus encoder frame intervals.

## Consequences
- Guaranteed compatibility with any default Windows audio playback device.
- Immaterial CPU overhead during conversion (< 2% of a single CPU core).
- Complete preservation of spatial cues and dialog clarity when playing multi-channel games on stereo client headphones.
