# Vi Filter

**Vi Filter** is a native AUv2 / Standalone audio filter for Logic Pro, built with C++17 and JUCE 8.

**v0.3.0 — Analog Engine.** A DSP-first project. **Custom UI is deliberately deferred to the final development phase**; use Logic Pro's generic host parameter controls.

## Features

- 12 dB/oct topology-preserving state-variable **Low-pass / High-pass / Band-pass** filter.
- **Cutoff** 20–20,000 Hz, **Resonance** 0–1 (Q = 0.5–12), **Output Gain** -24…+24 dB.
- **Drive** 0…24 dB; 0 dB is a mathematically exact waveshaper bypass.
- Three nonlinear **Analog Character** choices: Soft (symmetric tanh), Tube (asymmetric biased tanh), Tape (atan). These are digital waveshapers, not circuit-level hardware emulations.
- **Oversampling** 1x / 2x / 4x on the saturation stage, using JUCE high-quality equiripple half-band FIR anti-aliasing filters and integer latency.
- Fixed reported host latency equal to the slowest oversampling mode, with integer delay compensation for other modes. Live quality changes use a 40 ms crossfade. Quality is intentionally not host-automatable.
- 20 ms parameter smoothing, including Cutoff, Resonance, Drive, Analog Character and Output Gain. Filter modes crossfade without resetting state.
- Mono/stereo processing and session state restoration using JUCE AudioProcessorValueTreeState.
- No heap allocations in the audio callback; processing scratch buffers are allocated in `prepareToPlay`.

**Routing:** input → filter → saturation with selected oversampling → output gain. The nonlinear stage is **post-filter**; resonance feedback itself remains linear. A high Q setting can boost the filtered signal into the saturator significantly.

## Build and install (macOS)

Requires Xcode, Command Line Tools, Git, CMake 3.22+, and network access for the initial fetch of pinned JUCE `8.0.16-security`.

```sh
git clone https://github.com/markxxv/vifilter.git
cd vifilter
cmake -S . -B build -G Xcode -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --target ViFilter_AU ViFilter_Standalone ViFilterDSPTests ViFilterAnalogTests ViFilterOversamplingTests
ctest --test-dir build -C Release --output-on-failure
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
cp -R "build/ViFilter_artefacts/Release/AU/Vi Filter.component" "$HOME/Library/Audio/Plug-Ins/Components/"
auval -v aufx Vfil Vifr
```

Restart Logic Pro (or rescan in Plug-in Manager). In Audio FX, find **Vi Audio → Vi Filter**. Test at safe monitoring levels, especially at high resonance/drive. Check session restoration, automation and quality changes, and verify latency compensation with a track comparison. AU requires independent validation in Logic Pro; a successful CI build does not prove in-host behavior.

## Tests

- `Tests/FilterDSPTests.cpp`: LP/HP/BP frequency response, DC rejection, channel isolation and sample-rate sweeps.
- `Tests/AnalogDSPTests.cpp`: exact zero-drive bypass, finite waveshaping, asymmetry and integer delay alignment. Run standalone with `c++ -std=c++17 -O2 -ISource Tests/AnalogDSPTests.cpp -o /tmp/vi-analog && /tmp/vi-analog`.
- `Tests/OversamplingTests.cpp`: JUCE oversampling regression comparing the folded 21 kHz harmonic from a driven 9 kHz sine at 1x, 2x and 4x.
- `.github/workflows/build.yml`: builds AU, Standalone and all tests on macOS, then runs CTest.

Oversampling always adds reported delay (even when 1x is selected) so switching modes does not alter the DAW's latency compensation. Sample-rate changes call `prepareToPlay` and refresh latency; quality changes do not allocate on the audio thread.

## Roadmap

1. v0.1.0 Foundation — JUCE project and native plugin skeleton.
2. v0.2.0 Core DSP — LP/HP/BP, automation and filter checks.
3. v0.3.0 Analog Engine — drive, three characters, anti-aliased oversampling and latency-aligned mode changes.
4. **Next:** v0.4.0 factory/user presets, state migration and host regression checks.
5. **Final:** custom visual design, HiDPI controls, frequency-response display and detailed UI polish.

## Licensing

Review [JUCE licensing](https://juce.com/legal/) before distributing or selling compiled plug-ins. Final Vi Filter licensing has not been selected yet.
