#pragma once
#include "UI.h"

namespace BTUI
{
// The Advanced page: Gate, Compressor, Delay, Reverb and EQ, each with its own on/off switch and a full set of knobs.
// These effects run on the sample layers, in this order: Gate > EQ > Compressor > Delay > Reverb.
class AdvancedPage : public Component
{
public:
    explicit AdvancedPage (BTriggerProcessor& p) : proc (p)
    {
        setInterceptsMouseClicks (false, true);
        using namespace BT::Fx;

        auto dB   = [] (double v) { return String (v, 1) + " dB"; };
        auto ms   = [] (double v) { return v < 10.0 ? String (v, 1) + " ms" : String ((int) std::round (v)) + " ms"; };
        auto hz   = [] (double v) { return v >= 1000.0 ? String (v / 1000.0, 2) + " kHz" : String ((int) std::round (v)) + " Hz"; };
        auto pct  = [] (double v) { return String ((int) std::round (v)) + "%"; };
        auto ratio = [] (double v) { return String (v, 1) + " : 1"; };
        auto q    = [] (double v) { return String (v, 2); };

        auto* gate = addPanel ("GATE", { 0, 0, 5 * kCell + 24, kPanelH }, GateOn);
        addKnob (gate, GThr,   "THRESHOLD", dB, 0, kOrange);
        addKnob (gate, GAtk,   "ATTACK",    ms, 1, kOrange);
        addKnob (gate, GHold,  "HOLD",      ms, 2, kOrange);
        addKnob (gate, GRel,   "RELEASE",   ms, 3, kOrange);
        addKnob (gate, GRange, "RANGE",     dB, 4, kOrange);

        auto* comp = addPanel ("COMPRESSOR", { gate->r.getRight() + kGap, 0, 7 * kCell + 24, kPanelH }, CompOn);
        addKnob (comp, CThr,    "THRESHOLD", dB,    0, kOrange);
        addKnob (comp, CRatio,  "RATIO",     ratio, 1, kOrange);
        addKnob (comp, CAtk,    "ATTACK",    ms,    2, kOrange);
        addKnob (comp, CRel,    "RELEASE",   ms,    3, kOrange);
        addKnob (comp, CKnee,   "KNEE",      dB,    4, kOrange);
        addKnob (comp, CMakeup, "MAKEUP",    dB,    5, kOrange);
        addKnob (comp, CMix,    "MIX",       pct,   6, kGreen);

        const int row2 = kPanelH + kGap;
        auto* delay = addPanel ("DELAY", { 0, row2, 6 * kCell + 24, kPanelH }, DelayOn);
        addKnob (delay, DTime, "TIME",     ms,  0, kOrange);
        addKnob (delay, DFb,   "FEEDBACK", pct, 1, kOrange);
        addKnob (delay, DLow,  "LOW CUT",  hz,  2, kOrange);
        addKnob (delay, DHigh, "HIGH CUT", hz,  3, kOrange);
        addKnob (delay, DMix,  "MIX",      pct, 4, kGreen);
        addAndMakeVisible (pingBtn);
        pingBtn.setButtonText ("PING-PONG");
        pingBtn.setClickingTogglesState (true);
        pingAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, def (DPing).id, pingBtn);
        pingBtn.setBounds (delay->r.getX() + 12 + 5 * kCell + 2, delay->r.getY() + 62, kCell - 4, 28);
        delay->extras.push_back (&pingBtn);

        auto* rev = addPanel ("REVERB", { delay->r.getRight() + kGap, row2, 5 * kCell + 24, kPanelH }, ReverbOn);
        addKnob (rev, RSize,  "SIZE",      pct, 0, kOrange);
        addKnob (rev, RDamp,  "DAMPING",   pct, 1, kOrange);
        addKnob (rev, RWidth, "WIDTH",     pct, 2, kOrange);
        addKnob (rev, RPre,   "PRE-DELAY", ms,  3, kOrange);
        addKnob (rev, RMix,   "MIX",       pct, 4, kGreen);

        auto* eq = addPanel ("EQ", { 0, 2 * (kPanelH + kGap), 11 * kCell + 24, kPanelH }, EqOn);
        addKnob (eq, EHp,  "HIGH-PASS", hz, 0,  kOrange);
        addKnob (eq, ELoF, "LOW FREQ",  hz, 1,  kOrange);
        addKnob (eq, ELoG, "LOW GAIN",  dB, 2,  kGreen);
        addKnob (eq, ELmF, "LM FREQ",   hz, 3,  kOrange);
        addKnob (eq, ELmG, "LM GAIN",   dB, 4,  kGreen);
        addKnob (eq, ELmQ, "LM Q",      q,  5,  kOrange);
        addKnob (eq, EHmF, "HM FREQ",   hz, 6,  kOrange);
        addKnob (eq, EHmG, "HM GAIN",   dB, 7,  kGreen);
        addKnob (eq, EHmQ, "HM Q",      q,  8,  kOrange);
        addKnob (eq, EHiF, "HIGH FREQ", hz, 9,  kOrange);
        addKnob (eq, EHiG, "HIGH GAIN", dB, 10, kGreen);

        for (auto& pn : panels)
            applyDim (*pn);
    }

    void paint (Graphics& g) override
    {
        for (auto& pn : panels)
        {
            drawGlass (g, pn->r.toFloat(), 18.0f);
            g.setColour (kInk);
            g.setFont (bFont (16.0f));
            g.drawText (pn->title, pn->r.getX() + 16, pn->r.getY() + 10, 180, 22, Justification::centredLeft, false);
        }

        g.setFont (bFont (11.5f));
        for (auto& c : cells)
            g.drawText (c->label, c->labelRect, Justification::centred, false);

        g.setFont (bFont (12.0f));
        g.setColour (kInk.withAlpha (0.75f));
        g.drawText ("These effects work on the sample layers.   Signal flow:  GATE  >  EQ  >  COMPRESSOR  >  DELAY  >  REVERB",
                    0, 3 * (kPanelH + kGap) - 2, getWidth(), 20, Justification::centredLeft, false);
    }

    static constexpr int kPageW = 932, kPageH = 513;

private:
    static constexpr int kCell = 70, kPanelH = 150, kGap = 12;

    struct Panel
    {
        String title;
        Rectangle<int> r;
        TextButton power { "ON" };
        std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> att;
        std::vector<Component*> parts;     // knobs and pills that dim when the effect is off
        std::vector<Component*> extras;
    };

    struct Cell
    {
        KnobUnit k;
        String label;
        Rectangle<int> labelRect;
    };

    Panel* addPanel (const String& title, Rectangle<int> r, int onParam)
    {
        auto pn = std::make_unique<Panel>();
        pn->title = title;
        pn->r = r;
        addAndMakeVisible (pn->power);
        pn->power.setClickingTogglesState (true);
        pn->power.setBounds (r.getRight() - 12 - 52, r.getY() + 9, 52, 24);
        pn->att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, BT::Fx::def (onParam).id, pn->power);
        auto* raw = pn.get();
        pn->power.onStateChange = [this, raw] { applyDim (*raw); };
        panels.push_back (std::move (pn));
        return raw;
    }

    void addKnob (Panel* pn, int id, const String& label, std::function<String (double)> fmt, int col, Colour ring)
    {
        auto c = std::make_unique<Cell>();
        const int x = pn->r.getX() + 12 + col * kCell, y = pn->r.getY() + 44;
        c->label = label;
        c->labelRect = { x, y, kCell, 14 };
        c->k.setup (*this, proc.apvts, BT::Fx::def (id).id, ring, std::move (fmt));
        c->k.slider.setBounds (x + 11, y + 18, 48, 48);
        c->k.pill.setBounds (x + 3, y + 70, kCell - 6, 20);
        pn->parts.push_back (&c->k.slider);
        pn->parts.push_back (&c->k.pill);
        cells.push_back (std::move (c));
    }

    void applyDim (Panel& pn)
    {
        const bool on = pn.power.getToggleState();
        pn.power.setButtonText (on ? "ON" : "OFF");
        const float a = on ? 1.0f : 0.5f;
        for (auto* c : pn.parts)
            c->setAlpha (a);
        for (auto* c : pn.extras)
            c->setAlpha (a);
        repaint();
    }

    BTriggerProcessor& proc;
    std::vector<std::unique_ptr<Panel>> panels;
    std::vector<std::unique_ptr<Cell>> cells;
    TextButton pingBtn;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> pingAtt;
};
} // namespace BTUI
