# ALR VOICE COLOR — Controls & Workflow Manual

## Quick Studio Workflow
1. Insert **ALR VOICE COLOR** on a mono or stereo vocal track.
2. Click **RELEARN** and play back the vocal (up to 40 seconds of active singing/rap; pauses and breaths are ignored automatically). Click **STOP & APPLY** anytime after 2 seconds, or let it complete at 40.0 seconds.
3. In **ALR PITCH**, select the song **Key** and **Scale** (`Major` or `Minor`), then click **ACTIVE** to enable real-time pitch correction.
4. Shape the final vocal polish using the **VOICE COLOR** knobs. Use **BYPASS** with **AUTO GAIN** enabled for fair level-matched A/B checks.

## Section Reference

### HEADER
- **AUTO GAIN**: Automatically matches wet RMS loudness to dry input loudness so A/B comparisons reflect true tonal improvement rather than volume boosts.
- **SETTINGS**: Displays live sample rate, host-reported latency, auto-gain offset, and toggles the Pre-Processing spectrum overlay.
- **BYPASS**: Click-free, latency-compensated global true bypass.

### VOICE ANALYSIS (RELEARN)
- **RELEARN / STOP & APPLY**: Starts or finishes the 40-second active-voice analyzer. Repeatable whenever switching singers, microphones, or rooms.
- **ACTIVE VOICE CAPTURED**: Progress bar and timer counting strictly voiced vocal content (`0.0s` – `40.0s`).
- **DETECTOR STATE**: Live VAD classification (`SILENCE`, `ROOM NOISE`, `BREATH`, `CONSONANT / SIBILANT`, `ACTIVE VOICE`).
- **PROFILE LEARNED CARD**: Shows measured Median F0, adapted HPF anchor, Body focus frequency, Dynamic Spread, Harmonic Index, and primary resonance node.
- **RESET PROFILE**: Clears the learned vocal profile back to neutral conservative defaults.

### VOICE COLOR
- **BODY** (`0–100%`): Adds low-mid density anchored to the singer's learned fundamental warmth zone (`125–250 Hz`), paired with an automatic subtle mud-cleanup dip to prevent low-end buildup.
- **WARMTH** (`0–100%`): 2x oversampled asymmetric analog-style even and soft odd harmonics with sibilance protection.
- **CLARITY** (`0–100%`): Scoops veiled boxy low-mids (`~380 Hz`) while gently lifting articulation (`~2.2 kHz`).
- **PRESENCE** (`0–100%`): Broad musical forwardness (`~3.7 kHz`) calibrated to sit cleanly inside dense mixes without shoutiness.
- **AIR** (`0–100%`): Smooth Baxandall-style top-end sheen (`11.4 kHz`) avoiding the harsh 6–8 kHz sibilance region.
- **DENSITY** (`0–100%`): Parallel-blended hybrid Opto/FET vocal compression for intimate, consistent proximity.
- **SMOOTH** (`0–100%`): 3-band dynamic resonance suppressor and de-harsher targeting protruding peaks and RELEARN-discovered resonances.
- **COLOR** (`0–100%`): Master intensity macro scaling both learned profile adaptation and tonal colorization with anti-accumulation protection.

### ALR PITCH
- **OFF / ACTIVE**: Enables or bypasses real-time automatic pitch correction.
- **KEY**: Root note selector supporting all 17 standard & enharmonic spellings (`C, C#, Db, D, D#, Eb, E, F, F#, Gb, G, G#, Ab, A, A#, Bb, B`).
- **SCALE**: `Major` or `Minor` scale filter.
- **PITCH RANGE**: Restricts F0 tracking (`Low`, `Mid`, `High`, `Full`) to match vocal register and eliminate octave tracking errors.
- **RETUNE** (`0.5–200 ms`): Correction speed. Fast (`0–10 ms`) yields tight modern trap/pop tuning; slower (`40–200 ms`) yields transparent natural correction.
- **HUMANIZE** (`0–100%`): Preserves natural vocal vibrato and relaxes tuning stiffness on sustained notes.
- **FLEX** (`0–100%`): Softens pitch transitions during fast vocal glides and note-to-note portamento.
- **MIX** (`0–100%`): Phase-aligned parallel dry/wet blend for natural vocal doubling or subtle pitch polish.

### OUTPUT
- **IN / GR / OUT METERS**: Dual RMS + Peak + Peak-Hold input/output meters with center Gain Reduction bar.
- **OUT GAIN** (`-18 to +18 dB`): Final output trim with protective soft-clip safety ceiling.
- **MIX** (`0–100%`): Latency-aligned global parallel dry/wet mix.
