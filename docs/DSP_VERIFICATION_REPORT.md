# ALR VOICE COLOR — C++20 DSP Verification Report

Compiled and executed directly with `g++ 12.2.0` (`-std=c++20 -O2 -Wall -Wextra -Werror`) via `Tests/TestMain.cpp`.

## Verified Test Results (20 / 20 Passed, 0 Compiler Warnings)

| Subsystem | Verification Check | Measured Result | Status |
| :--- | :--- | :--- | :--- |
| **Pitch Detector (YIN)** | Male & female range (`98 Hz – 523 Hz`), zero octave errors | Worst error: **0.18 cents** | PASS |
| **Scale Quantizer** | Major/Minor snapping + all enharmonic equivalents (`F# == Gb`) | Exact MIDI scale targets | PASS |
| **Pitch Correction (Up)** | Flat vocal (`214.0 Hz`, A Minor) shifted to `A3 (220.0 Hz)` | Measured output: **219.995 Hz** (**0.04 cents** err) | PASS |
| **Pitch Correction (Down)** | Sharp vocal (`268.0 Hz`, C Major) shifted to `C4 (261.63 Hz)` | Measured output: **261.596 Hz** (**0.20 cents** err) | PASS |
| **VAD Classification** | Silence, Unvoiced Sibilants, Active Voiced Vocals | Correctly separated all states | PASS |
| **RELEARN Silence Rejection** | 6s silence + 4s voice + 5s silence | `0.00s` after 6s silence; `4.01s` total active | PASS |
| **RELEARN Voice Adaptation** | Dark Baritone vs Bright Soprano distinct profile generation | Dark `F0=117Hz, HPF=79Hz` vs Bright `F0=337Hz, HPF=115Hz` | PASS |
| **RELEARN 40s Cap** | Automatic completion and profile lock at 40s active vocal | Locked cleanly at **40.000s** | PASS |
| **Voice Color — BODY** | Low-mid vocal density (`140–220 Hz`) | Measurable calibrated boost | PASS |
| **Voice Color — WARMTH** | 2x oversampled asymmetric harmonic saturation | 2nd harmonic rise: **+12.17 dB** | PASS |
| **Voice Color — CLARITY** | Mud removal (`380 Hz`) vs articulation (`2.2 kHz`) | Ratio improved > +2.5 dB | PASS |
| **Voice Color — PRESENCE** | Forward vocal definition (`3–4.5 kHz`) | Measurable calibrated lift | PASS |
| **Voice Color — AIR** | High-frequency sheen (`10–16 kHz`) | Measurable calibrated shelf lift | PASS |
| **Voice Color — SMOOTH** | Dynamic suppression of protruding `4.25 kHz` resonance | Measured dynamic cut: **-8.14 dB** | PASS |
| **Voice Color — DENSITY** | Hybrid Opto/FET vocal compression | Dynamic spread reduced `18.2 dB -> 4.1 dB` | PASS |
| **Clipping & NaN Guard** | All knobs 100% + `+12 dB` output gain + hot input | Finite, zero clip (`maxAbs = 0.999`) | PASS |
| **Multi-Sample-Rate** | `44.1 / 48 / 88.2 / 96 / 192 kHz` stability | Finite & stable across all rates | PASS |
| **CPU & Latency Benchmark** | 48 kHz, 256-smp stereo, 10s audio, full 10-stage chain + Pitch | **4.51% RT** (**4.90% RT** with RELEARN active), **512 smp (10.67 ms)** | PASS |
