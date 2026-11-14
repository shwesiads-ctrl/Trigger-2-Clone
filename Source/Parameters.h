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

// ---- effects on the sample bus: Gate > EQ > Compressor > Delay > Reverb
namespace Fx
{
enum Id { GateOn, GThr, GAtk, GHold, GRel, GRange,
          CompOn, CThr, CRatio, CAtk, CRel, CKnee, CMakeup, CMix,
          DelayOn, DTime, DFb, DLow, DHigh, DMix, DPing,
          ReverbOn, RSize, RDamp, RWidth, RPre, RMix,
          EqOn, EHp, ELoF, ELoG, ELmF, ELmG, ELmQ, EHmF, EHmG, EHmQ, EHiF, EHiG,
          Count };

struct Def { const char* id; const char* name; float lo, hi, step, def, skew; bool isBool; };

inline const Def& def (int i)
{
    static const Def d[Count] = {
        { "gateOn",   "Gate On",              0, 1, 1, 0, 1, true },
        { "gateThr",  "Gate Threshold",     -80, 0, 0.1f, -40, 1, false },
        { "gateAtk",  "Gate Attack",        0.1f, 50, 0.1f, 1, 0.4f, false },
        { "gateHold", "Gate Hold",            0, 500, 1, 30, 0.5f, false },
        { "gateRel",  "Gate Release",         5, 1000, 1, 120, 0.4f, false },
        { "gateRange","Gate Range",         -80, 0, 0.1f, -60, 1, false },

        { "compOn",   "Compressor On",        0, 1, 1, 0, 1, true },
        { "compThr",  "Comp Threshold",     -60, 0, 0.1f, -18, 1, false },
        { "compRatio","Comp Ratio",           1, 20, 0.1f, 4, 0.5f, false },
        { "compAtk",  "Comp Attack",       0.1f, 100, 0.1f, 10, 0.4f, false },
        { "compRel",  "Comp Release",        10, 1000, 1, 120, 0.4f, false },
        { "compKnee", "Comp Knee",            0, 24, 0.1f, 6, 1, false },
        { "compMakeup","Comp Makeup",         0, 24, 0.1f, 0, 1, false },
        { "compMix",  "Comp Mix",             0, 100, 1, 100, 1, false },

        { "delayOn",  "Delay On",             0, 1, 1, 0, 1, true },
        { "delayTime","Delay Time",           1, 1000, 0.1f, 250, 0.5f, false },
        { "delayFb",  "Delay Feedback",       0, 95, 1, 35, 1, false },
        { "delayLow", "Delay Low Cut",       20, 2000, 1, 100, 0.4f, false },
        { "delayHigh","Delay High Cut",     500, 20000, 1, 8000, 0.4f, false },
        { "delayMix", "Delay Mix",            0, 100, 1, 25, 1, false },
        { "delayPing","Delay Ping-Pong",      0, 1, 1, 0, 1, true },

        { "revOn",    "Reverb On",            0, 1, 1, 0, 1, true },
        { "revSize",  "Reverb Size",          0, 100, 1, 50, 1, false },
        { "revDamp",  "Reverb Damping",       0, 100, 1, 50, 1, false },
        { "revWidth", "Reverb Width",         0, 100, 1, 100, 1, false },
        { "revPre",   "Reverb Pre-delay",     0, 100, 0.1f, 0, 0.6f, false },
        { "revMix",   "Reverb Mix",           0, 100, 1, 25, 1, false },

        { "eqOn",     "EQ On",                0, 1, 1, 0, 1, true },
        { "eqHp",     "EQ High-pass",        20, 500, 1, 20, 0.4f, false },
        { "eqLoF",    "EQ Low Freq",         30, 400, 1, 100, 0.5f, false },
        { "eqLoG",    "EQ Low Gain",        -18, 18, 0.1f, 0, 1, false },
        { "eqLmF",    "EQ Low-Mid Freq",    100, 3000, 1, 400, 0.5f, false },
        { "eqLmG",    "EQ Low-Mid Gain",    -18, 18, 0.1f, 0, 1, false },
        { "eqLmQ",    "EQ Low-Mid Q",      0.3f, 8, 0.01f, 1, 0.5f, false },
        { "eqHmF",    "EQ High-Mid Freq",   500, 8000, 1, 2500, 0.5f, false },
        { "eqHmG",    "EQ High-Mid Gain",   -18, 18, 0.1f, 0, 1, false },
        { "eqHmQ",    "EQ High-Mid Q",     0.3f, 8, 0.01f, 1, 0.5f, false },
        { "eqHiF",    "EQ High Freq",      2000, 18000, 1, 8000, 0.5f, false },
        { "eqHiG",    "EQ High Gain",       -18, 18, 0.1f, 0, 1, false },
    };
    return d[i];
}
} // namespace Fx

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
    // detection band and crosstalk suppression
    addFloat ("lowcut",  "Low Cut",      juce::NormalisableRange<float> (0.0f, 1000.0f, 1.0f, 0.4f), 0.0f);
    addFloat ("highcut", "High Cut",     juce::NormalisableRange<float> (500.0f, 20000.0f, 1.0f, 0.3f), 20000.0f);
    addFloat ("suppress","Suppress",     { 0.0f, 100.0f, 1.0f }, 0.0f);
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));
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

    for (int i = 0; i < Fx::Count; ++i)
    {
        const auto& d = Fx::def (i);
        if (d.isBool)
            addBool (d.id, d.name, d.def > 0.5f);
        else
            addFloat (d.id, d.name, NormalisableRange<float> (d.lo, d.hi, d.step, d.skew), d.def);
    }
    return layout;
}
} // namespace BT
