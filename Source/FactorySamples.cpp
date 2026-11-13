#include "FactorySamples.h"

namespace
{
constexpr double kSr = 44100.0;

struct DrumSpec
{
    float f0, f1, sweepT, decay, dur, click, noise, noiseDecay, noiseLP;
    juce::int64 seed;
};

juce::AudioBuffer<float> synth (const DrumSpec& s)
{
    const int n = (int) (kSr * s.dur);
    juce::AudioBuffer<float> b (1, n);
    float* d = b.getWritePointer (0);
    juce::Random rng (s.seed);

    double phase = 0.0;
    float lp = 0.0f, prev = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const double t = i / kSr;
        const double f = s.f1 + (s.f0 - s.f1) * std::exp (-t / s.sweepT);
        phase += juce::MathConstants<double>::twoPi * f / kSr;
        const float tone = (float) (std::sin (phase) * std::exp (-t / s.decay));

        const float nz = rng.nextFloat() * 2.0f - 1.0f;
        const float hp = (nz - prev) * 0.5f;
        prev = nz;

        const float click = hp * (float) std::exp (-t / 0.002) * s.click;

        float body = hp;
        if (s.noiseLP > 0.0f)
        {
            lp += s.noiseLP * (nz - lp);
            body = lp;
        }
        const float noise = body * (float) std::exp (-t / s.noiseDecay) * s.noise;

        d[i] = tone + click + noise;
    }

    const int fade = (int) (kSr * 0.005);
    for (int i = 0; i < fade && i < n; ++i)
        d[n - 1 - i] *= (float) i / (float) fade;

    const float mag = b.getMagnitude (0, n);
    if (mag > 0.0f)
        b.applyGain (0.7f / mag);
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
    const auto marker = dir.getChildFile ("Kicks").getChildFile ("Kick_Close.wav");
    if (marker.existsAsFile())
        return;

    const auto kicks = dir.getChildFile ("Kicks");
    const auto snares = dir.getChildFile ("Snares");
    const auto toms = dir.getChildFile ("Toms");

    //                       f0     f1    sweep  decay  dur   click noise nDecay nLP  seed
    writeWav (kicks.getChildFile ("Kick_Close.wav"),   synth ({ 180, 55, 0.020f, 0.14f, 0.5f, 0.6f, 0.0f, 0.1f, 0.0f, 1 }));
    writeWav (kicks.getChildFile ("Kick_Body.wav"),    synth ({  90, 45, 0.040f, 0.35f, 1.0f, 0.0f, 0.0f, 0.1f, 0.0f, 2 }));
    writeWav (kicks.getChildFile ("Kick_Room.wav"),    synth ({  60, 55, 0.100f, 0.30f, 1.5f, 0.0f, 0.9f, 0.40f, 0.08f, 3 }));
    writeWav (kicks.getChildFile ("Kick_Modern_01.wav"), synth ({ 170, 48, 0.025f, 0.30f, 0.9f, 0.5f, 0.0f, 0.1f, 0.0f, 4 }));
    writeWav (kicks.getChildFile ("Kick_Punch_01.wav"),  synth ({ 230, 58, 0.018f, 0.22f, 0.7f, 0.8f, 0.0f, 0.1f, 0.0f, 5 }));
    writeWav (kicks.getChildFile ("Kick_Rock_01.wav"),   synth ({ 150, 52, 0.030f, 0.42f, 1.1f, 0.4f, 0.0f, 0.1f, 0.0f, 6 }));
    writeWav (kicks.getChildFile ("Kick_Metal_01.wav"),  synth ({ 320, 68, 0.015f, 0.26f, 0.8f, 1.0f, 0.0f, 0.1f, 0.0f, 7 }));

    writeWav (snares.getChildFile ("Snare_Crack_01.wav"), synth ({ 260, 190, 0.030f, 0.10f, 0.7f, 0.4f, 0.9f, 0.18f, 0.0f, 8 }));
    writeWav (snares.getChildFile ("Snare_Tight_01.wav"), synth ({ 300, 210, 0.025f, 0.08f, 0.5f, 0.5f, 0.7f, 0.12f, 0.0f, 9 }));
    writeWav (snares.getChildFile ("Snare_Fat_01.wav"),   synth ({ 210, 150, 0.040f, 0.16f, 0.9f, 0.3f, 1.0f, 0.30f, 0.0f, 10 }));

    writeWav (toms.getChildFile ("Tom_High_01.wav"),  synth ({ 260, 170, 0.060f, 0.40f, 1.0f, 0.3f, 0.1f, 0.05f, 0.0f, 11 }));
    writeWav (toms.getChildFile ("Tom_Mid_01.wav"),   synth ({ 190, 120, 0.070f, 0.50f, 1.2f, 0.3f, 0.1f, 0.05f, 0.0f, 12 }));
    writeWav (toms.getChildFile ("Tom_Floor_01.wav"), synth ({ 130,  82, 0.080f, 0.65f, 1.4f, 0.3f, 0.1f, 0.05f, 0.0f, 13 }));
}
} // namespace FactorySamples
