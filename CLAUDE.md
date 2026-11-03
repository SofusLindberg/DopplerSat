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
- `Source/SatDsp.h` is a **copy** of MultibandSat's `SatDsp.h` (3.18). Only `Engine`, `BandSat`, `makeCurveModel` (zero-latency path) and the shapers are used. Changes there do not reach Multiband Saturator, and vice versa.
- `Source/PluginProcessor.*`: 14 parameters (`freq`, `amount`, `width`, `drive`, `type`, `mode`, `rate`, `sync`, `syncrate`, `shape`, `offset`, `drywet`, `outgain`, `bypass`). Never rename IDs.
- `Source/PluginEditor.*`: fixed 900 × 600 window in Multiband Saturator's design 2A (`HardwareLook` copied). Display (`PlotArea`), `LfoView` (the LFO curve with a moving dot) and four modules: LFO, Band, Saturation, Output.
- `installer-vst3.ps1` (elevated copy + hash check) and `ableton-automation.ps1` (Options.txt, a copy of MultibandSat's).
- `tests/host`: `HostSmoke` (loads the built VST3, measures pitch deviation with a 300 Hz tone, checks every shape / mode / sync / dry-wet / bypass for NaN, prints CPU) and `EditorShot` (PNG screenshots `doppler1..3.png`).

## Build, test and install

```sh
cmake -B build -G "Visual Studio 18 2026"
cmake --build build --config Release --target DopplerSat_VST3
cmake -S tests/host -B tests/host/build
cmake --build tests/host/build --config Release
tests/host/build/HostSmoke_artefacts/Release/HostSmoke.exe "build/DopplerSat_artefacts/Release/VST3/Doppler Saturator 1.0.vst3"
tests/host/build/EditorShot_artefacts/Release/EditorShot.exe "<folder>"
```

Install: `Start-Process powershell -Verb RunAs -Wait -ArgumentList '-ExecutionPolicy','Bypass','-File','<repo>\installer-vst3.ps1','-NoPause'` (UAC; run it in the background, the box can hide behind other windows).

## How it works

- **Engine use:** `satdsp::Engine` has four bands. Only band `kBand` (1) is on. Bands 0, 2 and 3 are off and parked far outside the audible range (10–12 Hz and 40–45 % of the processing rate), so their crossovers are static allpasses and add no pitch change. `engine.forceTree = true` keeps the split running at 0 % drive, so the Doppler effect is there without saturation. Edge Q is fixed: on, 50 %.
- **LFO → band:** every `kChunk` = 32 host samples, the LFO value v (−1..1, `lfoValue` = shape + offset, clipped) moves the band's middle to `log2(freq) + v · amount · 3` octaves (`bandEdgesFor`, clamped to 20 Hz–20 kHz). Then `engine.setParams` runs, and the chunk goes through the 4× oversampler and `engine.process`. Smaller chunks = smoother movement but more CPU (`setParams` designs 16 crossovers).
- **Shapes:** Sine, Triangle (in phase with sine), Square with raised-cosine transitions of about 8 ms (`squareW` = 0.008 · rate cycles, ≤ 0.25), Random = a cosine glide from one random value to the next each cycle.
- **Sync:** `syncBeats` (bars use the host's time signature). While the host plays, the phase is `ppq / beats` (locked to the bar); otherwise it free-runs at the host tempo (120 when unknown).
- **Dry/Wet:** the dry signal is delayed by the oversampler's latency (`juce::dsp::DelayLine`, Lagrange 3rd, fractional) and mixed linearly. Between dry and wet the moving allpass phase makes phaser notches.
- **Drawing:** the editor builds `EngineParams` from the band's current edges (`bandLoNow` / `bandHiNow`, written by the audio thread) and draws `CurveModel::contribPct` for band 1, the same model the sound uses.
