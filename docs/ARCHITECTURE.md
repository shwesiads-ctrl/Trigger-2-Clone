# Architecture — proposed

## Components

- `PluginProcessor`: JUCE AudioProcessor parameter and state owner; host audio/MIDI I/O.
- `TriggerDetector`: envelope/transient analysis, threshold, hysteresis, filtering and retrigger lockout.
- `SampleLibrary`: background file decoding, validation and immutable samples.
- `SampleVoicePool`: preallocated polyphonic playback with deterministic voice-stealing.
- `DrumProfile`: per-drum settings, sample mapping and presets.
- `PluginEditor`: controls and live waveform/trigger metrics read through a bounded, lock-free bridge.
- `StandaloneTransport`: file load/play/pause/stop/seek/export; excluded from VST3-only processing paths.

## Audio-thread contract

Avoid memory allocations, blocking locks, file I/O, network I/O and UI operations in `processBlock`. Load files off-thread, publish fully prepared buffers atomically, and test rapid preset switching. Latency must be measurable and reported to the host when introduced by analysis or lookahead.

## Project format

Persist versioned sample references, a missing-sample fallback state, a per-drum mode, detection parameters, mix parameters and a user preset ID. Do not bundle third-party samples without permission.

## Milestones

Start with a browser proof-of-work, then migrate behavior to JUCE while keeping the underlying parameter schema consistent. Browser-only DSP behavior must not be represented as production-ready VST3 behavior.
