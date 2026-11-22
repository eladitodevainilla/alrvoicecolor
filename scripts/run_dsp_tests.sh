#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Compiling ALR VOICE COLOR C++20 DSP Verification Suite..."
g++ -std=c++20 -O2 -Wall -Wextra -Werror \
    "$ROOT_DIR/Source/DSP/VoiceActivityDetector.cpp" \
    "$ROOT_DIR/Source/DSP/VoiceAnalyzer.cpp" \
    "$ROOT_DIR/Source/DSP/PitchDetector.cpp" \
    "$ROOT_DIR/Source/DSP/PitchCorrector.cpp" \
    "$ROOT_DIR/Source/DSP/VocalEqualizer.cpp" \
    "$ROOT_DIR/Source/DSP/ResonanceController.cpp" \
    "$ROOT_DIR/Source/DSP/HarmonicSaturation.cpp" \
    "$ROOT_DIR/Source/DSP/DynamicProcessor.cpp" \
    "$ROOT_DIR/Source/DSP/VocalProcessingChain.cpp" \
    "$ROOT_DIR/Tests/TestMain.cpp" \
    -o "$ROOT_DIR/alr_dsp_tests"

"$ROOT_DIR/alr_dsp_tests"
rm -f "$ROOT_DIR/alr_dsp_tests"
