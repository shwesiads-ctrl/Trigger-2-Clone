#include "PluginEditor.h"
#include "FxPage.h"

using namespace BTUI;

namespace
{
constexpr int kDesignW = 1180;
constexpr int kDesignH = 577;     // 44 px top bar + the 533 px main area
constexpr int kBarH = 44;
}

// A transparent page that paints its own panels and lets mouse clicks fall through to what is under it.
struct OverlayPage : public juce::Component
{
    std::function<void (juce::Graphics&)> paintFn;
    OverlayPage() { setInterceptsMouseClicks (false, true); }
    void paint (juce::Graphics& g) override { if (paintFn) paintFn (g); }
};

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

        // ---- top bar: A/B, Reset, Basic / Advanced
        for (auto* b : { &abA, &abB })
        {
            addAndMakeVisible (*b);
            b->setRadioGroupId (101);
            b->setClickingTogglesState (true);
        }
        abA.setBounds (24, 10, 36, 28);
        abB.setBounds (64, 10, 36, 28);
        abA.onClick = [this] { if (abA.getToggleState()) proc.selectAB (0); };
        abB.onClick = [this] { if (abB.getToggleState()) proc.selectAB (1); };

        addAndMakeVisible (copyBtn);
        copyBtn.setBounds (110, 10, 96, 28);
        copyBtn.onClick = [this] { proc.copyToOtherAB(); };

        addAndMakeVisible (resetBtn);
        resetBtn.setBounds (222, 10, 80, 28);
        resetBtn.onClick = [this]
        {
            SafePointer<Component> sp (this);
            AlertWindow::showAsync (MessageBoxOptions()
                                        .withIconType (MessageBoxIconType::QuestionIcon)
                                        .withTitle ("Reset")
                                        .withMessage ("Put every knob and layer back to the starting settings?")
                                        .withButton ("Reset")
                                        .withButton ("Cancel")
                                        .withAssociatedComponent (this),
                                    [this, sp] (int r)
                                    {
                                        if (r == 0 && sp != nullptr)
                                            proc.resetAll();
                                    });
        };

        for (auto* b : { &basicBtn, &advBtn })
        {
            addAndMakeVisible (*b);
            b->setRadioGroupId (102);
            b->setClickingTogglesState (true);
        }
        basicBtn.setBounds (988, 10, 80, 28);
        advBtn.setBounds (1072, 10, 88, 28);
        basicBtn.onClick = [this] { if (basicBtn.getToggleState()) { proc.advancedPage = false; syncTopBar(); } };
        advBtn.onClick   = [this] { if (advBtn.getToggleState())   { proc.advancedPage = true;  syncTopBar(); } };

        // ---- library stays visible on both pages
        addAndMakeVisible (library);
        library.setBounds (12, 12 + kBarH, 208, 508);

        // ---- pages sit on top of the library's neighbours and let clicks through
        addAndMakeVisible (basicPage);
        basicPage.setBounds (0, kBarH, kDesignW, 533);
        basicPage.paintFn = [this] (Graphics& g) { paintBasic (g); };

        addChildComponent (advPage);
        advPage.setBounds (236, 12 + kBarH, AdvancedPage::kPageW, AdvancedPage::kPageH);

        // input
        basicPage.addAndMakeVisible (inMeter);
        inMeter.setBounds (251, 38, 68, 128);
        kIn.setup (basicPage, vt, "inGain", kGreen, [] (double v) { return String (v, 2) + " dB"; });
        kIn.slider.setBounds (268, 188, 34, 34);
        kIn.pill.setBounds (252, 226, 66, 20);

        // waveform and trigger button
        basicPage.addAndMakeVisible (wave);
        wave.setBounds (336, 22, 532, 232);

        basicPage.addAndMakeVisible (trigBtn);
        trigBtn.getProperties().set ("primary", true);
        trigBtn.setBounds (542, 259, 120, 26);
        trigBtn.onClick = [this] { proc.manualTrigger (0.85f); };

        // REPLACE: on = the original drum is removed and only the samples play. Off = blend using MIX.
        basicPage.addAndMakeVisible (replaceBtn);
        replaceBtn.setClickingTogglesState (true);
        replaceBtn.setBounds (372, 259, 150, 26);
        replaceAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (vt, "replace", replaceBtn);

        basicPage.addAndMakeVisible (addLayerBtn);
        addLayerBtn.setBounds (682, 259, 150, 26);
        addLayerBtn.onClick = [this] { proc.addLayer(); };

        // detection knobs
        kSens.setup (basicPage, vt, "sens", kOrange, [] (double v) { return String ((int) std::round (v)); });
        kRetr.setup (basicPage, vt, "retrig", kOrange, [] (double v) { return String ((int) std::round (v)) + " ms"; });
        kDet.setup (basicPage, vt, "detail", kOrange, [] (double v) { return String ((int) std::round (v)); });
        KnobUnit* rows[3] = { &kSens, &kRetr, &kDet };
        for (int r = 0; r < 3; ++r)
        {
            const int y0 = 24 + r * 74;
            rows[r]->slider.setBounds (888, y0 + 26, 38, 38);
            rows[r]->pill.setBounds (938, y0 + 30, 80, 28);
        }

        // output
        basicPage.addAndMakeVisible (outMeter);
        outMeter.setBounds (1080, 38, 68, 80);
        kOut.setup (basicPage, vt, "outGain", kGreen, [] (double v) { return String (v, 2) + " dB"; });
        kOut.slider.setBounds (1099, 140, 30, 30);
        kOut.pill.setBounds (1081, 174, 66, 20);
        kMix.setup (basicPage, vt, "mix", kGreen, [] (double v) { return String ((int) std::round (v)) + "%"; });
        kMix.slider.setBounds (1099, 216, 30, 30);
        kMix.pill.setBounds (1081, 250, 66, 20);

        // sample layers: a scrolling row, so more than four layers still fit
        basicPage.addAndMakeVisible (stripView);
        stripView.setBounds (243, 290, 918, 232);
        stripView.setViewedComponent (&stripHolder, false);
        stripView.setScrollBarsShown (false, true);
        stripView.setScrollBarThickness (8);
        stripView.setColour (ScrollBar::thumbColourId, Colours::white.withAlpha (0.8f));
        rebuildStrips();

        syncTopBar();
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

        // top bar
        drawGlass (g, { 12.0f, 6.0f, 1156.0f, 36.0f }, 18.0f);
        g.setColour (kInk);
        g.setFont (bFont (13.0f));
        g.drawText ("B TRIGGER", 330, 6, 520, 36, Justification::centred, false);
    }

    void paintBasic (juce::Graphics& g)
    {
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

        const int srev = proc.structureRevision.load();
        if (srev != seenStructure)
            rebuildStrips();

        const bool replace = replaceBtn.getToggleState();
        const float a = replace ? 0.4f : 1.0f;
        if (kMix.slider.getAlpha() != a)
        {
            kMix.slider.setAlpha (a);
            kMix.pill.setAlpha (a);
            kMix.slider.setEnabled (! replace);
        }
        addLayerBtn.setEnabled (proc.activeLayerCount() < BTriggerProcessor::kLayers);
        syncTopBar();
    }

    // keeps the A/B, copy and page buttons in step with the processor (it can change from a project load or the host)
    void syncTopBar()
    {
        const int ab = proc.currentAB();
        abA.setToggleState (ab == 0, dontSendNotification);
        abB.setToggleState (ab == 1, dontSendNotification);
        const String copyText = ab == 0 ? "COPY TO B" : "COPY TO A";
        if (copyBtn.getButtonText() != copyText)
            copyBtn.setButtonText (copyText);

        const bool adv = proc.advancedPage.load();
        basicBtn.setToggleState (! adv, dontSendNotification);
        advBtn.setToggleState (adv, dontSendNotification);
        if (basicPage.isVisible() == adv)
        {
            basicPage.setVisible (! adv);
            advPage.setVisible (adv);
        }
    }

    void rebuildStrips()
    {
        seenStructure = proc.structureRevision.load();
        const int keepX = stripView.getViewPositionX();
        strips.clear();

        int n = 0;
        for (int slot = 0; slot < BTriggerProcessor::kLayers; ++slot)
        {
            if (! proc.isLayerActive (slot))
                continue;
            auto strip = std::make_unique<LayerStrip> (proc, slot);
            strip->setBounds (n * 231, 0, 225, 224);
            strip->onRemove = [this, slot] { proc.removeLayer (slot); };
            stripHolder.addAndMakeVisible (*strip);
            strips.push_back (std::move (strip));
            ++n;
        }
        for (auto& s : strips)
            s->setRemovable (n > 1);

        stripHolder.setSize (jmax (918, n * 231 - 6), 224);
        stripView.setViewPosition (jmin (keepX, jmax (0, stripHolder.getWidth() - 918)), 0);
    }

    BTriggerProcessor& proc;
    LibraryPanel library;
    LedMeter inMeter, outMeter;
    WaveView wave;
    TextButton abA { "A" }, abB { "B" }, copyBtn { "COPY TO B" }, resetBtn { "RESET" }, basicBtn { "BASIC" }, advBtn { "ADVANCED" };
    OverlayPage basicPage;
    AdvancedPage advPage { proc };
    TextButton trigBtn { String::fromUTF8 ("\xe2\x96\xb6 TRIGGER") };
    TextButton replaceBtn { "REPLACE ORIGINAL" }, addLayerBtn { String::fromUTF8 ("\xef\xbc\x8b ADD LAYER") };
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> replaceAtt;
    KnobUnit kIn, kSens, kRetr, kDet, kOut, kMix;
    Viewport stripView;
    Component stripHolder;
    std::vector<std::unique_ptr<LayerStrip>> strips;
    int seenRevision = -1, seenStructure = -1;
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
