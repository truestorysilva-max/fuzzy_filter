# Fuzzy Filter

A stereo audio effect plugin: a Fuzz Face style fuzz and a QPAS-inspired
four-peak resonant filter, with a MicroFreak-style mod grid.
Built with [JUCE](https://juce.com) 8 as VST3 (Windows and Mac), AU (Mac) and
AAX (Windows and Mac, for Pro Tools).

**AAX is unsigned for now.** Retail Pro Tools only loads AAX plugins signed
with PACE's wraptool. Until that's set up, test the AAX in Pro Tools
Developer, which Avid gives to members of its developer program. Signing
needs a PACE developer account and an iLok, and runs as one extra CI step
once they exist.

## Getting a build without compiling

Every push builds the plugin on GitHub for Windows and Mac. Open the
**Actions** tab, pick the latest green run, and download an artifact from the
bottom of the page:

- `FuzzyFilter-Windows-VST3`: unzip and copy `Fuzzy Filter.vst3` into
  `C:\Program Files\Common Files\VST3`, then rescan plugins in Ableton.
- `FuzzyFilter-macOS-VST3` / `FuzzyFilter-macOS-AU`: copy into
  `~/Library/Audio/Plug-Ins/VST3` or `~/Library/Audio/Plug-Ins/Components`.
  These builds are not signed yet, so macOS may block them until signing is
  set up in step 4.

## Building on Windows

1. Install [Visual Studio 2022](https://visualstudio.microsoft.com/) with the
   "Desktop development with C++" workload, plus [CMake](https://cmake.org)
   and [Git](https://git-scm.com).
2. From a terminal in this folder:

   ```
   cmake -B build
   cmake --build build --config Release
   ctest --test-dir build -C Release
   ```

3. The plugin is at `build/FuzzyFilter_artefacts/Release/VST3/Fuzzy Filter.vst3`.

CMake downloads JUCE the first time (about 150 MB).

## What is in the sound engine

| Stage | Controls |
| --- | --- |
| Input | Input Gain, -24 to +24 dB, saturation fades in above 0 dB |
| Fuzz | Fuzz, Volume, Bias, Character (Germanium or Silicon), Mix, Fuzz On, Fuzz Position (pre or post filter). Runs at 4x oversampling |
| Filter | Frequency, Rez (resonance, -100% to +100%; below 0 rounds the peaks off), Space L, Space R, Excite 1 and 2 with a source each (input audio, sidechain, or mod grid only), Comb Damping |
| Output | Levels for Low Pass, Band Pass, High Pass, Notch Pass and Comb; Filter Mix |

**Filter cores.** Four 12 dB/octave state variable filters: two on the left,
two on the right. Each core also has a comb filter tuned to the same frequency,
so Space spreads the combs exactly as it spreads the peaks. Notch Pass is the
QPAS's Smile Pass sound: high pass at the bottom of the range, a notch in the
middle, low pass at the top.

**Separate outputs.** The plugin has five extra stereo outputs, one per filter
type. They are always active: turn them on in the DAW and route them like any
other output, while the main output keeps the mix. They carry each filter type at full level (the mixer levels only
affect the main output), and are taken straight from the filter, so the fuzz
reaches them only when it is set to pre filter. Whether a host shows extra
outputs on an audio effect varies, so check this in Ableton and Pro Tools.

**Sidechain.** A stereo sidechain input can drive Excite 1 and 2, and either
envelope follower can listen to it.

## Modulation

| Section | What it does |
|---|---|
| 4 LFOs | Sine, triangle, saw up, saw down, square, sample and hold, smooth random (a curve through random points that never stops at them). 0.01 Hz to 2 kHz, or synced to the host tempo (1/64 to 8 bars). Phase, stereo offset (0 to 180 degrees) and retrigger from Env 1 or Env 2. |
| 2 envelope followers | Listen to the input before or after the input gain, or to the sidechain. Attack, hold and release set how they rise and fall; threshold and sensitivity set when they fire. In the UI they sit behind the envelopes: switch the Env panel from ADSR to FOLLOWER. |
| 2 ADSR envelopes | Each one is gated by its own follower. Attack 0.1 ms to 10 s, decay to 10 s, release to 20 s. |
| Mod grid | 8 sources (4 LFOs, 2 envelopes, 2 followers) by 14 destinations, each crossing -100% to +100%. |

Fixed destinations: Input Gain, Frequency, Rez, Space L, Space R, Excite 1,
Excite 2, Fuzz, Fuzz Bias, Fuzz Volume and Fuzz Mix. The three Assign columns
can each point at an LFO rate or shape, any envelope time, sustain or threshold,
Comb Damping, Filter Mix or one of the five output levels. Modulating an LFO
shape steps through the shapes in menu order (sine up to smooth random).

Modulation is computed per sample and per channel, so a stereo LFO moves the
left and right filters apart. Like the QPAS, Space R follows Space L's
modulation until something is routed to Space R itself. Setting an Excite
source to "Mod grid only" makes the grid signal the excite signal, with the
Excite knob as its amount.

## UI and presets

The window follows the Cut Paper design. Drag a knob up or down to turn it, and double-click it to reset it. A red arc around a knob shows how far the mod grid moves it, and a dot shows where it is right now. In the mod grid, click a crossing and drag to set its amount, and double-click to clear it. Click A1, A2 or A3 at the top of a column to choose what that Assign column controls. Drag the bottom-right corner to resize the window.

The arrows step through presets. Click the preset name to see the full list, save your own, or delete one. User presets are `.ffpreset` files in:

- Windows: `%APPDATA%\Fuzzy Filter\Presets`
- Mac: `~/Library/Fuzzy Filter/Presets`

They are plain XML files, so you can copy them between computers.

## Layout

```
Source/
  PluginProcessor.*   buses, parameters, signal routing
  Parameters.*        every parameter, its range and default
  ModDefs.h           mod sources, destinations and assignable targets
  PresetManager.*     factory and user presets
  ui/                 the editor: style, widgets, displays, mod grid
  dsp/                the sound engine (plain C++, no JUCE)
    InputStage.h
    FuzzFace.h
    QuadPeakFilter.h  four cores, LP/BP/HP/Notch Pass/Comb
    Modulation.h      LFOs, envelope followers, ADSRs
tests/
  dsp_tests.cpp           offline checks of the DSP
  plugin_smoke_test.cpp   runs the whole plugin without a host
```

## License note

The UI fonts are Londrina Solid and Archivo Narrow. Both are under the SIL Open Font License, and their license texts are in `Resources/fonts`.

JUCE is free for personal use and for small businesses under its starter
license, or under the AGPLv3. Check the [JUCE licence terms](https://juce.com/get-juce/)
before selling the plugin.
