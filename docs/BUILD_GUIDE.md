# Build & Installation Guide

## Option A — Automatic Cloud Build via GitHub Actions (Recommended, Zero Local Compilers)
1. Push this `ALR-VOICE-COLOR` folder to a GitHub repository (`main` branch).
2. Open the **Actions** tab on GitHub — the workflow **Build & Release ALR VOICE COLOR** runs automatically on Windows, macOS (Universal arm64+x86_64), and Linux.
3. When it turns green (~5–8 minutes), open the **Releases** section on the right sidebar of your repository and download the compiled ZIP for your OS (`ALR-VOICE-COLOR-Windows-x64.zip`, `ALR-VOICE-COLOR-macOS-Universal.zip`, or `ALR-VOICE-COLOR-Linux-x64.zip`).
4. Follow the `INSTALAR_*.txt` file inside the downloaded ZIP to copy the `.vst3` / `.component` into your DAW folder.

---

## Option B — Local Compilation Prerequisites
- **CMake** 3.22 or newer
- **C++20 Compiler**:
  - **Windows**: Visual Studio 2022 (Desktop Development with C++ workload)
  - **macOS**: Xcode 14+ with Command Line Tools (`xcode-select --install`)
  - **Linux**: GCC 11+ or Clang 14+ plus JUCE Linux dependencies (`libasound2-dev`, `libfreetype6-dev`, `libx11-dev`, `libxinerama-dev`, `libxrandr-dev`, `libxcursor-dev`, `libxcomposite-dev`, `libgtk-3-dev`, `pkg-config`)
- **Git & Internet access** during the first CMake configure step (downloads **JUCE 8.0.4** automatically via `FetchContent`).

## 1. Run Standalone C++20 DSP Tests (No JUCE Required)
```bash
bash scripts/run_dsp_tests.sh
```

## 2. Build VST3, AU (macOS), and Standalone Plugin

### Windows (PowerShell / CMD)
```bat
scripts\build_windows.bat
```
Or manually:
```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

### macOS & Linux
```bash
bash scripts/build_macos_linux.sh
```

## 3. Installing Compiled Plugins into Your DAW
Copy the built artifacts from `build/ALRVoiceColor_artefacts/Release/`:

- **Windows VST3**: Copy `VST3/ALR VOICE COLOR.vst3` to:
  `C:\Program Files\Common Files\VST3\`
- **macOS VST3**: Copy `VST3/ALR VOICE COLOR.vst3` to:
  `~/Library/Audio/Plug-Ins/VST3/` or `/Library/Audio/Plug-Ins/VST3/`
- **macOS Audio Unit (AU)**: Copy `AU/ALR VOICE COLOR.component` to:
  `~/Library/Audio/Plug-Ins/Components/` or `/Library/Audio/Plug-Ins/Components/`
- **Linux VST3**: Copy `VST3/ALR VOICE COLOR.vst3` to:
  `~/.vst3/`

Rescan plugins in **FL Studio**, **Ableton Live**, **Logic Pro**, **Cubase**, **Reaper**, or **Studio One**.
