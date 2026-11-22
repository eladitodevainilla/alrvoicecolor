# ALR VOICE COLOR — Technical Architecture

## 1. Design Separation
The codebase strictly decouples the pure C++20 audio DSP core (`ALRVoiceColorDSP`) from the JUCE 8 plugin host & UI layer (`ALRVoiceColor`):

- **Zero JUCE dependency in `Source/DSP/`**: Every audio processing module compiles standalone with any C++20 compiler (`g++`, `clang++`, `MSVC`), enabling deterministic offline verification and rapid benchmarking.
- **Zero heap allocations on the audio thread**: All scratch buffers, ring buffers, FFT twiddle tables, YIN correlation arrays, and lock-free SPSC FIFOs are pre-allocated in `prepare()`.
- **Lock-free Audio <-> UI communication**: Audio metering, VAD/RELEARN telemetry, and Pre/Post spectrum samples pass through double-buffered atomics and `LockFreeSPSCQueue` without mutex locks.

## 2. 10-Stage Internal Signal Chain (`VocalProcessingChain`)
1. **Input Stage**: NaN/Inf sanitization, 20 Hz DC blocker, stereo peak/RMS metering, latency-aligned dry delay line (`512` samples @ 48 kHz).
2. **Voice Detection (`VoiceActivityDetector`)**: Adaptive noise floor, vocal core (120–3500 Hz) vs HF (>4.2 kHz) energy ratios, zero-crossing rate, and periodicity confidence classify `Silence`, `BackgroundNoise`, `Breath`, `UnvoicedConsonant`, and `ActiveVoice` (+ `SustainedVowel`).
3. **Pitch Detection (`PitchDetector`)**: Anti-aliased ~24 kHz decimated **YIN** fundamental estimator with $O(1)$ sliding energy, SIMD-friendly dot product, absolute threshold search, parabolic interpolation (<0.2 cents accuracy), and 3-frame octave-jump protection.
4. **Automatic Pitch Correction (`PitchCorrector`)**: Musical scale quantizer (17 enharmonic root options, Major/Minor, 16-cent note-boundary hysteresis) driving a **Pitch-Synchronous Dual-Grain Overlap-Add (PSOLA-style)** shifter whose splice span locks to exact integer multiples of $T_0$ (`m * T0`) for phase-coherent Hann crossfades. Unvoiced consonants smoothly relax to `ratio = 1.0`.
5. **Corrective EQ (`VocalEqualizer` Stage 1)**: Voice-adapted high-pass filter (`0.68 * F0_p10`) + conservative RELEARN corrective bands (mud, nasal/boxy, body, presence, air).
6. **Resonance Control (`ResonanceController`)**: 3-band dynamic sidechain resonance suppressor targeting either default harshness zones (`2.55 / 4.25 / 6.8 kHz`) or the top 3 narrowband resonances discovered by RELEARN.
7. **Harmonic Saturation (`HarmonicSaturation`)**: True **2x oversampled** asymmetric tube/tape transfer function with HF pre-de-emphasis (-2.5 dB @ 6.5 kHz) and post DC blocker to enrich even/odd vocal harmonics without sibilant fizz.
8. **Tonal Enhancement (`VocalEqualizer` Stage 2)**: Calibrated `BODY`, `CLARITY`, `PRESENCE`, and `AIR` bands protected by an **Anti-Accumulation Interaction Guard** and loudness compensation.
9. **Dynamic Control (`DynamicProcessor`)**: Stereo-linked hybrid Opto/FET vocal compressor (parallel blended via `DENSITY`, threshold & ratio conservatively adapted by RELEARN) + transparent protective soft-clip ceiling at `-0.35 dBFS`.
10. **Output Stage**: Optional slow RMS Auto-Gain Compensation for fair A/B comparison, latency-aligned Global Mix, Output Gain (`-18` to `+18 dB`), click-free 10 ms True Bypass ramp, and Post-spectrum tap.

## 3. RELEARN Engine (`VoiceAnalyzer` + `VoiceProfile`)
- Accumulates up to **40.0 seconds** of *active vocal* audio only; silences, background room noise, and isolated breaths pause accumulation automatically.
- Computes 2048-point Hann-windowed normalized power spectra (hop 512), F0 percentiles (`P10 / P50 / P90`), dynamic range (`P90 - P10` RMS), approximate formant centroids (`F1 / F2 / F3`), spectral tilt, harmonic density, and top 3 protruding narrowband resonances.
- Serializes cleanly inside the DAW project state via JUCE `ValueTree` (`ALRVoiceProfile`).
