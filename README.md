# Vi Filter

**Vi Filter v0.7.0** — native macOS Audio Unit (AUv2) and standalone filter, JUCE 8 / C++17.

## The visual interface

The plugin now has its own **monochrome, vector-rendered editor** at 1440×820 reference size, scalable down to 1100×650. Graphite and black surfaces, silver highlights, engraved tick marks and six custom-drawn knobs are rendered with JUCE Graphics. **No raster/knob filmstrip dependencies** are required; Retina output remains sharp.

- Central frequency-response curve is derived mathematically from the existing TPT state-variable filter. Its draggable node controls Cutoff; the line includes Filter Mode, Resonance, Input Trim, Output Gain and Mix. A nonlinear saturation stage has no fixed linear transfer function and is not represented by this line.
- The optional spectrum overlay is a **real 2048-point FFT** of the processed output, read from a preallocated SPSC FIFO on the message thread, not an animation or placeholder. Analyzer toggle and dB/linear graph mode are in the chart.
- Six functional knobs: Cutoff, Resonance, Drive, Output Gain, **Input Trim** and **Mix**. Trim and Mix are new automatable DSP parameters, defaults 0 dB and 100%; old sessions load with these defaults. Drag vertically; hold Shift for fine adjustment, use the wheel for small changes, double-click a knob to reset.
- Filter Mode LP/HP/BP, Analog Character Soft/Tube/Tape, Oversampling 1×/2×/4× are functioning segmented selectors.
- Factory preset browser with previous/next buttons, saved user presets and **Save Current As**. Saved files go to `~/Library/Application Support/Vi Audio/Vi Filter/Presets/`. No overwrite of existing user files.
- A/B snapshot comparison while the editor is open, and Undo/Redo using the APVTS UndoManager. A/B buffers are transient and do not replace the saved host-state format.
- Every DSP parameter remains host-automatable; the standalone UI and Logic AU use the same processor.

The design follows the approved black/white mockup as closely as JUCE vector primitives allow. Precise visual fidelity must be reviewed using the **CI-generated offscreen editor screenshots** and the plug-in in Logic; hardware font rasterization and GPU/window scaling can differ.

## DSP

12 dB/oct TPT LP/HP/BP, 20–20kHz Cutoff, Resonance, post-filter Drive 0–24 dB, Soft/Tube/Tape waveshaping, 1×/2×/4× FIR oversampling, gain smoothing, fixed reported latency. Input Trim applies before filtering, and Mix crossfades a latency-aligned dry signal with the saturated result.

Presets and state restore remain backward compatible with v0.1–v0.6. Plugin identity and AU manufacturer/subtype codes are unchanged.

## Universal macOS build

Requires macOS with Xcode + Command Line Tools, Git, CMake 3.22+ and network for the initial pinned JUCE download.

```sh
git clone https://github.com/markxxv/vifilter.git
cd vifilter
cmake -S . -B build -G Xcode -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target ViFilter_AU ViFilter_Standalone ViFilterDSPTests ViFilterAnalogTests ViFilterOversamplingTests ViFilterPresetTests ViFilterProcessorTests ViFilterResponseTests ViFilterEditorTests --parallel 2
ctest --test-dir build -C Release --output-on-failure
bash Scripts/package-macos.sh 0.7.0 build dist
```

Successful GitHub Actions runs attach a **universal Audio Unit ZIP**, **unsigned local-test PKG**, checksum file, and offscreen PNG previews at 1440×820 and 1100×650. Check the run's Artifacts panel.

The `.pkg` is **unsigned and unnotarized**: for local testing only; shipping requires Developer ID signing and notarization.

## Installation

**Local development builds install the AU automatically.** JUCE's `COPY_PLUGIN_AFTER_BUILD TRUE` copies the finished `Vi Filter.component` to `~/Library/Audio/Plug-Ins/Components/` after a successful `ViFilter_AU` build, replacing the previous local copy. This is per-user installation, with no `sudo` or manual copying.

After pulling `main`, configure the existing build directory once, then build the Audio Unit as usual:

```sh
cmake -S . -B build-local -G Xcode
cmake --build build-local --config Release --target ViFilter_AU --parallel 2
```

A regular `cmake --build build-local --config Release` that includes the AU target also triggers this copy. Building only the test executables does not. The Standalone app is not copied to the AU directory. **Building does not force Logic Pro to reload an already-open plug-in**: reopen Logic or rescan in Plug-in Manager after updating the component.

The CI ZIP is also available for manual installation on another Mac. The AU appears as **Vi Audio: Vi Filter**. Verify in Terminal:

```sh
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -v aufx Vfil Vifr
```

The CI tests the processor, DSP, preset migration, response math, and draws PNG screenshots of the real JUCE interface without using a simulated image. **Manual validation in Logic Pro on the target Mac is still required**, especially automation, real-time analyzer load, clicks on parameter changes and latency compensation; see [Docs/LOGIC_QA.md](Docs/LOGIC_QA.md).

## License

JUCE licensing and distribution rights require review before shipping. See [JUCE licensing](https://juce.com/legal/).
