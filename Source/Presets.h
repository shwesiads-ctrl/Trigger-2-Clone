#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <utility>
#include "Parameters.h"

// The 100 factory presets: 25 kits, and each kit has a Kick, a Snare and two Toms.
// They are built from the built-in modelled sounds, varied by layering, tuning, EQ, compression, room and gate.
// Anything you make yourself is saved as a user preset (see BTriggerProcessor::savePreset).
namespace BTPresets
{
struct LayerSpec
{
    juce::String file;     // relative to the factory sample folder, e.g. "Kicks/Kick_Close.wav"
    float level, tune;     // dB, semitones
    float pan = 0.0f;
};

struct FactoryPreset
{
    juce::String kit, name;
    std::vector<LayerSpec> layers;
    std::vector<std::pair<juce::String, float>> params;   // parameter id, value
};

namespace detail
{
struct Kit
{
    const char* name;
    int kick, kick2, snare, snare2, tomA, tomB;      // indexes into the sample lists below
    float tune, room, punch, lowG, highG;
    bool echo;                                       // adds a short delay to the snare and toms
};

inline const char* kicks[]  = { "Kicks/Kick_Close.wav", "Kicks/Kick_Body.wav", "Kicks/Kick_Room.wav", "Kicks/Kick_Modern_01.wav",
                                "Kicks/Kick_Punch_01.wav", "Kicks/Kick_Rock_01.wav", "Kicks/Kick_Metal_01.wav" };
inline const char* snares[] = { "Snares/Snare_Crack_01.wav", "Snares/Snare_Tight_01.wav", "Snares/Snare_Fat_01.wav" };
inline const char* toms[]   = { "Toms/Tom_High_01.wav", "Toms/Tom_Mid_01.wav", "Toms/Tom_Floor_01.wav" };

inline const Kit kits[] = {
    //  name               k  k2 s  s2 tA tB  tune  room  punch lowG highG echo
    { "Arena Rock",        5, 1, 0, 2, 0, 2,  0.0f, 0.9f, 0.6f,  3.0f,  2.0f, false },
    { "Studio Pop",        3, 1, 1, 0, 0, 1,  1.0f, 0.4f, 0.5f,  2.0f,  3.0f, false },
    { "Tight Funk",        4, 1, 0, 1, 1, 2, -1.0f, 0.1f, 0.8f,  1.0f,  3.0f, false },
    { "Vintage 60s",       2, 1, 2, 1, 1, 2, -2.0f, 0.5f, 0.3f,  4.0f, -1.0f, false },
    { "Heavy Metal",       6, 1, 0, 1, 0, 2, -3.0f, 0.3f, 0.9f,  5.0f,  1.0f, false },
    { "Punk Energy",       4, 2, 1, 0, 0, 1,  2.0f, 0.2f, 0.9f,  0.0f,  4.0f, false },
    { "Jazz Club",         1, 2, 2, 1, 1, 2, -2.0f, 0.6f, 0.2f,  3.0f, -2.0f, false },
    { "Indie Room",        0, 1, 2, 1, 1, 2,  0.0f, 0.7f, 0.4f,  1.0f,  1.0f, false },
    { "Hard Rock",         5, 1, 0, 2, 0, 2, -1.0f, 0.8f, 0.7f,  4.0f,  2.0f, false },
    { "Modern Pop",        3, 1, 1, 0, 0, 1,  1.0f, 0.3f, 0.8f,  2.0f,  3.0f, false },
    { "Electro Pulse",     4, 1, 1, 2, 1, 2,  0.0f, 0.0f, 0.7f,  6.0f,  2.0f, true  },
    { "Hip Hop Dusty",     1, 2, 2, 1, 0, 1, -3.0f, 0.2f, 0.5f,  5.0f, -3.0f, false },
    { "Gospel Chops",      2, 1, 0, 2, 1, 2,  1.0f, 0.4f, 0.4f,  2.0f,  1.0f, false },
    { "Latin Heat",        0, 1, 1, 2, 0, 2,  2.0f, 0.2f, 0.6f,  1.0f,  3.0f, false },
    { "RnB Smooth",        1, 2, 2, 0, 1, 1,  0.0f, 0.4f, 0.5f,  3.0f,  2.0f, false },
    { "Blues Shuffle",     2, 1, 2, 1, 1, 2, -1.0f, 0.5f, 0.3f,  2.0f, -1.0f, false },
    { "Stadium Anthem",    5, 2, 0, 2, 0, 2,  0.0f, 1.0f, 0.7f,  3.0f,  2.0f, false },
    { "Dry Session",       4, 1, 1, 0, 1, 2,  1.0f, 0.0f, 0.6f,  0.0f,  3.0f, false },
    { "Big Ballad",        2, 2, 2, 1, 0, 2, -2.0f, 0.9f, 0.4f,  4.0f,  1.0f, false },
    { "Thrash Attack",     6, 1, 0, 2, 0, 2, -4.0f, 0.3f, 1.0f,  3.0f,  4.0f, false },
    { "Lo-Fi Basement",    1, 2, 2, 1, 1, 2, -5.0f, 0.6f, 0.3f,  4.0f, -4.0f, false },
    { "Cinematic Boom",    2, 2, 2, 1, 2, 2, -7.0f, 1.0f, 0.5f,  6.0f,  0.0f, false },
    { "Garage Band",       0, 1, 1, 0, 1, 1,  0.0f, 0.5f, 0.5f,  1.0f,  3.0f, false },
    { "Country Road",      2, 1, 2, 1, 1, 2, -1.0f, 0.4f, 0.4f,  2.0f,  1.0f, false },
    { "Dance Floor",       4, 3, 0, 1, 0, 1,  2.0f, 0.2f, 0.9f,  5.0f,  3.0f, true  },
};
} // namespace detail

inline const std::vector<FactoryPreset>& factory()
{
    static const std::vector<FactoryPreset> all = []
    {
        using juce::String;
        std::vector<FactoryPreset> out;
        auto P = [] (const char* id, float v) { return std::make_pair (String (id), v); };

        for (const auto& k : detail::kits)
        {
            const float room = k.room, punch = k.punch;

            // shared processing: compressor, EQ, room, tail gate, echo
            auto fx = [&] (FactoryPreset& p, float roomScale, bool allowGate)
            {
                if (punch > 0.45f)
                {
                    p.params.push_back (P ("compOn", 1.0f));
                    p.params.push_back (P ("compThr", -26.0f + (1.0f - punch) * 12.0f));
                    p.params.push_back (P ("compRatio", 2.0f + punch * 4.0f));
                    p.params.push_back (P ("compAtk", 6.0f + (1.0f - punch) * 14.0f));
                    p.params.push_back (P ("compRel", 110.0f));
                    p.params.push_back (P ("compMakeup", punch * 4.0f));
                }
                if (k.lowG != 0.0f || k.highG != 0.0f)
                {
                    p.params.push_back (P ("eqOn", 1.0f));
                    p.params.push_back (P ("eqLoF", 90.0f));
                    p.params.push_back (P ("eqLoG", k.lowG));
                    p.params.push_back (P ("eqHiF", 7000.0f));
                    p.params.push_back (P ("eqHiG", k.highG));
                }
                if (room > 0.25f)
                {
                    p.params.push_back (P ("revOn", 1.0f));
                    p.params.push_back (P ("revSize", 20.0f + room * 50.0f));
                    p.params.push_back (P ("revDamp", 55.0f));
                    p.params.push_back (P ("revMix", room * 28.0f * roomScale));
                }
                if (allowGate && punch > 0.85f)
                {
                    p.params.push_back (P ("gateOn", 1.0f));
                    p.params.push_back (P ("gateThr", -48.0f));
                    p.params.push_back (P ("gateHold", 70.0f));
                    p.params.push_back (P ("gateRel", 160.0f));
                }
            };
            auto echo = [&] (FactoryPreset& p)
            {
                if (! k.echo)
                    return;
                p.params.push_back (P ("delayOn", 1.0f));
                p.params.push_back (P ("delayTime", 125.0f));
                p.params.push_back (P ("delayFb", 25.0f));
                p.params.push_back (P ("delayMix", 14.0f));
            };

            // ---- Kick
            {
                FactoryPreset p;
                p.kit = k.name;
                p.name = String (k.name) + " Kick";
                p.layers.push_back ({ detail::kicks[k.kick], 0.0f, k.tune });
                p.layers.push_back ({ detail::kicks[k.kick2], -6.0f, k.tune - 1.0f });
                if (room > 0.35f)
                    p.layers.push_back ({ "Kicks/Kick_Room.wav", -13.0f + room * 5.0f, k.tune });
                p.params.push_back (P ("sens", 46.0f));
                p.params.push_back (P ("retrig", 45.0f + room * 25.0f));
                p.params.push_back (P ("detail", 39.0f));
                fx (p, 0.6f, true);
                out.push_back (std::move (p));
            }
            // ---- Snare
            {
                FactoryPreset p;
                p.kit = k.name;
                p.name = String (k.name) + " Snare";
                p.layers.push_back ({ detail::snares[k.snare], 0.0f, k.tune * 0.5f });
                p.layers.push_back ({ detail::snares[k.snare2], -5.0f, k.tune * 0.5f + 1.0f, 0.1f });
                if (punch > 0.6f)
                    p.layers.push_back ({ "Snares/Snare_Crack_01.wav", -10.0f, k.tune * 0.5f + 2.0f, -0.1f });
                p.params.push_back (P ("sens", 42.0f));
                p.params.push_back (P ("retrig", 40.0f));
                p.params.push_back (P ("detail", 45.0f));
                fx (p, 1.5f, false);
                echo (p);
                out.push_back (std::move (p));
            }
            // ---- Tom 1
            {
                FactoryPreset p;
                p.kit = k.name;
                p.name = String (k.name) + " Tom 1";
                p.layers.push_back ({ detail::toms[k.tomA], 0.0f, k.tune });
                p.layers.push_back ({ detail::toms[k.tomA], -8.0f, k.tune + 5.0f });
                p.params.push_back (P ("sens", 50.0f));
                p.params.push_back (P ("retrig", 70.0f));
                p.params.push_back (P ("detail", 35.0f));
                fx (p, 1.2f, false);
                echo (p);
                out.push_back (std::move (p));
            }
            // ---- Tom 2
            {
                FactoryPreset p;
                p.kit = k.name;
                p.name = String (k.name) + " Tom 2";
                p.layers.push_back ({ detail::toms[k.tomB], 0.0f, k.tune });
                p.layers.push_back ({ detail::toms[2], -7.0f, k.tune - 2.0f });
                p.params.push_back (P ("sens", 50.0f));
                p.params.push_back (P ("retrig", 80.0f));
                p.params.push_back (P ("detail", 35.0f));
                fx (p, 1.2f, false);
                echo (p);
                out.push_back (std::move (p));
            }
        }
        return out;
    }();
    return all;
}
} // namespace BTPresets
