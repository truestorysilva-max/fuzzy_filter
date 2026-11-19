# Fuzzy Filter

A stereo audio effect plugin: a Fuzz Face style fuzz and a QPAS-inspired
four-peak resonant filter, with a MicroFreak-style mod grid (coming in step 3).
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
| Filter | Frequency, Q (-100% to +100%; below 0 rounds the peaks off), Space L, Space R, Excite 1 and 2 with a source each (input audio, sidechain, or mod grid only), Comb Damping |
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

**Sidechain.** A stereo sidechain input can drive Excite 1 and 2. The envelope
followers that listen to it arrive with the modulation in step 3.

## Layout

```
Source/
  PluginProcessor.*   buses, parameters, signal routing
  Parameters.*        every parameter, its range and default
  dsp/                the sound engine (plain C++, no JUCE)
    InputStage.h
    FuzzFace.h
    QuadPeakFilter.h  four cores, LP/BP/HP/Notch Pass/Comb
tests/
  dsp_tests.cpp           offline checks of the DSP
  plugin_smoke_test.cpp   runs the whole plugin without a host
```

## License note

JUCE is free for personal use and for small businesses under its starter
license, or under the AGPLv3. Check the [JUCE licence terms](https://juce.com/get-juce/)
before selling the plugin.
