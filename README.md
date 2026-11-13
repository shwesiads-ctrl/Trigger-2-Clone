# Trigger-2-Clone

An **independently implemented drum replacement and enhancement project**, planned for Windows VST3 and standalone use.

> Status: **planning / foundation only**. A working drum trigger, audio engine, browser UI and release binaries are **not yet implemented**.

## Workflow

1. Select a drum source (Kick / Snare / Rack Tom / Floor Tom).
2. In **Basic** mode, load a starter preset and replacement sample.
3. In **Advanced** mode, adjust sensitivity, threshold, retrigger interval, dynamics, and wet/dry mix.
4. Replace or augment the recorded hits with legally sourced WAV samples.
5. Save a preset; eventually process audio in a DAW or standalone app.

Only one source drum track is edited at a time. Sample layering is distinct from a multi-track editor.

## Implementation plan

See [Roadmap](docs/ROADMAP.md) and [Architecture](docs/ARCHITECTURE.md).

**Intended stack:** JUCE, modern C++, CMake; HTML/CSS/JavaScript for the initial interface prototype.

## Legal and licensing

The repository is licensed under GPL-3.0 (see LICENSE). The project is not affiliated with Steven Slate Drums or its TRIGGER product. Do not use copyrighted audio libraries, proprietary code, logos, or exact product graphics without permission. JUCE and all dependencies must be reviewed for license compatibility before distributing binaries.

## Development

- `main`: stable reviewed milestones
- `develop`: prototype and implementation work

No build instructions are published yet because source and build tooling have not been added.
