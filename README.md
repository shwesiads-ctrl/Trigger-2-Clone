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
- Settings and the loaded sample files are saved with your DAW project.
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
- `.tci` files (Trigger 2 instruments) are a closed format and cannot be read. Export the samples as WAV first.
- No velocity layers or round robin yet, and no MIDI output.

## License

B Trigger uses [JUCE](https://juce.com), which is available under the AGPLv3 or a commercial license.
Check the JUCE license terms before you distribute the plugin.
