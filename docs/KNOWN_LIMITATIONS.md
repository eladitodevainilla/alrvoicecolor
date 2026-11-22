# Technical Notes & Known Limitations

In keeping with transparent engineering standards, the following technical boundaries of v1.0.0 are explicitly documented:

1. **Pre-compiled DAW Binaries Not Included in Source Archive**:
   - The core C++20 DSP engine (`ALRVoiceColorDSP`) and its 20-test verification suite have been compiled and verified with GCC 12.2 (`-std=c++20 -O2 -Wall -Wextra -Werror`).
   - VST3, AU, and Standalone binaries must be compiled on the target operating system (Windows/macOS/Linux) using `CMakeLists.txt` (which fetches JUCE 8.0.4 automatically) so that native OS frameworks and ABI signatures match the host DAW.
2. **Pitch Correction Formant Preservation (PSOLA vs. True Vocal-Tract Resynthesis)**:
   - `ALR PITCH` uses a phase-coherent Pitch-Synchronous Dual-Grain Overlap-Add shifter. Within typical vocal correction ranges (`+/- 1 to 2.5 semitones`), vocal timbre remains highly natural and artifact-free. Extreme pitch shifts (`> 4–5 semitones`) will exhibit mild formant shifting compared to offline LPC/cepstral vocoders, as real-time low-latency efficiency (`10.67 ms @ 48 kHz`) is prioritized.
3. **Monophonic Vocal Requirement for Pitch Tracking**:
   - YIN pitch detection and pitch correction are designed for monophonic lead or backing vocal tracks. Polyphonic material (chords, harmonies printed on a single track, or heavy backing-track bleed) will reduce pitch tracking confidence and cause `PitchCorrector` to gracefully relax correction.
4. **Scale Modes in v1.0**:
   - `ALR PITCH` currently implements **Major** and **Natural Minor** scales across all 17 root note spellings. Chromatic, Pentatonic, Harmonic Minor, and custom per-note scale masks are planned for v1.1.
5. **RELEARN Adaptation Scope**:
   - RELEARN constructs an acoustic statistical vocal profile (F0 distribution, 7-band balance, approximate formants, top 3 resonances, harmonic density, and dynamic range) and computes conservative DSP targets. It is deterministic, explainable, and lightweight—not a heavy real-time neural network—and works best on dry or lightly treated raw vocal recordings.
