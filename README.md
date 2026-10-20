# Vi Filter

**Vi Filter** is a native Audio Unit effect for Apple Logic Pro, built with C++ and [JUCE](https://juce.com/).

> **Stage 1 — Foundation.** This is a working audio-processing skeleton, **not yet a filter**. It passes mono/stereo audio through with a host-automatable Output Gain parameter (-24 to +24 dB), a 20 ms gain ramp, and saved/restored plugin state. The custom interface is intentionally postponed until the final phase.

## Requirements

- macOS, Xcode with Command Line Tools, CMake 3.22+ and Git
- Internet access on first CMake configure (JUCE is downloaded automatically)
- Logic Pro for AU hosting, or the standalone build for basic audio-path checks

JUCE is pinned to **8.0.16-security** in `CMakeLists.txt` for reproducible builds.

## Build

```sh
git clone https://github.com/markxxv/vifilter.git
cd vifilter
cmake -S . -B build -G Xcode -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --target ViFilter_AU ViFilter_Standalone
```

The resulting files will be under:

- `build/ViFilter_artefacts/Release/AU/Vi Filter.component`
- `build/ViFilter_artefacts/Release/Standalone/Vi Filter.app`

To install the AU for the current macOS account:

```sh
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
cp -R "build/ViFilter_artefacts/Release/AU/Vi Filter.component" "$HOME/Library/Audio/Plug-Ins/Components/"
```

Restart Logic Pro (or rescan in Plugin Manager), then look for **Vi Filter** under **Vi Audio** in the Audio FX menu. For an AU registration check:

```sh
auval -v aufx Vfil Vifr
```

The plugin currently has **no custom editor**; use the host's generic parameter controls to test Output Gain. Output Gain must remain automatable and must persist after saving/reopening a Logic project.

## Architecture

- `Source/PluginProcessor.h/.cpp` — audio callback, channel validation, parameter management and state persistence
- `Source/FilterDSP.*` — future DSP module (not implemented in Stage 1)
- `Source/Presets.*` — future factory/user presets (not implemented in Stage 1)
- `Source/ResponseCurve.*` and editor — **final visual phase only**
- `.github/workflows/build.yml` — macOS build check on pushes and pull requests

## Development roadmap

1. **Foundation** — AUv2 + Standalone, mono/stereo audio, automatable Output Gain, saved state, macOS CI.
2. **Core DSP** — low-pass, high-pass, band-pass, cutoff, resonance; click-free automation and response tests.
3. **Analog character** — saturation/drive and optional 2x/4x oversampling; aliasing and CPU checks.
4. **Presets & behavior** — factory presets, user preset files, versioned parameter state and regression tests.
5. **Final UI** — deliberate visual direction, custom controls, precise frequency-response visualization, accessibility and HiDPI.

**Important:** The first commit provides source code and a CI build workflow; compilation in Logic must still be verified on macOS. Do not consider AU compatibility validated until the macOS build and Logic test pass.

## Licensing

JUCE licensing depends on the use and distribution of the final plugin. Review [JUCE licensing](https://juce.com/legal/) before publishing binaries or selling the plugin. A distribution license for Vi Filter has not been selected yet.
