#pragma once
#include <juce_audio_formats/juce_audio_formats.h>

// Generates the built-in kick, snare and tom samples the first time the plugin runs,
// so the library is never empty. Files are written under <Documents>/B Trigger/.
namespace FactorySamples
{
    juce::File factoryDir (const juce::File& libraryRoot);
    void install (const juce::File& libraryRoot);
}
