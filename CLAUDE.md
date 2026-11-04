# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

**Doppler Saturator**: a JUCE (CMake) VST3 plugin, split off from Multiband Saturator (`../MultibandSat`, repo `SofusLindberg/MultibandSatV2`) on 2026-11-03. It is one band of Multiband Saturator's zero-latency engine (`satdsp::Engine`, the "0 ms - 24 / 48 dB/oct" modes), moved from side to side by an LFO. A minimum-phase crossover that moves rotates the phase of the dry sum over time, so the pitch bends while the band moves. That is the Doppler effect, used here on purpose (the user: "Dobbler-effekt (ptitch ændres, når man trækker båndet henover frekvenserne) [...] er faktisk super fedt"). Windows/MSVC only in practice. Remote: `https://github.com/SofusLindberg/DopplerSat` (private).

## Same rules as Multiband Saturator

Read `../MultibandSat/CLAUDE.md` for the shared workflow. In short:
- **`FREMDRIFT.md`** (Danish, newest entry first) is the progress log. At session start, read it and tell the user where we got to. Log every prompt in the same turn. "Gem og luk" = log, commit, push, install (verified), and ask the user to type `/exit`.
- **`KRAV.md`** lists every requirement. Never remove or change one unless the user explicitly asks. Add new requests with the date and the user's words.
- Every installed version gets its own number: bump the minor version in `CMakeLists.txt` (`project(DopplerSat VERSION 1.x.0)`) before each new install. CMake derives `PRODUCT_NAME "Doppler Saturator 1.x"` and `PLUGIN_CODE D1xx` (Multiband Saturator uses `M3xx`, same manufacturer code `Dtnv`). Never delete old versions. Verify the install by comparing file hashes.
- Commit and push yourself after every completed prompt with changes (Danish commit message).
- Code comments in Danish with ASCII spelling (`aa`/`oe`/`ae`), JUCE formatting style. `.ps1` files ASCII only (PowerShell 5.1 reads them as ANSI).

## Layout

- `CMakeLists.txt` + `Source/`. JUCE **9.0.3** must be the sibling folder `../JUCE` (shared with MultibandSat).
- `Source/SatDsp.h` is a **copy** of MultibandSat's `SatDsp.h` (3.18), unchanged. Since 1.1 the sound uses `Source/DopplerCore.h` (crossover design helpers, `Biquad2` / `ApChain2` from SatDsp.h); the editor still uses `makeCurveModel` to draw the band. Changes in SatDsp.h do not reach Multiband Saturator, and vice versa.
- `Source/PluginProcessor.*`: 12 parameters (`freq`, `amount`, `width`, `mode`, `rate`, `sync`, `syncrate`, `shape`, `offset`, `drywet`, `outgain`, `bypass`). `drive` and `type` were removed in 1.1 (user request: no saturation). `getBypassParameter()` returns `bypass`, so it is the DAW's bypass too. Never rename IDs.
- `Source/PluginEditor.*`: fixed 900 × 600 window in Multiband Saturator's design 2A (`HardwareLook` copied). Display (`PlotArea`, output spectrum only), `LfoView` (the LFO curve with a moving dot) and four modules: LFO, Band, Edges (the two 0 ms buttons, a "?" help box copied from Multiband Saturator, `SlopeView`), Output. Every control is a `ParamControl<>`: right-click opens `showParamMenu` (the DAW's own items via `getHostContext()` when the host has them, then "Select for automation", "Reset", and a short how-to).
- `installer-vst3.ps1` (elevated copy + hash check) and `ableton-automation.ps1` (Options.txt, a copy of MultibandSat's).
- `tests/host`: `HostSmoke` (loads the built VST3; parameter + automatable check, pitch deviation and a 24 vs 48 pitch table, every shape / mode / sync / dry-wet / bypass for NaN and level jumps, CPU; `--compare <old.vst3>` checks the sound is identical, `--bench` times CPU), `EditorShot` (PNGs `doppler1..3`, `doppler4_help`, `doppler5_menu`, plus a right-click check) and `DspBench` (CPU of oversampling / DopplerCore / setParams, no host). Hosted VST3 parameters are not `RangedAudioParameter`: HostSmoke's `normFor` converts real values to 0..1 (before 1.1 it did not, so 1.0's logged pitch numbers were measured at wrong settings).

## Build, test and install

```sh
cmake -B build -G "Visual Studio 18 2026"
cmake --build build --config Release --target DopplerSat_VST3
cmake -S tests/host -B tests/host/build
cmake --build tests/host/build --config Release
tests/host/build/HostSmoke_artefacts/Release/HostSmoke.exe "build/DopplerSat_artefacts/Release/VST3/Doppler Saturator 1.1.vst3"
tests/host/build/HostSmoke_artefacts/Release/HostSmoke.exe "build/DopplerSat_artefacts/Release/VST3/Doppler Saturator 1.1.vst3" --compare "<copy of the previous version .vst3>"
tests/host/build/EditorShot_artefacts/Release/EditorShot.exe "<folder>"
```

Install: `Start-Process powershell -Verb RunAs -Wait -ArgumentList '-ExecutionPolicy','Bypass','-File','<repo>\installer-vst3.ps1','-NoPause'` (UAC; run it in the background, the box can hide behind other windows).

## How it works

- **Engine use:** the sound is `satdsp::DopplerCore`: exactly what `satdsp::Engine` does at 0 % drive (Engine's BandSat is off then, so the output is only the dry sum T = tch(gch0(h23(x))), the allpasses of all crossovers). Only band `kBand` (1) moves; bands 0, 2 and 3 are parked far outside the audible range (10–12 Hz and 40–45 % of the processing rate), so their crossovers are static allpasses and add no pitch change. Edge Q is fixed off. Keep it bit-identical: `HostSmoke --compare` against the previous build must say "SAMME LYD".
- **LFO → band:** every `kChunk` = 32 host samples, the LFO value v (−1..1, `lfoValue` = shape + offset, clipped) moves the band's middle to `log2(freq) + v · amount · 3` octaves (`bandEdgesFor`, clamped to 20 Hz–20 kHz). Then `core.setParams` runs (it designs only the allpass part of band 1's four crossovers, and only when the edges moved), and the chunk goes through the 4× oversampler and `core.process`. The 4× oversampling (60 samples latency) stays: at 1× the top crossovers could not go above 20.7 kHz, which changes the sound when the band is high.
- **Shapes:** Sine, Triangle (in phase with sine), Square with raised-cosine transitions of about 8 ms (`squareW` = 0.008 · rate cycles, ≤ 0.25), Random = a cosine glide from one random value to the next each cycle.
- **Sync:** `syncBeats` (bars use the host's time signature). While the host plays, the phase is `ppq / beats` (locked to the bar); otherwise it free-runs at the host tempo (120 when unknown).
- **Dry/Wet:** the dry signal is delayed by the oversampler's latency (`juce::dsp::DelayLine`, Lagrange 3rd, fractional) and mixed linearly. Between dry and wet the moving allpass phase makes phaser notches.
- **Drawing:** the editor builds `EngineParams` from the band's current edges (`bandLoNow` / `bandHiNow`, written by the audio thread) with a nominal drive of 100 % (`kShowDrive`, the sound has none) and draws `CurveModel::contribPct` for band 1, so the band looks like 1.0 at its default.
