# Vi Filter

**Vi Filter** is a native macOS AUv2 / Standalone audio effect, written in C++17 with JUCE 8. The custom GUI remains **deliberately deferred** until the final visual-design phase.

## v0.5.0 — Engineering QA

### DSP
- 12 dB/oct TPT state-variable Low-pass, High-pass and Band-pass filter.
- Cutoff 20–20,000 Hz, Resonance 0–1 (Q = 0.5–12), Output Gain -24…+24 dB.
- Post-filter saturation with Drive 0–24 dB and Soft, Tube, Tape digital waveshapers.
- 1× / 2× / 4× saturation oversampling with JUCE FIR anti-alias filters; constant reported host latency and 40 ms quality-switch fades.
- 20 ms smoothing for cutoff, resonance, output gain, drive and modes; Mono/Stereo support.

### Preset engine
- Eight immutable factory programs: Clean Start, Soft Piano, Velvet Lowpass, Warm Tape, Gentle Tube, Focused Mid, Low Cut Air, Vintage Radio.
- Factory programs are exposed via JUCE host program APIs (host-dependent visibility). **Custom** is program 0. A factory-to-Custom selection bug has been fixed in v0.5.0 and covered by a processor integration test.
- Backend save/load/list for user XML `*.vifilterpreset` files under `~/Library/Application Support/Vi Audio/Vi Filter/Presets/`. Safe file writes, no silent overwrite, size and filename validation.
- Full session state persistence and migration from bare v0.1–v0.3 `ViFilterParameters` to schema 1.
- Strict numeric parsing prevents malformed literals from silently becoming different parameter values; corrupt restores keep the current state.

**No GUI/file picker is implemented yet**. You can use Logic's host settings; user file functions are currently available to a future UI or programmatic harness.

### Integration and quality assurance

`ViFilterProcessorTests` instantiates the **actual JUCE audio processor** and checks:
- Factory-program selection, user edits reverting to Custom, and project-state roundtrips.
- Corrupt host-state rejection without destroying the current parameters.
- Host-style parameter changes (filter mode, drive, character, oversampling, resonance and cutoff).
- Mono and Stereo processing on 44.1/48/96 kHz; silent right-channel isolation.
- Short, irregular and **oversized processing blocks** (larger than the prepared block size).
- Rapid mode and quality changes while audio is processing; finite bounded output and constant reported latency.
- Release/reprepare after sample-rate changes.
- Informational wall-clock CPU measurements for 1× and 4× oversampling, with realtime throughput and relative-cost reporting. Benchmarks are **not portable absolute performance guarantees**.

The other four tests cover filter response, analog waveshaping, alias suppression, and preset file integrity. macOS CI builds AU and Standalone, runs all five test executables, then installs the AU in the runner and runs Apple's **`auval`** registration/validation.

**Limitations:** GitHub Actions cannot operate your personal Logic project or substitute for listening tests. Manual inspection in Logic Pro is still needed, especially host automation and latency compensation while switching quality, restoring old projects, and macOS Intel/Apple Silicon distribution.

## Build and test

Requires macOS, Xcode Command Line Tools, Git and CMake 3.22+. JUCE dependency pinned to `8.0.16-security`.

```sh
git clone https://github.com/markxxv/vifilter.git
cd vifilter
cmake -S . -B build -G Xcode -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --target ViFilter_AU ViFilter_Standalone ViFilterDSPTests ViFilterAnalogTests ViFilterOversamplingTests ViFilterPresetTests ViFilterProcessorTests
ctest --test-dir build -C Release --output-on-failure --verbose
```

AU component path: `build/ViFilter_artefacts/Release/AU/Vi Filter.component`

Install for the current macOS user:

```sh
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
cp -R "build/ViFilter_artefacts/Release/AU/Vi Filter.component" "$HOME/Library/Audio/Plug-Ins/Components/"
auval -v aufx Vfil Vifr
```

Restart Logic Pro, or rescan in Plug-in Manager, and use **Vi Audio → Vi Filter** in Audio FX.

### Manual Logic Pro checklist

- Load Mono and Stereo on instrument/audio tracks.
- Open Logic's generic controls and automate Cutoff, Resonance, Mode and Drive.
- Test 1× / 2× / 4× changes; check audible transitions and fixed reported latency under PDC.
- Save, close and reload sessions, including a project saved by v0.3.
- Compare levels at high resonance and Drive; keep monitors at a safe level.
- Verify CPU load and playback at 44.1, 48 and 96 kHz with small and large buffer sizes.

## Roadmap

1. v0.1.0 — Foundation.
2. v0.2.0 — Core DSP.
3. v0.3.0 — Analog Engine.
4. v0.4.0 — Preset Engine.
5. **v0.5.0 — Engineering QA**: processor tests, CPU measurements, AU validation (current).
6. **Next:** v0.6.0 — final technical reliability pass: inspect and fix any findings from AU validation, Logic interoperability, real-time safety, and release build/distribution preparations.
7. **Final:** independent UI/UX art direction, responsive HiDPI controls, accurate frequency-response visualization, integrated presets browser, accessibility.

## License

JUCE licensing and Vi Filter's redistribution license require review before publication. See [JUCE licensing](https://juce.com/legal/).
