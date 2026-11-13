#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

namespace BT
{
inline constexpr int kMaxLayers = 8;
inline juce::String lvlId   (int i) { return "lvl"   + juce::String (i + 1); }
inline juce::String panId   (int i) { return "pan"   + juce::String (i + 1); }
inline juce::String tuneId  (int i) { return "tune"  + juce::String (i + 1); }
inline juce::String muteId  (int i) { return "mute"  + juce::String (i + 1); }
inline juce::String soloId  (int i) { return "solo"  + juce::String (i + 1); }
inline juce::String phaseId (int i) { return "phase" + juce::String (i + 1); }

// Fader law: 0 dB sits at 70 % of the travel, +6 dB at the top, -60 dB at the bottom.
inline float faderPToDb (float p)
{
    p = juce::jlimit (0.0f, 1.0f, p);
    if (p >= 0.7f)
        return (p - 0.7f) / 0.3f * 6.0f;
    return -60.0f * std::pow (1.0f - p / 0.7f, 1.5f);
}

inline float faderDbToP (float db)
{
    db = juce::jlimit (-60.0f, 6.0f, db);
    if (db >= 0.0f)
        return 0.7f + db / 6.0f * 0.3f;
    return 0.7f * (1.0f - std::pow (-db / 60.0f, 2.0f / 3.0f));
}

inline juce::NormalisableRange<float> faderRange()
{
    return juce::NormalisableRange<float> (
        -60.0f, 6.0f,
        [] (float, float, float p) { return faderPToDb (p); },
        [] (float, float, float v) { return faderDbToP (v); },
        [] (float s, float e, float v) { return juce::jlimit (s, e, v); });
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&] (const String& id, const String& name, NormalisableRange<float> range, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def));
    };
    auto addBool = [&] (const String& id, const String& name, bool def = false)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    addFloat ("inGain",  "Input Volume", { -24.0f, 24.0f, 0.01f }, 0.0f);
    addFloat ("sens",    "Sensitivity",  { 0.0f, 100.0f, 1.0f }, 46.0f);
    addFloat ("retrig",  "Retrigger",    { 0.0f, 500.0f, 1.0f }, 54.0f);
    addFloat ("detail",  "Detail",       { 0.0f, 100.0f, 1.0f }, 39.0f);
    addFloat ("outGain", "Output Volume",{ -24.0f, 12.0f, 0.01f }, 0.0f);
    addFloat ("mix",     "Mix",          { 0.0f, 100.0f, 1.0f }, 100.0f);
    addBool  ("replace", "Replace Original", true);   // on: the original drum is removed, only samples play

    const float defLevel[kMaxLayers] = { 0.0f, -6.0f, -12.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < kMaxLayers; ++i)
    {
        const String n = "Layer " + String (i + 1) + " ";
        addFloat (lvlId (i),  n + "Level", faderRange(), defLevel[i]);
        addFloat (panId (i),  n + "Pan",   { -100.0f, 100.0f, 1.0f }, 0.0f);
        addFloat (tuneId (i), n + "Tune",  { -12.0f, 12.0f, 1.0f }, 0.0f);
        addBool  (muteId (i),  n + "Mute");
        addBool  (soloId (i),  n + "Solo");
        addBool  (phaseId (i), n + "Phase");
    }
    return layout;
}
} // namespace BT
