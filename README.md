# Vi Filter

**Vi Filter** is a native Audio Unit (AUv2) audio filter for Logic Pro, built with C++17 and [JUCE](https://juce.com/).

> **Stage 2 — Core DSP (v0.2.0).** Low-pass / high-pass / band-pass filters are implemented. The custom interface is deliberately reserved for the **final** development phase; use the host's generic parameter view.

## Implemented

- Topology-preserving state-variable (TPT/SVF) filter, 12 dB/octave, two independent channel states.
- Low-pass, high-pass, and band-pass modes with a 20 ms crossfade between outputs (filter history is not reset during mode changes).
- **Cutoff** 20 Hz–20 kHz with logarithmic 20 ms smoothing. Effective cutoff is clamped to 45% of sample rate at low host sample rates.
- **Resonance** 0–1 (mapping to Q = 0.5–12 quadratically) with 20 ms smoothing.
- **Output Gain** -24 to +24 dB with 20 ms smoothing.
- Host-automatable parameters with serialized project state via JUCE AudioProcessorValueTreeState.
- Native AUv2 and Standalone targets; mono and stereo buses.
- Portable DSP unit tests: frequency response, DC behaviour, channel isolation, reset and stability sweeps over five sample rates.

## Build (macOS)

Requires Xcode + Command Line Tools, CMake 3.22+ and Git; the initial CMake run downloads pinned JUCE tag **8.0.16-security**.

```sh
git clone https://github.com/markxxv/vifilter.git
cd vifilter
cmake -S . -B build -G Xcode -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --target ViFilter_AU ViFilter_Standalone ViFilterDSPTests
ctest --test-dir build -C Release --output-on-failure
```

Output:

- `build/ViFilter_artefacts/Release/AU/Vi Filter.component`
- `build/ViFilter_artefacts/Release/Standalone/Vi Filter.app`

Install for the current account and restart Logic Pro (or rescan in Plug-in Manager):

```sh
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
cp -R "build/ViFilter_artefacts/Release/AU/Vi Filter.component" "$HOME/Library/Audio/Plug-Ins/Components/"
auval -v aufx Vfil Vifr
```

In Logic Pro, add **Vi Filter** under **Vi Audio** as Audio FX. Use generic host controls to automate **Filter Mode**, **Cutoff**, **Resonance** and **Output Gain**. Verify those values survive saving and reopening a project. Test at safe monitor levels; high resonance can amplify narrow frequency bands.

Portable DSP-only check (any C++17 compiler; JUCE not required):

```sh
c++ -std=c++17 -O2 -ISource Tests/FilterDSPTests.cpp -o /tmp/vifilter-dsp-tests
/tmp/vifilter-dsp-tests
```

## Code organization

- `Source/FilterDSP.h` — portable TPT filter coefficients and realtime audio processing.
- `Source/PluginProcessor.h/.cpp` — AU processor, smoothing, bus layout and state persistence.
- `Tests/FilterDSPTests.cpp` — self-contained DSP regression tests.
- `.github/workflows/build.yml` — macOS build and DSP tests on every `main` push.

## Roadmap

1. Foundation — AUv2 and Standalone, saved state and gain automation (done).
2. Core DSP — LP / HP / BP, cutoff, resonance, smooth automation and tests (this stage).
3. Analog character — saturation, drive, 2x/4x oversampling; anti-aliasing and CPU checks.
4. Presets and behavior — factory/user presets, session compatibility and regression tests.
5. **Final UI** — detailed visual exploration and polish, then bespoke JUCE controls and response visualization. No visual implementation before this stage.

**Validation:** C++ unit tests alone do not demonstrate AU compatibility. A successful macOS CI build and actual Logic Pro test are required before calling the plugin fully verified.

## License

JUCE licensing depends on final use and distribution. Review [JUCE licensing](https://juce.com/legal/) before releasing binaries or selling the plugin. Vi Filter distribution licensing is not yet decided.
