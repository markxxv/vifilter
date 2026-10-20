# Vi Filter

Vi Filter is a native macOS **Audio Unit v2** and standalone audio filter, built with C++17 and JUCE 8. The custom UI is **reserved for the final stage**; current builds use the host's generic parameter interface.

## v0.4.0 — Preset Engine

### Audio engine

- 12 dB/oct topology-preserving Low-pass / High-pass / Band-pass filter.
- Cutoff 20–20,000 Hz, Resonance 0–1 (Q = 0.5–12), Output Gain -24…+24 dB.
- Drive 0–24 dB, Soft / Tube / Tape digital waveshaping.
- Saturation-stage oversampling 1× / 2× / 4× with FIR anti-alias filters, fixed reported latency, 40 ms mode-change crossfades.
- Mono and stereo support. Parameter ramps of 20 ms.

### Factory programs

Built-in bank (program 0 is **Custom**):

| Program | Intent |
| --- | --- |
| Clean Start | Open filter, no coloration |
| Soft Piano | Subtle, rounded piano |
| Velvet Lowpass | Softened highs with gentle tube drive |
| Warm Tape | Warm coloration with tape-type waveshaping |
| Gentle Tube | Controlled harmonic richness |
| Focused Mid | Midrange-focused band-pass |
| Low Cut Air | Gentle high-pass cleaning |
| Vintage Radio | Narrower, more saturated band-pass |

Factory programs are **host programs** exposed through JUCE's `getNumPrograms` / `getProgramName` / `setCurrentProgram`. Whether they appear as a menu depends on the host; some Logic Pro AU views may not expose this selector. This is not yet an in-plugin preset browser. Factory selection notifies parameter changes to the host; editing a factory parameter switches the current program to Custom.

### User preset files

The engine can save, load and enumerate `*.vifilterpreset` XML files. It uses atomic temporary-file replacement, rejects invalid names and malformed files, limits input size to 1 MiB, and does not silently overwrite an existing preset.

The default folder on macOS is:

```text
~/Library/Application Support/Vi Audio/Vi Filter/Presets/
```

The backend calls are available in `ViFilterProcessor`:

```cpp
juce::Result saveUserPreset (const juce::String& name, juce::File* savedFile = nullptr);
juce::Result loadUserPreset (const juce::File& file);
juce::Array<juce::File> listUserPresets() const;
```

**No file picker or Save/Load UI exists yet.** These methods are for integration into the final UI, and must be called from a non-audio thread. For immediate use in Logic, save plugin settings or the Logic project using the host's own controls.

### Backward-compatible project state

- New project sessions are wrapped in `<ViFilterSession schema="1" program="...">` with the existing `ViFilterParameters` tree inside.
- Bare `ViFilterParameters` XML from **v0.1–v0.3** is accepted and migrated.
- Missing parameters are set to stable documented defaults; unknown parameter IDs are ignored.
- Invalid numeric values, duplicated parameters and unknown future schemas are rejected **without modifying the live state**.
- Factory program index is persisted alongside exact parameters. User files always restore as Custom.

Note: use a backup project when testing early-stage plugin upgrades. Logic Pro version compatibility and host program-menu behavior still need an actual Logic session test.

## Build and test (macOS)

Requires Xcode and Command Line Tools, CMake 3.22+, Git, internet access for the first JUCE download. JUCE is pinned to `8.0.16-security`.

```sh
git clone https://github.com/markxxv/vifilter.git
cd vifilter
cmake -S . -B build -G Xcode -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --target ViFilter_AU ViFilter_Standalone ViFilterDSPTests ViFilterAnalogTests ViFilterOversamplingTests ViFilterPresetTests
ctest --test-dir build -C Release --output-on-failure
```

The AU output is `build/ViFilter_artefacts/Release/AU/Vi Filter.component`.

```sh
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
cp -R "build/ViFilter_artefacts/Release/AU/Vi Filter.component" "$HOME/Library/Audio/Plug-Ins/Components/"
auval -v aufx Vfil Vifr
```

Restart Logic Pro or rescan Plug-in Manager, then find **Vi Audio → Vi Filter** in Audio FX. Start at safe monitoring levels: high resonance/drive can boost narrow bands.

### Automated tests

- `ViFilterDSPTests`: filter response, DC, stereo isolation and five sample rates.
- `ViFilterAnalogTests`: waveshaping, bypass and delay alignment.
- `ViFilterOversamplingTests`: attenuation of a folded saturation harmonic.
- `ViFilterPresetTests`: factory data validity, XML and disk roundtrips, legacy migration, corrupt data, safety against failed loads, no unintended overwrite.

CI compiles AU + Standalone and runs all four test targets on macOS. **Successful CI compilation is not a substitute for listening tests and in-Logic validation.**

## Project structure

- `Source/FilterDSP.h` — state-variable filter
- `Source/AnalogDSP.h` — waveshaping and delay
- `Source/PresetEngine.h/.cpp` — factory bank, XML schema/migrations and file operations
- `Source/PluginProcessor.h/.cpp` — realtime processing, APVTS and native host programs
- `Tests/` — DSP, oversampling and preset tests
- `.github/workflows/build.yml` — macOS CI

## Roadmap

1. v0.1.0 — Foundation.
2. v0.2.0 — Core DSP.
3. v0.3.0 — Analog Engine.
4. **v0.4.0 — Preset Engine** (current).
5. **Next:** v0.5.0 — engineering QA: comprehensive processor/host-state tests, tougher realtime/samplerate checks, real Logic Pro validation.
6. **Final:** UI/UX exploration, refined custom controls, interactive response curve, presets browser, HiDPI and accessibility.

## Licensing

Review [JUCE licensing](https://juce.com/legal/) before distributing or selling binary builds. A final distribution license for Vi Filter is not yet selected.
