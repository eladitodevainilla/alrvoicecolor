# ALR VOICE COLOR

**Professional Adaptive Vocal Processing, Harmonic Coloration & Real-Time Automatic Pitch Correction Plugin (C++20 / JUCE 8)**

> *Philosophy: Enhance the voice to commercial studio quality without stripping away its natural identity.*

---

## Overview

**ALR VOICE COLOR** is a modular, production-grade vocal studio plugin developed in modern C++20 and the JUCE 8 framework (VST3, AU, Standalone). It combines three tightly integrated, real-time DSP engines inside a restrained dark-studio horizontal interface:

1. **RELEARN — 40-Second Smart Voice Analysis**: Listens exclusively to *active vocal segments* (ignoring silence, room noise, and breaths via multi-feature VAD) up to 40.0 seconds, measures the vocalist's acoustic signature (F0 register, 7-band spectral balance, approximate formants, narrowband resonances, harmonic density, and dynamic range), and adapts corrective EQ, resonance targeting, saturation bias, and compression automatically. Learned profiles persist inside the DAW project state.
2. **VOICE COLOR — Calibrated Vocal Character Engine**: Eight real DSP controls (`BODY`, `WARMTH`, `CLARITY`, `PRESENCE`, `AIR`, `DENSITY`, `SMOOTH`, `COLOR`) featuring 2x oversampled asymmetric tube/tape harmonic saturation, 3-band dynamic resonance control, parallel Opto/FET vocal compression, and an Anti-Accumulation Guard that keeps tones musical even when stacking multiple controls.
3. **ALR PITCH — Real-Time Automatic Pitch Correction**: Low-latency (`10.67 ms @ 48 kHz`) YIN fundamental tracking paired with a phase-coherent Pitch-Synchronous Dual-Grain Overlap-Add (PSOLA-style) shifter, Major/Minor scale quantization across all 17 root note spellings (`C` through `B` with sharps and flats), `RETUNE SPEED`, vibrato-preserving `HUMANIZE`, glide-aware `FLEX`, `PITCH RANGE`, and latency-aligned `MIX`.

---

## Visual Identity

Designed for long mixing sessions on studio displays with an original **ALR** visual system:
- **Charcoal Black** (`#111215`) primary canvas & **Graphite** (`#17191E`) modular panels.
- **Champagne Gold** (`#D2B478`) for Voice Color & RELEARN intelligence; **Subtle Electric Blue** (`#5A8ED6`) for ALR PITCH.
- Live dual-layer **Pre/Post FFT Spectrum Analyzer** with real-time **Combined EQ & Learned Profile Curve** overlay, dual RMS/Peak/Peak-Hold metering + Gain Reduction bar, and real-time pitch cents visualizer.

---

## Project Structure

```text
ALR-VOICE-COLOR/
├── .github/workflows/
│   └── build-and-release.yml        # Auto-builds Windows/macOS/Linux VST3/AU/Standalone Releases
├── CMakeLists.txt                   # JUCE 8 FetchContent + VST3 / AU / Standalone + CTest
├── LICENSE                          # MIT License
├── README.md
├── .gitignore
├── docs/
│   ├── ARCHITECTURE.md              # 10-stage signal flow & lock-free threading architecture
│   ├── BUILD_GUIDE.md               # Windows / macOS / Linux build & DAW installation guide
│   ├── CONTROLS_MANUAL.md           # Full studio control reference & workflow
│   ├── DSP_VERIFICATION_REPORT.md   # Measured C++20 DSP test & CPU benchmark results
│   └── KNOWN_LIMITATIONS.md         # Transparent technical scope & v1.0 boundaries
├── scripts/
│   ├── build_macos_linux.sh
│   ├── build_windows.bat
│   └── run_dsp_tests.sh
├── Source/
│   ├── PluginProcessor.h / .cpp     # JUCE AudioProcessor, APVTS, latency & DAW state persistence
│   ├── PluginEditor.h / .cpp        # Resizable horizontal commercial studio GUI
│   ├── Parameters/
│   │   ├── ParameterIDs.h
│   │   └── ParameterManager.h / .cpp
│   ├── DSP/                         # Pure C++20 zero-JUCE DSP library
│   │   ├── DSPCommon.h
│   │   ├── VoiceProfile.h
│   │   ├── VoiceActivityDetector.h / .cpp
│   │   ├── VoiceAnalyzer.h / .cpp
│   │   ├── PitchDetector.h / .cpp
│   │   ├── PitchCorrector.h / .cpp
│   │   ├── VocalEqualizer.h / .cpp
│   │   ├── ResonanceController.h / .cpp
│   │   ├── HarmonicSaturation.h / .cpp
│   │   ├── DynamicProcessor.h / .cpp
│   │   └── VocalProcessingChain.h / .cpp
│   └── UI/
│       ├── ALRLookAndFeel.h / .cpp
│       ├── RotaryKnobComponent.h / .cpp
│       ├── MeterComponent.h / .cpp
│       ├── SpectrumDisplay.h / .cpp
│       ├── RelearnPanel.h / .cpp
│       ├── VoiceColorPanel.h / .cpp
│       └── PitchPanel.h / .cpp
└── Tests/
    ├── CMakeLists.txt
    └── TestMain.cpp                 # Standalone 20-test C++20 DSP verification & CPU suite
```

---

## Automatic Cloud Build & 1-Click Download (GitHub Actions)

This repository includes a complete CI/CD pipeline (`.github/workflows/build-and-release.yml`) that **automatically compiles ready-to-use plugins** every time you push to `main` or push a version tag (`v1.0.0`):

- **Windows x64**: `ALR-VOICE-COLOR-Windows-x64.zip` (`VST3` + `Standalone`)
- **macOS Universal (Apple Silicon + Intel)**: `ALR-VOICE-COLOR-macOS-Universal.zip` (`VST3` + `AU` + `Standalone`)
- **Linux x64**: `ALR-VOICE-COLOR-Linux-x64.zip` (`VST3` + `Standalone`)

Once pushed to GitHub, wait ~5–8 minutes for the **Actions** workflow to complete and download the compiled plugin directly from your repository's **Releases** page — no local compiler required.

---

## Quick Start

### 1. Verify the C++20 DSP Engine Immediately
```bash
bash scripts/run_dsp_tests.sh
```

### 2. Compile VST3 / AU / Standalone (JUCE 8 via CMake)
**macOS / Linux:**
```bash
bash scripts/build_macos_linux.sh
```
**Windows (Visual Studio 2022):**
```bat
scripts\build_windows.bat
```

See [docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md) for full OS dependencies and DAW installation paths (FL Studio, Ableton Live, Logic Pro, Cubase, Reaper, Studio One).

---

## Development Status & Verified Benchmarks

- **DSP Core (`ALRVoiceColorDSP`)**: Compiled & verified with `g++ 12.2.0 -std=c++20 -O2 -Wall -Wextra -Werror` (**20 / 20 tests passing, 0 warnings**).
  - Pitch Detection accuracy: **0.18 cents** worst-case error (98–523 Hz).
  - Real-time Pitch Correction accuracy: **0.04 – 0.20 cents** steady-state accuracy.
  - CPU footprint (48 kHz, 256-sample stereo, full 10-stage chain + ALR PITCH): **4.51% single-core RT** (**4.90% RT** with RELEARN active).
  - Deterministic reported latency: **512 samples (10.67 ms @ 48 kHz)**.
- **JUCE 8 Plugin Targets (VST3 / AU / Standalone)**: Complete source & CMake build configuration ready for native OS compilation (pre-built OS binaries are not bundled in the source repository).

See [docs/DSP_VERIFICATION_REPORT.md](docs/DSP_VERIFICATION_REPORT.md) and [docs/KNOWN_LIMITATIONS.md](docs/KNOWN_LIMITATIONS.md) for complete technical details.
