# Vi Filter — Logic Pro hardware QA checklist (v0.6)

GitHub's macOS CI performs AU compilation, DSP/processor tests and `auval` on a Mac runner. It **does not** have Logic Pro installed. Real host compatibility is **not confirmed** until this checklist is run in Logic Pro on an Intel Mac and on an Apple Silicon Mac (or both architectures verified separately on suitable machines).

## Install locally for testing

1. Download the CI artifact (or build from `main`), unzip `ViFilter-0.6.0-macOS-universal.zip`.
2. Quit Logic Pro; back up any previously installed `Vi Filter.component` before replacing it.
3. Copy `Vi Filter.component` to `~/Library/Audio/Plug-Ins/Components/`.
4. Open Terminal, inspect architecture and signature:

```sh
component="$HOME/Library/Audio/Plug-Ins/Components/Vi Filter.component"
lipo -archs "$component/Contents/MacOS/Vi Filter"
codesign --verify --deep --strict --verbose=2 "$component"
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -v aufx Vfil Vifr
```

5. Open Logic Pro → Settings → Plug-in Manager. Confirm Vi Audio / Vi Filter passes validation; if not, rescan and attach Log/Crash Report.

## Functional recording checklist

- [ ] AU loads on a mono audio track (Mac architecture and Logic version noted).
- [ ] AU loads on a stereo audio track.
- [ ] Verify Filter Mode LP/HP/BP, Cutoff sweep 20–20,000 Hz, and Resonance including high Q.
- [ ] Verify Soft/Tube/Tape with Drive 0, 6, 12, 24 dB, levels safe.
- [ ] Switch 1× ↔ 2× ↔ 4× while playing, monitor clicks and latency PDC; listen on headphones first.
- [ ] Automate Cutoff, Resonance, Mode, Drive; save, quit, reopen and verify automation.
- [ ] Save/reopen factory and Custom programs, verify names (where host exposes programs) and exact values.
- [ ] Reopen a project made with v0.3.0 (preferably a backup), confirm original parameter values restored.
- [ ] Change project rate 44.1/48/96 kHz; repeat with I/O buffer 32/128/1024 samples where supported.
- [ ] Compare dry vs processed time alignment with Logic plugin delay compensation enabled.
- [ ] Bounce audio both in real time and offline, compare with playback, check numerical anomalies/silence.
- [ ] Load 10 instances, observe Logic CPU/latency and record glitches.
- [ ] Test on Apple Silicon Logic running natively and Intel Logic running natively; record the outcome for each separately.

## Report

Fill a short report for each Mac: chip/arch, macOS version, Logic version, audio interface, project rate, buffer size, test verdict, exact steps for failure, crash report or screenshots.

The `*.pkg` artifact is **unsigned and unnotarized**. It is a test installer, not a production-distribution installer. Public distribution requires a Developer ID installer/app signature, proper notarization/stapling, and license review. The standalone app is built and validated but is not included in the installer.
