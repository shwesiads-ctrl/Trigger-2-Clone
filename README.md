# B Trigger

A drum trigger plugin (VST3 and standalone) built with JUCE. It listens to a drum track, detects every hit
and plays up to four sample layers in its place, or blended with the original.

## What it does

- **Hit detection** from the audio input
  - *Sensitivity* sets the threshold (the two white lines in the waveform).
  - *Retrigger* is the shortest time between two hits.
  - *Detail* controls how easily a new hit is detected on top of the previous hit's tail (rolls, ghost notes).
  - *Input Volume* is applied before detection, so a hotter input finds quieter hits.
- **Velocity**: louder hits play their samples louder.
- **Sample layers** (up to 8, scroll sideways when there are more than four), each with level fader, pan, tune (+/- 12 semitones), Solo, Mute and polarity (Ø).
  - **ADD LAYER** adds a fader. The **×** on a layer removes it (one layer always stays).
  - **Click a layer's title** (the pencil) to give it your own name. It is saved with the project. Unnamed layers are called Layer 1, 2, 3 ...
- **REPLACE ORIGINAL** (on by default): the original drum is removed and only your samples play. Switch it off to blend the original with the samples; the **Mix** knob then sets the balance (0 % original only, 100 % samples only). Output Volume is the final gain.
- **Top bar**
  - **A / B**: two complete sets of settings to compare. B starts as a copy of A the first time you open it. **COPY TO B** (or **COPY TO A**) copies the current settings across. A and B are kept while the plugin window or project is open.
  - **RESET** puts every knob, layer and effect back to the starting sound (asks first).
  - The **Advanced page is hidden by default.** Press **Ctrl+Alt+V** (or Ctrl+Alt+click on the top bar) to show it, and again (or BACK TO BASIC) to hide it. The plugin always opens on the Basic page. Some hosts may grab Ctrl+Alt+V for themselves; the Ctrl+Alt+click works there.
- **Library delete**: right-click (or select and press Delete) any file or folder in My Library or User Samples to delete it. Deleted items go to the trash. **Right-click → Rename** (or F2) renames a file or folder in place. The Factory folder cannot be changed.
- **Attack detail**: the right side of the waveform display zooms into the last hit, from 11 ms before the trigger to 35 ms after, with a millisecond ruler, the trigger point (orange), the peak point, the threshold lines, the peak level and the rise time (trigger to peak). The left side is the two-second overview.
- **Folders**: right-click any folder in My Library, or User Presets in the Presets tab, for New folder, Rename and Delete.
- **Presets tab**: a folder tree like Trigger 2. **Factory Presets** has 25 kits with 4 presets each (Kick, Snare, Tom 1, Tom 2), 100 in all. They are variations of the built-in modelled sounds (layering, tuning, EQ, compression, room, gate, echo), not recordings. **User Presets** holds your own: type a name and press SAVE (it goes into the selected user folder, or User Presets), click to load, right-click or Delete key to remove. They are saved as `.btp` files in `Documents/B Trigger/User Presets`.
- **Advanced page**: five effects that work on the sample layers, in this order: Gate, EQ, Compressor, Delay, Reverb. Each has its own ON/OFF switch (off by default), so the starting sound is unchanged.
  - **Gate**: Threshold, Attack, Hold, Release, Range (how far the gate closes).
  - **Compressor**: Threshold, Ratio, Attack, Release, Knee, Makeup, Mix (parallel compression).
  - **Delay**: Time (1 to 1000 ms), Feedback, Low Cut, High Cut (these shape the repeats), Mix, Ping-Pong.
  - **Reverb**: Size, Damping, Width, Pre-delay, Mix.
  - **EQ**: High-pass, Low shelf (Freq, Gain), two bells (Freq, Gain, Q), High shelf (Freq, Gain).
- **Waveform and meters**: the last two seconds of input with a marker on every detected hit.
- **TRIGGER button** plays the layers without any input, handy for checking your samples.
- **Library** (left side)
  - Built-in kicks, snares and toms are created the first time the plugin loads. They are modelled sounds (pitch-dropping shell, overtones, stick transient, snare wires, room), not recordings. Import your own samples for real-kit tone.
  - **IMPORT TO MY LIBRARY** copies WAV, MP3, FLAC, OGG, AIFF and M4A files into your library.
  - **Drag and drop**
    - Drag a sample from the library onto a layer, or drag one layer's sample onto another layer to copy it.
    - Drop audio files from your computer onto a layer, or onto the library to copy them into My Library.
  - Click a file to hear it. Double-click it to load it into the first empty layer.
  - Right-click a layer's file box to empty the layer. Right-click a file in My Library to remove it.
  - The PRESETS tab loads a few starting points.
- Settings, layers, effects and the loaded sample files are saved with your DAW project.
- The window can be resized.

The library lives in `Documents/B Trigger/`.

## Get the VST3 without installing anything (GitHub Actions)

1. Push this folder to a GitHub repository (including the `.github` folder). The *Build B Trigger* workflow starts by itself on every push, or start it with **Run workflow** in the **Actions** tab.
2. When it finishes (a few minutes), open the **Releases** page of the repository. Each run publishes `B-Trigger-Windows.zip` and `B-Trigger-macOS.zip`.
3. Inside each zip you will find `B Trigger.vst3` (the plugin) and a small Standalone app for testing without a DAW.

## Build on your own computer

You need CMake 3.22 or newer and a C++17 compiler (Visual Studio 2022 on Windows, Xcode command line tools on macOS).

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

JUCE 8.0.4 is downloaded automatically. To use your own copy of JUCE add `-DBTRIGGER_JUCE_DIR=/path/to/JUCE`.
The plugin appears in `build/BTrigger_artefacts/Release/VST3/B Trigger.vst3`.

## Install

| System  | Copy `B Trigger.vst3` to |
|---------|---------------------------|
| Windows | `C:\Program Files\Common Files\VST3\` |
| macOS   | `~/Library/Audio/Plug-Ins/VST3/` |

Then rescan plugins in your DAW. On macOS a plugin downloaded from the internet may be blocked the first time;
run `xattr -cr "B Trigger.vst3"` in Terminal once, or allow it in System Settings, Privacy & Security.

## Use it in a DAW

Put B Trigger on the drum track (kick, snare or tom), as an insert effect. Start with *Mix* at 100 %. Adjust
*Sensitivity* until the markers sit on every hit you want and nothing else. The plugin reports a latency of about
1.5 ms so the samples line up with the original drums.

## Limits of this first version

- Samples longer than 30 seconds are cut at 30 seconds.
- **.tci import**: drop a `.tci` file on the library (or use IMPORT TO MY LIBRARY). It is converted automatically on a background thread with a progress bar, into a new folder in My Library with 24-bit WAVs named `FAMILY_MIC_V##_RR#.wav` (velocity layer and round robin, ordered by attack peak) plus a `MAP.txt`. Supports Trigger 2 V1/V2 and Instrument Editor files, mono and stereo. Limits: stereo samples lose their first ~2 ms of attack (not decodable yet, left silent), and a mono sample the solver cannot work out is skipped (listed in `MAP.txt`). Only convert libraries you own.
- No velocity layers or round robin yet, and no MIDI output.

## License

B Trigger uses [JUCE](https://juce.com), which is available under the AGPLv3 or a commercial license.
Check the JUCE license terms before you distribute the plugin.

## Third-party code

`Source/TciConverter.h` is a C++ port of the decoders in [tci2wav](https://github.com/AidanScroggins/tci2wav) by Aidan Scroggins (MIT License, Copyright (c) 2026 Aidan Scroggins), which reverse engineered the TCI format. The MIT license text:

> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
