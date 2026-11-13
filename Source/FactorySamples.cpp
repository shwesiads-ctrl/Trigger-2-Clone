#include "FactorySamples.h"
#include <juce_audio_basics/juce_audio_basics.h>

// Built-in drum sounds. They are modelled, not recorded: each hit is built from the parts a real drum
// makes (pitch-dropping shell tone, inharmonic overtone, stick/beater transient, snare wires, room).
namespace
{
constexpr double kSr = 44100.0;
constexpr int kFactoryVersion = 2;

// ---------------------------------------------------------------- small helpers
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    static Biquad bandpass (double f, double q)
    {
        const double w = juce::MathConstants<double>::twoPi * f / kSr, al = std::sin (w) / (2.0 * q), c = std::cos (w);
        return make (al, 0.0, -al, 1.0 + al, -2.0 * c, 1.0 - al);
    }
    static Biquad lowpass (double f, double q = 0.707)
    {
        const double w = juce::MathConstants<double>::twoPi * f / kSr, al = std::sin (w) / (2.0 * q), c = std::cos (w);
        return make ((1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + al, -2.0 * c, 1.0 - al);
    }
    static Biquad highpass (double f, double q = 0.707)
    {
        const double w = juce::MathConstants<double>::twoPi * f / kSr, al = std::sin (w) / (2.0 * q), c = std::cos (w);
        return make ((1.0 + c) / 2.0, -(1.0 + c), (1.0 + c) / 2.0, 1.0 + al, -2.0 * c, 1.0 - al);
    }
    static Biquad make (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        Biquad f;
        f.b0 = (float) (B0 / A0); f.b1 = (float) (B1 / A0); f.b2 = (float) (B2 / A0);
        f.a1 = (float) (A1 / A0); f.a2 = (float) (A2 / A0);
        return f;
    }
    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

inline float env (double t, double tau) { return (float) std::exp (-t / tau); }
inline float attack (double t) { return (float) juce::jmin (1.0, t / 0.0006); }   // 0.6 ms ramp, no click at the start

// A short, dark room: early reflections plus a decaying tail. Mixed under the dry hit.
void addRoom (juce::AudioBuffer<float>& b, float amount, float size, float damping, float brightHz)
{
    if (amount <= 0.0f)
        return;
    juce::Reverb rv;
    juce::Reverb::Parameters p;
    p.roomSize = size; p.damping = damping; p.wetLevel = 1.0f; p.dryLevel = 0.0f; p.width = 1.0f; p.freezeMode = 0.0f;
    rv.setParameters (p);
    rv.setSampleRate (kSr);

    const int n = b.getNumSamples();
    std::vector<float> l (b.getReadPointer (0), b.getReadPointer (0) + n), r = l;
    rv.processStereo (l.data(), r.data(), n);

    auto dark = Biquad::lowpass (brightHz, 0.6);
    auto dark2 = Biquad::lowpass (brightHz, 0.6);
    float* d = b.getWritePointer (0);
    for (int i = 0; i < n; ++i)
        d[i] += dark2.process (dark.process (0.5f * (l[(size_t) i] + r[(size_t) i]))) * amount;
}

void finish (juce::AudioBuffer<float>& b, float lowCutHz)
{
    const int n = b.getNumSamples();
    float* d = b.getWritePointer (0);

    auto hp = Biquad::highpass (lowCutHz);           // remove sub-sonic rumble and DC
    for (int i = 0; i < n; ++i)
        d[i] = hp.process (d[i]);

    const int fade = (int) (kSr * 0.012);
    for (int i = 0; i < fade && i < n; ++i)
    {
        const float x = (float) i / (float) fade;
        d[n - 1 - i] *= x * x;
    }

    const float mag = b.getMagnitude (0, n);
    if (mag > 0.0f)
        b.applyGain (0.7f / mag);
}

// ---------------------------------------------------------------- kick
struct KickSpec
{
    float f0, f1, sweepT, decay;     // pitch glide start, end, glide time, body decay (s)
    float drive;                     // saturation, adds harmonics
    float click, clickHz;            // beater transient level and brightness
    float sub;                       // extra sub octave
    float room;                      // room level
    float dur;
    juce::int64 seed;
};

juce::AudioBuffer<float> kick (const KickSpec& s)
{
    const int n = (int) (kSr * s.dur);
    juce::AudioBuffer<float> b (1, n);
    float* d = b.getWritePointer (0);
    juce::Random rng (s.seed);
    auto clickBp = Biquad::bandpass (s.clickHz, 1.1);
    auto clickLp = Biquad::lowpass (9000.0);

    double phase = 0.0, subPhase = 0.0;
    const float norm = std::tanh (s.drive);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / kSr;
        const double f = s.f1 + (s.f0 - s.f1) * std::exp (-t / s.sweepT);
        phase += juce::MathConstants<double>::twoPi * f / kSr;
        subPhase += juce::MathConstants<double>::twoPi * (f * 0.5) / kSr;

        float tone = (float) std::sin (phase) * env (t, s.decay) * attack (t);
        tone = std::tanh (s.drive * tone) / norm;                    // soft saturation
        const float sub = (float) std::sin (subPhase) * env (t, s.decay * 1.3) * attack (t) * s.sub;

        const float nz = rng.nextFloat() * 2.0f - 1.0f;
        float click = clickLp.process (clickBp.process (nz)) * env (t, 0.0028) * s.click * 2.2f;
        click += (float) std::sin (juce::MathConstants<double>::twoPi * 2100.0 * t) * env (t, 0.0012) * s.click * 0.5f;

        d[i] = tone + sub + click;
    }
    addRoom (b, s.room, 0.28f, 0.8f, 2200.0f);
    finish (b, 28.0f);
    return b;
}

// ---------------------------------------------------------------- snare
struct SnareSpec
{
    float f0, f1;          // shell fundamental, pitch drop
    float shellDecay;
    float wires, wireHz, wireDecay;
    float crack;           // stick transient level
    float room, dur;
    juce::int64 seed;
};

juce::AudioBuffer<float> snare (const SnareSpec& s)
{
    const int n = (int) (kSr * s.dur);
    juce::AudioBuffer<float> b (1, n);
    float* d = b.getWritePointer (0);
    juce::Random rng (s.seed);

    auto wireBp = Biquad::bandpass (s.wireHz, 0.6);
    auto wireLp = Biquad::lowpass (9500.0);
    auto wireHp = Biquad::highpass (1400.0);
    auto crackBp = Biquad::bandpass (3800.0, 0.8);

    double ph1 = 0.0, ph2 = 0.0, ph3 = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / kSr;
        const double f = s.f1 + (s.f0 - s.f1) * std::exp (-t / 0.012);
        ph1 += juce::MathConstants<double>::twoPi * f / kSr;
        ph2 += juce::MathConstants<double>::twoPi * f * 1.62 / kSr;      // inharmonic overtone
        ph3 += juce::MathConstants<double>::twoPi * f * 2.31 / kSr;

        const float shell = ((float) std::sin (ph1) * 0.9f + (float) std::sin (ph2) * 0.45f * env (t, s.shellDecay * 0.6)
                             + (float) std::sin (ph3) * 0.22f * env (t, s.shellDecay * 0.35))
                            * env (t, s.shellDecay) * attack (t);

        const float nz = rng.nextFloat() * 2.0f - 1.0f;
        // wires: bright noise with a fast first burst and a longer rattle behind it
        const float wireEnv = 0.82f * env (t, s.wireDecay * 0.22) + 0.18f * env (t, s.wireDecay * 0.8);
        const float wires = wireLp.process (wireHp.process (wireBp.process (nz))) * 1.15f * wireEnv * s.wires * attack (t);

        const float crack = crackBp.process (rng.nextFloat() * 2.0f - 1.0f) * env (t, 0.0018) * s.crack * 1.4f;

        d[i] = shell + wires + crack;
    }
    addRoom (b, s.room, 0.4f, 0.7f, 5500.0f);
    finish (b, 90.0f);
    return b;
}

// ---------------------------------------------------------------- tom
struct TomSpec
{
    float f0, f1, sweepT, decay;
    float stick, room, dur;
    juce::int64 seed;
};

juce::AudioBuffer<float> tom (const TomSpec& s)
{
    const int n = (int) (kSr * s.dur);
    juce::AudioBuffer<float> b (1, n);
    float* d = b.getWritePointer (0);
    juce::Random rng (s.seed);
    auto stickBp = Biquad::bandpass (3200.0, 0.9);

    double ph1 = 0.0, ph2 = 0.0, ph3 = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / kSr;
        const double f = s.f1 + (s.f0 - s.f1) * std::exp (-t / s.sweepT);
        ph1 += juce::MathConstants<double>::twoPi * f / kSr;
        ph2 += juce::MathConstants<double>::twoPi * f * 1.59 / kSr;
        ph3 += juce::MathConstants<double>::twoPi * f * 2.14 / kSr;

        const float body = ((float) std::sin (ph1) * env (t, s.decay)
                            + (float) std::sin (ph2) * 0.32f * env (t, s.decay * 0.45)
                            + (float) std::sin (ph3) * 0.15f * env (t, s.decay * 0.25)) * attack (t);
        const float stick = stickBp.process (rng.nextFloat() * 2.0f - 1.0f) * env (t, 0.0025) * s.stick * 2.4f;

        d[i] = std::tanh (1.3f * (body + stick)) * 0.8f;
    }
    addRoom (b, s.room, 0.35f, 0.75f, 3000.0f);
    finish (b, 40.0f);
    return b;
}

void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b)
{
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    std::unique_ptr<juce::FileOutputStream> out (f.createOutputStream());
    if (out == nullptr)
        return;

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (out.get(), kSr, (unsigned int) b.getNumChannels(), 24, {}, 0));
    if (writer != nullptr)
    {
        out.release(); // the writer owns the stream now
        writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}
} // namespace

namespace FactorySamples
{
juce::File factoryDir (const juce::File& root) { return root.getChildFile ("B Trigger Factory"); }

void install (const juce::File& root)
{
    root.getChildFile ("User Samples").createDirectory();
    root.getChildFile ("My Library").createDirectory();

    const auto dir = factoryDir (root);
    const auto marker = dir.getChildFile ("version.txt");
    if (marker.existsAsFile() && marker.loadFileAsString().trim().getIntValue() >= kFactoryVersion)
        return;

    const auto kicks = dir.getChildFile ("Kicks");
    const auto snares = dir.getChildFile ("Snares");
    const auto toms = dir.getChildFile ("Toms");

    //                                                  f0   f1  sweep decay drive click clkHz sub  room dur  seed
    writeWav (kicks.getChildFile ("Kick_Close.wav"),   kick ({ 150, 52, 0.020f, 0.070f, 1.6f, 0.55f, 4200, 0.00f, 0.00f, 0.40f, 1 }));
    writeWav (kicks.getChildFile ("Kick_Body.wav"),    kick ({  95, 46, 0.040f, 0.160f, 1.2f, 0.15f, 2500, 0.35f, 0.00f, 0.75f, 2 }));
    writeWav (kicks.getChildFile ("Kick_Room.wav"),    kick ({ 100, 50, 0.035f, 0.110f, 1.1f, 0.25f, 3000, 0.00f, 0.45f, 1.30f, 3 }));
    writeWav (kicks.getChildFile ("Kick_Modern_01.wav"), kick ({ 170, 48, 0.024f, 0.120f, 1.8f, 0.45f, 3800, 0.30f, 0.08f, 0.70f, 4 }));
    writeWav (kicks.getChildFile ("Kick_Punch_01.wav"),  kick ({ 210, 56, 0.016f, 0.080f, 2.2f, 0.75f, 4800, 0.00f, 0.00f, 0.45f, 5 }));
    writeWav (kicks.getChildFile ("Kick_Rock_01.wav"),   kick ({ 135, 50, 0.030f, 0.170f, 1.5f, 0.35f, 3200, 0.25f, 0.14f, 0.85f, 6 }));
    writeWav (kicks.getChildFile ("Kick_Metal_01.wav"),  kick ({ 260, 62, 0.012f, 0.090f, 2.6f, 1.00f, 6000, 0.00f, 0.00f, 0.50f, 7 }));

    //                                                      f0   f1  shell wires wireHz wireDec crack room dur  seed
    writeWav (snares.getChildFile ("Snare_Crack_01.wav"), snare ({ 245, 190, 0.080f, 1.00f, 4800, 0.16f, 0.9f, 0.15f, 0.70f, 8 }));
    writeWav (snares.getChildFile ("Snare_Tight_01.wav"), snare ({ 285, 215, 0.055f, 0.75f, 5400, 0.09f, 0.7f, 0.00f, 0.42f, 9 }));
    writeWav (snares.getChildFile ("Snare_Fat_01.wav"),   snare ({ 190, 150, 0.120f, 1.10f, 3600, 0.24f, 0.6f, 0.30f, 0.95f, 10 }));

    //                                                  f0   f1  sweep decay stick room dur seed
    writeWav (toms.getChildFile ("Tom_High_01.wav"),  tom ({ 290, 205, 0.045f, 0.150f, 0.7f, 0.22f, 0.85f, 11 }));
    writeWav (toms.getChildFile ("Tom_Mid_01.wav"),   tom ({ 215, 150, 0.055f, 0.200f, 0.7f, 0.24f, 1.00f, 12 }));
    writeWav (toms.getChildFile ("Tom_Floor_01.wav"), tom ({ 150, 98, 0.065f, 0.280f, 0.7f, 0.26f, 1.30f, 13 }));

    marker.replaceWithText (juce::String (kFactoryVersion));
}
} // namespace FactorySamples
