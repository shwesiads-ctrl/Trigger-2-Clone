#include "PluginEditor.h"

using namespace BTUI;

namespace
{
constexpr int kDesignW = 1180;
constexpr int kDesignH = 533;
}

// Everything is laid out at a fixed 1180 x 533 design size; the editor scales this component
// to whatever window size the host gives it.
class BTriggerEditor::Content : public juce::Component, private juce::Timer
{
public:
    Content (BTriggerProcessor& p, BLook& look)
        : proc (p), library (p), wave (p)
    {
        setLookAndFeel (&look);
        setSize (kDesignW, kDesignH);
        auto& vt = proc.apvts;

        addAndMakeVisible (library);
        library.setBounds (12, 12, 208, 508);

        // input
        addAndMakeVisible (inMeter);
        inMeter.setBounds (251, 38, 68, 128);
        kIn.setup (*this, vt, "inGain", kGreen, [] (double v) { return String (v, 2) + " dB"; });
        kIn.slider.setBounds (268, 188, 34, 34);
        kIn.pill.setBounds (252, 226, 66, 20);

        // waveform and trigger button
        addAndMakeVisible (wave);
        wave.setBounds (336, 22, 532, 232);

        addAndMakeVisible (trigBtn);
        trigBtn.getProperties().set ("primary", true);
        trigBtn.setBounds (542, 259, 120, 26);
        trigBtn.onClick = [this] { proc.manualTrigger (0.85f); };

        // detection knobs
        kSens.setup (*this, vt, "sens", kOrange, [] (double v) { return String ((int) std::round (v)); });
        kRetr.setup (*this, vt, "retrig", kOrange, [] (double v) { return String ((int) std::round (v)) + " ms"; });
        kDet.setup (*this, vt, "detail", kOrange, [] (double v) { return String ((int) std::round (v)); });
        KnobUnit* rows[3] = { &kSens, &kRetr, &kDet };
        for (int r = 0; r < 3; ++r)
        {
            const int y0 = 24 + r * 74;
            rows[r]->slider.setBounds (888, y0 + 26, 38, 38);
            rows[r]->pill.setBounds (938, y0 + 30, 80, 28);
        }

        // output
        addAndMakeVisible (outMeter);
        outMeter.setBounds (1080, 38, 68, 80);
        kOut.setup (*this, vt, "outGain", kGreen, [] (double v) { return String (v, 2) + " dB"; });
        kOut.slider.setBounds (1099, 140, 30, 30);
        kOut.pill.setBounds (1081, 174, 66, 20);
        kMix.setup (*this, vt, "mix", kGreen, [] (double v) { return String ((int) std::round (v)) + "%"; });
        kMix.slider.setBounds (1099, 216, 30, 30);
        kMix.pill.setBounds (1081, 250, 66, 20);

        // sample layers
        for (int i = 0; i < BTriggerProcessor::kLayers; ++i)
        {
            strips[i] = std::make_unique<LayerStrip> (proc, i);
            addAndMakeVisible (*strips[i]);
            strips[i]->setBounds (243 + i * 231, 290, 225, 224);
        }

        startTimerHz (30);
    }

    ~Content() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        const float w = (float) getWidth(), h = (float) getHeight();

        ColourGradient base (Colour (0xffd6f23c), 0.0f, 0.0f, Colour (0xfff08a2a), w, h, false);
        base.addColour (0.55, Colour (0xfff4c63a));
        g.setGradientFill (base);
        g.fillAll();

        g.setGradientFill (ColourGradient (Colour (0xffb9f24a), w * 0.12f, h * 0.08f,
                                           Colour (0x00b9f24a), w * 0.12f + 520.0f, h * 0.08f, true));
        g.fillAll();
        g.setGradientFill (ColourGradient (Colour (0xfff58325), w * 0.98f, h, Colour (0x00f58325), w * 0.98f - 520.0f, h, true));
        g.fillAll();
        g.setGradientFill (ColourGradient (Colour (0xfff7e23a), w * 0.7f, -h * 0.05f, Colour (0x00f7e23a), w * 0.7f - 420.0f, -h * 0.05f, true));
        g.fillAll();

        drawGlass (g, { 240.0f, 10.0f, 90.0f, 254.0f }, 20.0f);
        drawGlass (g, { 1066.0f, 10.0f, 96.0f, 272.0f }, 20.0f);

        g.setColour (kInk);
        g.setFont (bFont (16.0f));
        g.drawText ("INPUT", 240, 14, 90, 22, Justification::centred, false);
        g.drawText ("OUTPUT", 1066, 14, 96, 22, Justification::centred, false);

        g.setFont (bFont (12.0f));
        g.drawText ("VOLUME", 240, 170, 90, 16, Justification::centred, false);
        g.drawText ("VOLUME", 1066, 122, 96, 16, Justification::centred, false);
        g.drawText ("MIX", 1066, 198, 96, 16, Justification::centred, false);

        g.setFont (bFont (16.0f));
        const char* names[3] = { "SENSITIVITY", "RETRIGGER", "DETAIL" };
        for (int r = 0; r < 3; ++r)
            g.drawText (names[r], 886, 24 + r * 74, 178, 22, Justification::centredLeft, false);
    }

private:
    void timerCallback() override
    {
        auto db = [] (float peak) { return Decibels::gainToDecibels (peak, -90.0f); };
        inMeter.setLevels (db (proc.inPeak[0].load()), db (proc.inPeak[1].load()));
        outMeter.setLevels (db (proc.outPeak[0].load()), db (proc.outPeak[1].load()));
        wave.repaint();

        const int rev = proc.layerRevision.load();
        if (rev != seenRevision)
        {
            seenRevision = rev;
            for (auto& s : strips)
                if (s != nullptr)
                    s->repaint();
        }
    }

    BTriggerProcessor& proc;
    LibraryPanel library;
    LedMeter inMeter, outMeter;
    WaveView wave;
    TextButton trigBtn { String::fromUTF8 ("\xe2\x96\xb6 TRIGGER") };
    KnobUnit kIn, kSens, kRetr, kDet, kOut, kMix;
    std::unique_ptr<LayerStrip> strips[BTriggerProcessor::kLayers];
    int seenRevision = -1;
};

// ----------------------------------------------------------------------------------------------

BTriggerEditor::BTriggerEditor (BTriggerProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    content = std::make_unique<Content> (proc, look);
    addAndMakeVisible (*content);

    setResizable (true, true);
    setResizeLimits (kDesignW / 2, kDesignH / 2, kDesignW * 2, kDesignH * 2);
    getConstrainer()->setFixedAspectRatio ((double) kDesignW / (double) kDesignH);
    setSize (kDesignW, kDesignH);
}

BTriggerEditor::~BTriggerEditor() = default;

void BTriggerEditor::resized()
{
    const float scale = (float) getWidth() / (float) kDesignW;
    content->setTransform (juce::AffineTransform::scale (scale));
    content->setBounds (0, 0, kDesignW, kDesignH);
}
