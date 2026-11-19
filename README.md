# Fuzzy Filter

A stereo audio effect plugin: a Fuzz Face style fuzz and a QPAS-inspired
four-peak resonant filter, with a MicroFreak-style mod grid.
Built with [JUCE](https://juce.com) 8 as VST3 (Windows and Mac) and AU (Mac).
AAX for Pro Tools comes in step 4.

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
| Output | Levels for Low Pass, Band Pass, High Pass, Notch Pass and Comb; Filter Mix; Output Mode (Mixed or Separate) |

**Filter cores.** Four 12 dB/octave state variable filters: two on the left,
two on the right. Each core also has a comb filter tuned to the same frequency,
so Space spreads the combs exactly as it spreads the peaks. Notch Pass is the
QPAS's Smile Pass sound: high pass at the bottom of the range, a notch in the
middle, low pass at the top.

**Separate outputs.** The plugin has five extra stereo outputs, one per filter
type. They are off by default; turn them on in the DAW and set Output Mode to
Separate. They carry each filter type at full level (the mixer levels only
affect the main output), and are taken straight from the filter, so the fuzz
reaches them only when it is set to pre filter. Whether a host shows extra
outputs on an audio effect varies, so check this in Ableton and Pro Tools.

**Sidechain.** A stereo sidechain input can drive Excite 1 and 2, and either
envelope follower can listen to it.

## Modulation

| Section | What it does |
|---|---|
| 4 LFOs | Sine, triangle, saw up, saw down, square, sample and hold, smooth random. 0.01 Hz to 2 kHz, or synced to the host tempo (1/64 to 8 bars). Phase, stereo offset (0 to 180 degrees) and retrigger from Env 1 or Env 2. |
| 2 envelope followers | Listen to the input before or after the input gain, or to the sidechain. Threshold and sensitivity set when they fire. |
| 2 ADSR envelopes | Each one is gated by its own follower. Attack 0.1 ms to 10 s, decay to 10 s, release to 20 s. |
| Mod grid | 8 sources (4 LFOs, 2 envelopes, 2 followers) by 14 destinations, each crossing -100% to +100%. |

Fixed destinations: Input Gain, Frequency, Rez, Space L, Space R, Excite 1,
Excite 2, Fuzz, Fuzz Bias, Fuzz Volume and Fuzz Mix. The three Assign columns
can each point at an LFO rate, any envelope time, sustain or threshold, Comb
Damping, Filter Mix or one of the five output levels.

Modulation is computed per sample and per channel, so a stereo LFO moves the
left and right filters apart. Like the QPAS, Space R follows Space L's
modulation until something is routed to Space R itself. Setting an Excite
source to "Mod grid only" makes the grid signal the excite signal, with the
Excite knob as its amount.

## Layout

```
Source/
  PluginProcessor.*   buses, parameters, signal routing
  Parameters.*        every parameter, its range and default
  ModDefs.h           mod sources, destinations and assignable targets
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

JUCE is free for personal use and for small businesses under its starter
license, or under the AGPLv3. Check the [JUCE licence terms](https://juce.com/get-juce/)
before selling the plugin.
