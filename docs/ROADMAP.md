# Trigger-2-Clone — Roadmap

Independent drum replacement and augmentation software, inspired by the workflow of professional drum trigger tools. Do not copy proprietary DSP, UI graphics, sample libraries or trademarks.

## Product targets
- Windows VST3 and standalone, using JUCE/C++/CMake.
- One drum source track per plugin instance; choose Kick, Snare, Rack Tom or Floor Tom.
- Two modes: Basic (presets and quick replacement) and Advanced (manual controls).
- Import legally owned WAV samples; optional MP3 decoding in a later milestone.
- Independent sample selection/processing for the selected drum.
- Real-time safe audio: no disk operations, allocations or locks on the audio thread.

## Phase 1 — Browser UI and interaction prototype
- [ ] Responsive original dark UI with gold/blue accents and a clearly selected drum.
- [ ] Basic/Advanced mode switch and per-drum controls.
- [ ] Load and preview an audio file in a browser demo.
- [ ] Show source waveform and detected transient markers.
- [ ] Load a replacement sample and hear source/sample/wet-dry blend.
- [ ] Preset save/restore using a versioned JSON schema.
- [ ] Keyboard accessibility and reasonable mobile/desktop layout.
- [ ] Distinguish UI demo from actual DAW plugin.

## Phase 2 — JUCE real-time engine
- [ ] CMake JUCE project with standalone and VST3 targets.
- [ ] Envelope/transient detection, hysteresis, retrigger interval, and velocity mapping.
- [ ] Sample-accurate or latency-compensated scheduling with documented timing.
- [ ] WAV sample loader, sample preloading, and safe state replacement.
- [ ] Original/sampled wet-dry mixing, choke handling, multi-voice playback.
- [ ] DAW save/load, parameter automation, and deterministic offline rendering.
- [ ] Unit tests for detection, false triggers, sample timing, gain and persistence.

## Phase 3 — Pro feature set
- [ ] Multiple velocity/round-robin sample layers and drum-specific presets.
- [ ] User-configurable bleed filtering/suppression and manual correction.
- [ ] Waveform zoom, trigger marker editing in standalone.
- [ ] MIDI input and event export, where supported by the host.
- [ ] Independent processing for each selected drum, without a mandatory multi-track editor.

## Phase 4 — Release and QA
- [ ] GitHub Actions builds and reproducible Windows packaging.
- [ ] DAW integration testing in REAPER and Cubase; FL Studio smoke test.
- [ ] CPU, memory, sample-rate (44.1/48/96 kHz), buffer-size and latency QA.
- [ ] Documentation and third-party license/asset attribution.
- [ ] Sign and publish only verified release artifacts.

## Acceptance gate
No milestone is marked complete without implementation plus recorded QA evidence. No claim of Trigger 2 feature parity before matched real-audio tests.
