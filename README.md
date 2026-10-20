# Vi Filter

Vi Filter is a native AUv2 / Standalone audio filter for Logic Pro, built with C++17 and JUCE 8. **v0.6.0 — Release Readiness.** Its bespoke UI is still intentionally deferred; use Logic's generic parameter view.

## Sound engine

- 12 dB/oct TPT Low-pass / High-pass / Band-pass, Cutoff 20–20 kHz, Resonance Q 0.5–12, Output Gain -24 to +24 dB.
- Drive 0–24 dB and Soft / Tube / Tape saturation. JUCE FIR anti-alias oversampling: 1×, 2×, 4×, constant reported host latency.
- 20 ms smooth automation and 40 ms crossfade on oversampling changes; independent Mono / Stereo states.
- Eight factory programs and user `.vifilterpreset` file backend; forwards-compatible versioned schema validation and migration of session data from v0.1–v0.3.

## v0.6.0 changes

- **CPU optimization:** precomputed/cached saturation transfer normalizations per Drive value. High-cost `pow`, `atan` and `tanh` normalizations are no longer repeated per channel and oversampled sample. DSP regression compares with v0.5 curves.
- **Real-time checks:** processor integration tests instrument ordinary C++ heap allocations while `processBlock` runs across 1/2/4× quality changes, irregular buffers including oversized blocks, and 44.1/48/96 kHz sample rates.
- **Universal macOS build:** CMake Xcode compiles both `arm64` (Apple Silicon) and `x86_64` (Intel) slices and CI verifies both with `lipo`.
- **AU validation:** CI verifies code signature and Info.plist and refreshes AudioComponentRegistrar before Apple's `auval` validation.
- **Distribution artifacts:** CI publishes a manual-install ZIP, an **unsigned system-wide `.pkg`** test installer and SHA256 checksums.
- **Manual Logic QA:** `Docs/LOGIC_QA.md` documents precise functional checks on real Macs.

## Build and test (macOS)

Requires Xcode, Command Line Tools, Git and CMake 3.22+. Pinned JUCE tag `8.0.16-security` is fetched on initial configure.

```sh
git clone https://github.com/markxxv/vifilter.git
cd vifilter
cmake -S . -B build -G Xcode -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target ViFilter_AU ViFilter_Standalone ViFilterDSPTests ViFilterAnalogTests ViFilterOversamplingTests ViFilterPresetTests ViFilterProcessorTests --parallel 2
ctest --test-dir build -C Release --output-on-failure
bash Scripts/package-macos.sh 0.6.0 build dist
```

The component is `build/ViFilter_artefacts/Release/AU/Vi Filter.component`. Check slices:

```sh
lipo -archs "build/ViFilter_artefacts/Release/AU/Vi Filter.component/Contents/MacOS/Vi Filter"
```

### Artifacts

| File | How to use |
| --- | --- |
| `ViFilter-0.6.0-macOS-universal.zip` | Unzip and manually copy `Vi Filter.component` to `~/Library/Audio/Plug-Ins/Components/` |
| `ViFilter-0.6.0-macOS-universal-unsigned.pkg` | Test installer targeting `/Library/Audio/Plug-Ins/Components/` system-wide; requires administrator authorization |
| `SHA256SUMS.txt` | SHA-256 hashes of the ZIP and PKG |

Download ZIP/PKG from the successful **GitHub Actions run's Artifacts** section. These are CI test artifacts, not a signed, notarized public release.

Local AU check after install:

```sh
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -v aufx Vfil Vifr
```

Restart Logic Pro or rescan Plug-in Manager, then look for `Vi Audio → Vi Filter`.

**Important compatibility boundary:** the CI runner is a genuine macOS machine and validates AU with Apple `auval`, but **Logic Pro is not installed on that runner**. Successful `auval` is not a substitute for a Logic Pro session on an Intel/Apple Silicon Mac. The final real-host checklist in [`Docs/LOGIC_QA.md`](Docs/LOGIC_QA.md) must be completed on actual Logic Pro installations.

### Benchmarks

`ViFilterProcessorTests` prints a wall-clock processing time and real-time factor for each selected quality. Measurements are comparative, not representative of every Mac or full Logic project. A passing allocation guard covers ordinary C++ `new`/`new[]` within the tested callbacks; it is not a proof of the absence of all possible real-time stalls.

### Release prerequisites

- Review JUCE and Vi Filter distribution licensing.
- Sign bundle using Developer ID Application and the installer using Developer ID Installer; notarize and staple before public distribution.
- Complete the Logic Pro checklist on real Macs, including state compatibility and plug-in delay compensation.
- Finish the bespoke UI only after technical and acoustic acceptance. There is **no GUI or built-in preset file picker yet**.

## Roadmap

1. v0.1 Foundation → v0.2 Core DSP → v0.3 Analog Engine → v0.4 Presets → v0.5 Engineering QA.
2. **v0.6 Release Readiness:** universal binaries, test installer, audio callback audits and real-host acceptance checklist.
3. **Next stage:** actual Logic Pro smoke tests and resolution of any discovered audio/host issues.
4. **Final stage:** separately art-directed UI, precise frequency response curve, preset browser, HiDPI/accessibility.
