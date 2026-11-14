#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <algorithm>
#include <functional>
#include "PluginProcessor.h"
#include "FactorySamples.h"
#include "Presets.h"
#include "TciConverter.h"

// All custom UI pieces for B Trigger: look and feel, meters, waveform, knobs, layer strips, library.
namespace BTUI
{
using namespace juce;
using SliderAttachment = AudioProcessorValueTreeState::SliderAttachment;
using ButtonAttachment = AudioProcessorValueTreeState::ButtonAttachment;

inline const Colour kLime   = Colour (0xffc9f23c);
inline const Colour kGreen  = Colour (0xff9fd321);
inline const Colour kOrange = Colour (0xfff0661c);
inline const Colour kInk    = Colour (0xff2b2a22);
inline const Colour kWell   = Colour (0xff2a2e20);

inline Font bFont (float height, bool bold = true)
{
    return Font (FontOptions (height, bold ? Font::bold : Font::plain));
}

inline void drawGlass (Graphics& g, Rectangle<float> r, float radius, float fillAlpha = 0.40f)
{
    Path p;
    p.addRoundedRectangle (r, radius);
    DropShadow (Colour (0x2a823c00), 14, { 0, 6 }).drawForPath (g, p);
    g.setColour (Colours::white.withAlpha (fillAlpha));
    g.fillPath (p);
    g.setColour (Colours::white.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

inline void drawPill (Graphics& g, Rectangle<float> r, float radius, float alpha = 0.58f)
{
    g.setColour (Colours::white.withAlpha (alpha));
    g.fillRoundedRectangle (r, radius);
    g.setColour (Colours::white.withAlpha (0.8f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

inline void drawLogo (Graphics& g, Rectangle<float> r)
{
    const float cx = r.getCentreX(), cy = r.getCentreY(), s = r.getWidth() / 56.0f;
    auto diamond = [&] (float half, Colour fill, Colour stroke)
    {
        Path p;
        p.startNewSubPath (cx, cy - half * s);
        p.lineTo (cx + half * s, cy);
        p.lineTo (cx, cy + half * s);
        p.lineTo (cx - half * s, cy);
        p.closeSubPath();
        g.setColour (fill);
        g.fillPath (p);
        g.setColour (stroke);
        g.strokePath (p, PathStrokeType (1.2f * s));
    };
    diamond (27.0f, Colour (0xff161614), Colour (0xff3d3d3a));
    diamond (22.0f, Colour (0xff1d1d1b), Colour (0xff4a4a46));
    diamond (17.0f, Colour (0xff2a2a28), Colour (0xff4a4a46));
    g.setColour (Colours::white);
    g.setFont (Font (FontOptions (Font::getDefaultSerifFontName(), 25.0f * s, Font::bold)));
    g.drawText ("B", r.translated (0.0f, 0.5f * s), Justification::centred, false);
}

// ---------------------------------------------------------------------------------------------
class BLook : public LookAndFeel_V4
{
public:
    BLook()
    {
        setColour (TextButton::textColourOffId, kInk);
        setColour (TextButton::textColourOnId, kInk);
        setColour (Label::textColourId, kInk);
        setColour (ScrollBar::thumbColourId, Colours::white.withAlpha (0.75f));
        setColour (ScrollBar::backgroundColourId, Colours::transparentBlack);
    }

    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, Slider& s) override
    {
        const auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
        const float r = jmin (b.getWidth(), b.getHeight()) / 2.0f;
        const auto c = b.getCentre();
        const Colour ring ((uint32) (int) s.getProperties().getWithDefault ("ring", (int) 0xfff0661c));

        g.setColour (Colour (0x40502800));
        g.fillEllipse (c.x - r, c.y - r + 3.0f, 2.0f * r, 2.0f * r);

        g.setGradientFill (ColourGradient (Colours::white, c.x - r * 0.3f, c.y - r * 0.4f,
                                           Colour (0xffdcd2bf), c.x + r * 0.8f, c.y + r * 0.9f, true));
        g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);

        g.setColour (ring);
        g.drawEllipse (c.x - r + 2.0f, c.y - r + 2.0f, 2.0f * r - 4.0f, 2.0f * r - 4.0f, 4.0f);

        const float angle = startAngle + pos * (endAngle - startAngle);
        Path p;
        p.addRoundedRectangle (-1.6f, -r * 0.72f, 3.2f, r * 0.42f, 1.6f);
        g.setColour (kInk);
        g.fillPath (p, AffineTransform::rotation (angle).translated (c.x, c.y));
    }

    int getSliderThumbRadius (Slider& s) override
    {
        return s.getProperties().contains ("fader") ? 12 : LookAndFeel_V4::getSliderThumbRadius (s);
    }

    void drawLinearSlider (Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float minPos, float maxPos, Slider::SliderStyle style, Slider& s) override
    {
        if (! s.getProperties().contains ("fader"))
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minPos, maxPos, style, s);
            return;
        }

        const auto b = Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const float cx = b.getCentreX();
        const float top = b.getY() + 2.0f, bottom = b.getBottom() - 2.0f;

        g.setColour (Colour (0xbb6a604a));
        for (float ty = top + 2.0f; ty <= bottom - 2.0f; ty += 8.0f)
        {
            g.fillRect (cx - 16.0f, ty, 7.0f, 1.0f);
            g.fillRect (cx + 9.0f, ty, 7.0f, 1.0f);
        }

        const auto track = Rectangle<float> (cx - 3.5f, top, 7.0f, bottom - top);
        g.setGradientFill (ColourGradient (Colour (0xffc9461a), track.getX(), 0.0f,
                                           Colour (0xfff07a34), track.getCentreX(), 0.0f, false));
        g.fillRoundedRectangle (track, 3.5f);

        const auto thumb = Rectangle<float> (cx - 18.0f, sliderPos - 11.5f, 36.0f, 23.0f);
        g.setColour (Colour (0x55462800));
        g.fillRoundedRectangle (thumb.translated (0.0f, 3.0f), 6.0f);
        g.setGradientFill (ColourGradient (Colours::white, 0.0f, thumb.getY(), Colour (0xffe7e2d6), 0.0f, thumb.getBottom(), false));
        g.fillRoundedRectangle (thumb, 6.0f);
        g.setColour (Colour (0xffcfc7b4));
        g.drawRoundedRectangle (thumb.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (Colour (0xff9d957f));
        for (int i = -1; i <= 1; ++i)
            g.fillRect (thumb.getX() + 6.0f, thumb.getCentreY() + (float) i * 3.5f - 0.5f, thumb.getWidth() - 12.0f, 1.0f);
    }

    void drawLabel (Graphics& g, Label& l) override
    {
        if (! l.getProperties().contains ("pill"))
        {
            LookAndFeel_V4::drawLabel (g, l);
            return;
        }
        const auto b = l.getLocalBounds().toFloat();
        const float radius = b.getHeight() * 0.34f;
        g.setColour (Colours::white.withAlpha (0.58f));
        g.fillRoundedRectangle (b, radius);
        g.setColour (Colours::white.withAlpha (0.8f));
        g.drawRoundedRectangle (b.reduced (0.5f), radius, 1.0f);
        g.setColour (kInk);
        g.setFont (bFont (b.getHeight() * 0.6f));
        g.drawText (l.getText(), l.getLocalBounds(), Justification::centred, false);
    }

    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool) override
    {
        const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool primary = b.getProperties().contains ("primary");
        const bool on = b.getToggleState();
        const bool orange = b.getProperties().contains ("orange");

        Colour top = Colours::white.withAlpha (0.55f), bottom = top;
        if (primary || (on && ! orange))
        {
            top = Colour (0xffd9ff4a);
            bottom = Colour (0xffb6ec22);
        }
        else if (on)
        {
            top = Colour (0xffffb26a);
            bottom = kOrange;
        }
        if (over)
        {
            top = top.brighter (0.1f);
            bottom = bottom.brighter (0.1f);
        }
        g.setGradientFill (ColourGradient (top, 0.0f, r.getY(), bottom, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (primary || on ? Colour (0xffeaff90) : Colours::white.withAlpha (0.85f));
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
    }

    void fillTextEditorBackground (Graphics& g, int w, int h, TextEditor& t) override
    {
        g.setColour (t.findColour (TextEditor::backgroundColourId));
        g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 12.0f);
    }

    void drawTextEditorOutline (Graphics& g, int w, int h, TextEditor& t) override
    {
        g.setColour (t.findColour (t.hasKeyboardFocus (true) ? TextEditor::focusedOutlineColourId
                                                              : TextEditor::outlineColourId));
        g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 12.0f, 1.0f);
    }

    Font getTextButtonFont (TextButton&, int buttonHeight) override { return bFont (jmin (13.0f, (float) buttonHeight * 0.5f)); }
};

// ---------------------------------------------------------------------------------------------
class LedMeter : public Component
{
public:
    void setLevels (float l, float r)
    {
        if (std::abs (l - dbL) > 0.3f || std::abs (r - dbR) > 0.3f)
        {
            dbL = l;
            dbR = r;
            repaint();
        }
    }

    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (kWell);
        g.fillRoundedRectangle (b, 9.0f);

        const auto area = b.reduced (7.0f, 6.0f);
        constexpr int rows = 11;
        static const float th[rows] = { 0.0f, -1.5f, -3.0f, -4.5f, -6.0f, -9.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f };
        const float gap = 2.0f, colW = 12.0f;
        const float rowH = (area.getHeight() - gap * (float) (rows - 1)) / (float) rows;

        for (int r = 0; r < rows; ++r)
        {
            const float y = area.getY() + (float) r * (rowH + gap);
            for (int c = 0; c < 2; ++c)
            {
                const float x = c == 0 ? area.getX() : area.getRight() - colW;
                const bool lit = (c == 0 ? dbL : dbR) >= th[r];
                g.setColour (lit ? (r < 2 ? Colour (0xffffb02e) : kLime) : Colour (0xff4b5230));
                g.fillRoundedRectangle (x, y, colW, rowH, 2.0f);
            }
        }

        static const char* labels[6] = { "-0", "-3", "-6", "-12", "-24", "-48" };
        g.setColour (Colour (0xffd9d9bc));
        g.setFont (bFont (8.5f));
        for (int i = 0; i < 6; ++i)
        {
            const float cy = area.getY() + rowH * 0.5f + (float) i * (area.getHeight() - rowH) / 5.0f;
            g.drawText (labels[i], Rectangle<float> (area.getX() + colW, cy - 6.0f, area.getWidth() - 2.0f * colW, 12.0f),
                        Justification::centred, false);
        }
    }

private:
    float dbL = -90.0f, dbR = -90.0f;
};

// ---------------------------------------------------------------------------------------------
class WaveView : public Component
{
public:
    explicit WaveView (BTriggerProcessor& p) : proc (p) {}

    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setGradientFill (ColourGradient (Colour (0xff3b4029), b.getCentreX(), b.getCentreY() * 0.9f,
                                           Colour (0xff272b1d), b.getX(), b.getY(), true));
        g.fillRoundedRectangle (b, 20.0f);

        const auto area = b.reduced (12.0f, 14.0f);
        const float cy = area.getCentreY();
        const float half = area.getHeight() * 0.5f;
        const int cols = BTriggerProcessor::kCols;
        const float colW = area.getWidth() / (float) cols;
        const int head = proc.waveHead.load();

        g.setColour (Colour (0xffd4f53a));
        for (int c = 0; c < cols; ++c)
        {
            const int idx = (head + c) % cols;
            const float db = Decibels::gainToDecibels (proc.wavePeaks[idx].load(), -60.0f);
            const float h = jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * half;
            if (h > 0.5f)
                g.fillRect (area.getX() + (float) c * colW, cy - h, jmax (1.0f, colW * 0.75f), h * 2.0f);
        }

        g.setColour (Colour (0x88c9f23c));
        g.fillRect (area.getX(), cy - 0.5f, area.getWidth(), 1.0f);

        const float sens = proc.apvts.getRawParameterValue ("sens")->load();
        const float thr = jlimit (0.0f, 1.0f, (BTriggerProcessor::thresholdDb (sens) + 60.0f) / 60.0f) * half;
        g.setColour (Colours::white.withAlpha (0.85f));
        g.fillRect (area.getX(), cy - thr - 0.75f, area.getWidth(), 1.5f);
        g.fillRect (area.getX(), cy + thr - 0.75f, area.getWidth(), 1.5f);

        g.setColour (kOrange);
        for (int c = 0; c < cols; ++c)
        {
            const int idx = (head + c) % cols;
            if (proc.waveMarks[idx].load() == 0)
                continue;
            const float x = area.getX() + ((float) c + 0.5f) * colW;
            g.fillRect (x - 1.0f, area.getY() - 4.0f, 2.0f, area.getHeight() + 8.0f);
            Path t;
            t.addTriangle (x - 6.0f, area.getY() - 8.0f, x + 6.0f, area.getY() - 8.0f, x, area.getY() + 1.0f);
            t.addTriangle (x - 6.0f, area.getBottom() + 8.0f, x + 6.0f, area.getBottom() + 8.0f, x, area.getBottom() - 1.0f);
            g.fillPath (t);
        }

        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (1.0f), 20.0f, 2.0f);
    }

private:
    BTriggerProcessor& proc;
};

// ---------------------------------------------------------------------------------------------
struct KnobUnit
{
    Slider slider;
    Label pill;
    std::unique_ptr<SliderAttachment> att;

    void setup (Component& parent, AudioProcessorValueTreeState& vt, const String& id, Colour ring,
                std::function<String (double)> fmt)
    {
        slider.setSliderStyle (Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
        slider.setMouseDragSensitivity (160);
        slider.getProperties().set ("ring", (int) ring.getARGB());
        pill.getProperties().set ("pill", true);
        pill.setJustificationType (Justification::centred);
        pill.setInterceptsMouseClicks (false, false);
        parent.addAndMakeVisible (slider);
        parent.addAndMakeVisible (pill);

        att = std::make_unique<SliderAttachment> (vt, id, slider);
        slider.onValueChange = [this, fmt] { pill.setText (fmt (slider.getValue()), dontSendNotification); };
        slider.onValueChange();
    }
};

// ---------------------------------------------------------------------------------------------
class LayerStrip : public Component, public DragAndDropTarget, public FileDragAndDropTarget
{
public:
    LayerStrip (BTriggerProcessor& p, int i) : proc (p), index (i)
    {
        auto& vt = proc.apvts;

        addAndMakeVisible (fader);
        fader.setSliderStyle (Slider::LinearVertical);
        fader.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        fader.getProperties().set ("fader", true);
        faderAtt = std::make_unique<SliderAttachment> (vt, BT::lvlId (i), fader);

        addAndMakeVisible (dbPill);
        dbPill.getProperties().set ("pill", true);
        dbPill.setJustificationType (Justification::centred);
        dbPill.setInterceptsMouseClicks (false, false);
        fader.onValueChange = [this] { dbPill.setText (String (fader.getValue(), 2) + " dB", dontSendNotification); };
        fader.onValueChange();

        pan.setup (*this, vt, BT::panId (i), kGreen, [] (double v)
                   {
                       const int p = (int) std::round (v);
                       return p == 0 ? String ("<C>") : (p < 0 ? "L" + String (-p) : "R" + String (p));
                   });
        tune.setup (*this, vt, BT::tuneId (i), kOrange, [] (double v) { return String ((int) std::round (v)); });

        setupButton (sBtn, soloAtt, BT::soloId (i), false);
        setupButton (mBtn, muteAtt, BT::muteId (i), true);
        setupButton (pBtn, phaseAtt, BT::phaseId (i), false);

        addChildComponent (removeBtn);
        removeBtn.onClick = [this] { if (onRemove) onRemove(); };
        removeBtn.setTooltip ("Remove this layer");

        addChildComponent (nameEdit);
        nameEdit.setFont (bFont (16.0f));
        nameEdit.setJustification (Justification::centred);
        nameEdit.setMultiLine (false);
        nameEdit.setSelectAllWhenFocused (true);
        nameEdit.setInputRestrictions (32);
        nameEdit.setColour (TextEditor::backgroundColourId, Colours::white.withAlpha (0.9f));
        nameEdit.setColour (TextEditor::textColourId, kInk);
        nameEdit.setColour (TextEditor::outlineColourId, Colours::transparentBlack);
        nameEdit.setColour (TextEditor::focusedOutlineColourId, kOrange);
        nameEdit.onReturnKey = [this] { finishRename (true); };
        nameEdit.onEscapeKey = [this] { finishRename (false); };
        nameEdit.onFocusLost = [this] { finishRename (true); };
    }

    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (Colours::white.withAlpha (highlight ? 0.6f : 0.34f));
        g.fillRoundedRectangle (b, 16.0f);
        g.setColour (Colours::white.withAlpha (0.7f));
        g.drawRoundedRectangle (b.reduced (0.5f), 16.0f, 1.0f);

        drawPill (g, { 6.0f, 6.0f, b.getWidth() - 12.0f, 24.0f }, 8.0f, 0.5f);
        g.setColour (kInk);
        g.setFont (bFont (16.0f));
        g.drawText (proc.layerDisplayName (index), 30, 6, getWidth() - 60, 24, Justification::centred, true);

        // small pencil: click the title to rename the layer
        Path pen;
        pen.addRoundedRectangle (-1.6f, -7.0f, 3.2f, 10.0f, 1.0f);
        pen.addTriangle (-1.6f, 3.0f, 1.6f, 3.0f, 0.0f, 6.5f);
        g.setColour (kInk.withAlpha (0.5f));
        g.fillPath (pen, AffineTransform::rotation (0.7854f).translated (19.0f, 18.0f));
        g.setColour (kInk);

        g.setFont (bFont (12.0f));
        g.drawText ("PAN", knobColX1 - 23, 58, 46, 14, Justification::centred, false);
        g.drawText ("TUNE", knobColX2 - 23, 58, 46, 14, Justification::centred, false);

        drawPill (g, slotBounds.toFloat(), 10.0f);
        if (proc.layerHasSample (index))
        {
            g.setColour (kInk);
            g.setFont (bFont (14.0f));
            g.drawText (proc.layerFileName (index), slotBounds.reduced (6, 0), Justification::centred, true);
        }
        else
        {
            g.setColour (kInk.withAlpha (0.6f));
            g.setFont (bFont (14.0f, false).italicised());
            g.drawText ("Drop sample here", slotBounds, Justification::centred, false);
        }
    }

    void resized() override
    {
        fader.setBounds (6, 36, 38, 118);
        pan.slider.setBounds (knobColX1 - 17, 75, 34, 34);
        pan.pill.setBounds (knobColX1 - 23, 112, 46, 19);
        tune.slider.setBounds (knobColX2 - 17, 75, 34, 34);
        tune.pill.setBounds (knobColX2 - 23, 112, 46, 19);
        const int bx = getWidth() - 6 - 24;
        sBtn.setBounds (bx, 53, 24, 24);
        mBtn.setBounds (bx, 83, 24, 24);
        pBtn.setBounds (bx, 113, 24, 24);
        dbPill.setBounds ((getWidth() - 90) / 2, 158, 90, 20);
        slotBounds = { 8, 182, getWidth() - 16, 34 };
        titleBounds = { 6, 6, getWidth() - 12, 24 };
        nameEdit.setBounds (titleBounds);
        removeBtn.setBounds (getWidth() - 6 - 25, 8, 22, 22);
    }

    std::function<void()> onRemove;
    void setRemovable (bool r) { removeBtn.setVisible (r); }

    // click the title to give the layer your own name
    void startRename()
    {
        nameEdit.setText (proc.layerCustomName (index), dontSendNotification);
        nameEdit.setVisible (true);
        nameEdit.grabKeyboardFocus();
        nameEdit.selectAll();
    }

    // drag the loaded sample onto another layer to copy it there
    void mouseDrag (const MouseEvent& e) override
    {
        if (dragStarted || ! slotBounds.contains (e.getMouseDownPosition()) || ! proc.layerHasSample (index)
            || e.mods.isPopupMenu() || e.getDistanceFromDragStart() < 8)
            return;
        if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
        {
            dragStarted = true;
            c->startDragging ("BT:" + proc.layerPath (index), this);
        }
    }

    void mouseUp (const MouseEvent& e) override
    {
        const bool wasDragging = dragStarted;
        dragStarted = false;
        if (wasDragging)
            return;
        if (titleBounds.contains (e.getPosition()) && ! e.mouseWasDraggedSinceMouseDown() && ! e.mods.isPopupMenu())
        {
            startRename();
            return;
        }
        if (! slotBounds.contains (e.getPosition()) || e.mouseWasDraggedSinceMouseDown())
            return;
        if (e.mods.isPopupMenu())
        {
            proc.clearLayer (index);
            repaint();
        }
        else
            openChooser();
    }

    // drag from the library
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("BT:"); }
    void itemDragEnter (const SourceDetails&) override { setHighlight (true); }
    void itemDragExit (const SourceDetails&) override { setHighlight (false); }
    void itemDropped (const SourceDetails& d) override
    {
        setHighlight (false);
        proc.loadLayerFile (index, File (d.description.toString().substring (3)));
        repaint();
    }

    // drag from the operating system
    bool isInterestedInFileDrag (const StringArray& files) override { return ! files.isEmpty(); }
    void filesDropped (const StringArray& files, int, int) override
    {
        if (proc.loadLayerFile (index, File (files[0])))
            repaint();
    }

private:
    void setHighlight (bool h) { highlight = h; repaint(); }

    void finishRename (bool apply)
    {
        if (! nameEdit.isVisible())
            return;
        if (apply)
            proc.setLayerName (index, nameEdit.getText());
        nameEdit.setVisible (false);
        repaint();
    }

    void setupButton (TextButton& b, std::unique_ptr<ButtonAttachment>& att, const String& id, bool orange)
    {
        b.setClickingTogglesState (true);
        if (orange)
            b.getProperties().set ("orange", true);
        addAndMakeVisible (b);
        att = std::make_unique<ButtonAttachment> (proc.apvts, id, b);
    }

    void openChooser()
    {
        chooser = std::make_unique<FileChooser> ("Choose a sample", File(),
                                                 "*.wav;*.wave;*.mp3;*.flac;*.ogg;*.aif;*.aiff;*.m4a");
        SafePointer<LayerStrip> sp (this);
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [sp] (const FileChooser& fc)
                              {
                                  if (auto* self = sp.getComponent())
                                  {
                                      const auto f = fc.getResult();
                                      if (f.existsAsFile())
                                          self->proc.loadLayerFile (self->index, f);
                                      self->repaint();
                                  }
                              });
    }

    BTriggerProcessor& proc;
    const int index;
    bool highlight = false, dragStarted = false;
    Rectangle<int> titleBounds;
    TextEditor nameEdit;
    TextButton removeBtn { String::fromUTF8 ("\xc3\x97") };
    static constexpr int knobColX1 = 92, knobColX2 = 146;
    Rectangle<int> slotBounds;

    Slider fader;
    Label dbPill;
    KnobUnit pan, tune;
    TextButton sBtn { "S" }, mBtn { "M" }, pBtn { String::fromUTF8 ("\xc3\x98") };
    std::unique_ptr<SliderAttachment> faderAtt;
    std::unique_ptr<ButtonAttachment> soloAtt, muteAtt, phaseAtt;
    std::unique_ptr<FileChooser> chooser;
};

// ---------------------------------------------------------------------------------------------
class LibraryPanel : public Component, public FileDragAndDropTarget, private Timer
{
public:
    explicit LibraryPanel (BTriggerProcessor& p) : proc (p)
    {
        for (auto* b : { &tabLibrary, &tabPresets })
        {
            b->setClickingTogglesState (true);
            b->setRadioGroupId (101);
            addAndMakeVisible (*b);
        }
        tabLibrary.setToggleState (true, dontSendNotification);
        tabLibrary.onClick = [this] { showTab (true); };
        tabPresets.onClick = [this] { showTab (false); };

        addAndMakeVisible (search);
        search.setFont (bFont (13.0f, false));
        search.setTextToShowWhenEmpty ("Search Library...", kInk.withAlpha (0.6f));
        search.setColour (TextEditor::backgroundColourId, Colours::white.withAlpha (0.55f));
        search.setColour (TextEditor::outlineColourId, Colours::white.withAlpha (0.8f));
        search.setColour (TextEditor::focusedOutlineColourId, Colours::white);
        search.setColour (TextEditor::textColourId, kInk);
        search.setIndents (34, 8);
        search.onTextChange = [this]
        {
            query = search.getText().trim();
            rebuild();
        };

        addAndMakeVisible (tree);
        tree.setRootItemVisible (false);
        tree.setIndentSize (14);
        tree.setColour (TreeView::backgroundColourId, Colours::transparentBlack);

        tree.onDelete = [this] { return deleteSelectedLibraryItem(); };
        tree.onRename = [this]
        {
            if (auto* it = dynamic_cast<LibItem*> (tree.getSelectedItem (0)))
                if (isDeletable (it->getFile())) { startRename (it->getFile(), false); return true; }
            return false;
        };

        addChildComponent (presetTree);
        presetTree.setRootItemVisible (false);
        presetTree.setIndentSize (14);
        presetTree.setColour (TreeView::backgroundColourId, Colours::transparentBlack);
        presetTree.onDelete = [this] { return deleteSelectedPreset(); };
        presetTree.onRename = [this]
        {
            if (auto* it = dynamic_cast<PresetItem*> (presetTree.getSelectedItem (0)))
                if (it->isUser()) { startRename (it->getFile(), true); return true; }
            return false;
        };

        addChildComponent (presetName);
        presetName.setFont (bFont (13.0f, false));
        presetName.setTextToShowWhenEmpty ("Preset name...", kInk.withAlpha (0.6f));
        presetName.setColour (TextEditor::backgroundColourId, Colours::white.withAlpha (0.55f));
        presetName.setColour (TextEditor::outlineColourId, Colours::white.withAlpha (0.8f));
        presetName.setColour (TextEditor::focusedOutlineColourId, Colours::white);
        presetName.setColour (TextEditor::textColourId, kInk);
        presetName.setIndents (10, 8);
        presetName.onReturnKey = [this] { saveCurrentPreset(); };

        addChildComponent (renameEd);
        renameEd.setFont (bFont (12.5f, false));
        renameEd.setColour (TextEditor::backgroundColourId, Colours::white);
        renameEd.setColour (TextEditor::outlineColourId, Colour (0xffa6dc1c));
        renameEd.setColour (TextEditor::focusedOutlineColourId, Colour (0xffa6dc1c));
        renameEd.setColour (TextEditor::textColourId, kInk);
        renameEd.setIndents (6, 4);
        renameEd.onReturnKey = [this] { finishRename (true); };
        renameEd.onEscapeKey = [this] { finishRename (false); };
        renameEd.onFocusLost = [this] { finishRename (true); };

        addChildComponent (saveBtn);
        saveBtn.getProperties().set ("primary", true);
        saveBtn.onClick = [this] { saveCurrentPreset(); };

        addAndMakeVisible (msg);
        msg.setFont (bFont (10.5f));
        msg.setColour (Label::textColourId, kInk);

        addChildComponent (bar);
        bar.setPercentageDisplay (true);
        bar.setColour (ProgressBar::foregroundColourId, Colour (0xff8fc417));
        bar.setColour (ProgressBar::backgroundColourId, Colours::white.withAlpha (0.55f));

        addAndMakeVisible (importBtn);
        importBtn.getProperties().set ("primary", true);
        importBtn.onClick = [this] { startImport(); };

        rebuild();
    }

    ~LibraryPanel() override { stopTimer(); job.reset(); tree.setRootItem (nullptr); presetTree.setRootItem (nullptr); }

    void paint (Graphics& g) override
    {
        drawGlass (g, getLocalBounds().toFloat(), 20.0f);
        drawLogo (g, { 22.0f, 20.0f, 54.0f, 54.0f });
        g.setColour (kInk);
        g.setFont (bFont (22.0f));
        g.drawText ("B Trigger", 82, 22, 120, 28, Justification::centredLeft, false);
        g.setFont (bFont (11.0f));
        g.drawText ("Drum Trigger Studio", 82, 48, 120, 16, Justification::centredLeft, false);

        if (search.isVisible())
        {
            const float cx = 8.0f + 18.0f, cy = 127.0f + 16.0f;
            g.setColour (kInk);
            g.drawEllipse (cx - 5.0f, cy - 5.5f, 10.0f, 10.0f, 1.8f);
            g.drawLine (cx + 3.0f, cy + 2.5f, cx + 7.0f, cy + 6.5f, 1.8f);
        }
    }

    void resized() override
    {
        tabLibrary.setBounds (8, 84, 94, 34);
        tabPresets.setBounds (106, 84, 94, 34);
        search.setBounds (8, 127, 192, 34);
        tree.setBounds (8, 168, 192, 276);
        presetTree.setBounds (8, 127, 192, 317);
        msg.setBounds (12, 448, 184, 14);
        importBtn.setBounds (8, 466, 192, 34);
        bar.setBounds (8, 466, 192, 34);
        presetName.setBounds (8, 466, 118, 34);
        saveBtn.setBounds (130, 466, 70, 34);
    }

    bool isInterestedInFileDrag (const StringArray& files) override { return ! files.isEmpty(); }
    void filesDropped (const StringArray& files, int, int) override { importFiles (files, myLibrary()); }

    // ---- called by tree items
    bool fileVisible (const File& f) const
    {
        if (! f.hasFileExtension (audioExts))
            return false;
        return query.isEmpty() || f.getFileName().containsIgnoreCase (query);
    }

    bool dirVisible (const File& d) const
    {
        if (query.isEmpty())
            return true;
        for (const auto& entry : RangedDirectoryIterator (d, true, "*", File::findFiles))
            if (fileVisible (entry.getFile()))
                return true;
        return false;
    }

    void fileClicked (const File& f) { proc.previewFile (f); }

    void fileDoubleClicked (const File& f)
    {
        int target = -1;
        for (int l = 0; l < BTriggerProcessor::kLayers; ++l)
            if (proc.isLayerActive (l) && ! proc.layerHasSample (l))
            {
                target = l;
                break;
            }
        if (target < 0)
            target = proc.addLayer();
        if (target < 0)
            target = 0;
        proc.loadLayerFile (target, f);
    }

    void rebuild()
    {
        std::unique_ptr<XmlElement> state (tree.getOpennessState (false));
        tree.setRootItem (nullptr);
        root = std::make_unique<LibItem> (*this, BTriggerProcessor::libraryRoot());
        tree.setRootItem (root.get());
        root->setOpen (true);

        if (query.isNotEmpty())
            openAll (root.get());
        else if (state != nullptr)
            tree.restoreOpennessState (*state, false);
        else
            openDefaults();
    }

private:
    static constexpr const char* audioExts = "wav;wave;mp3;flac;ogg;aif;aiff;m4a";

    class LibItem : public TreeViewItem
    {
    public:
        LibItem (LibraryPanel& o, const File& f) : owner (o), file (f), isDir (f.isDirectory()) {}

        bool mightContainSubItems() override { return isDir; }
        String getUniqueName() const override { return file.getFullPathName(); }
        int getItemHeight() const override { return 28; }
        var getDragSourceDescription() override { return isDir ? var() : var ("BT:" + file.getFullPathName()); }

        void itemOpennessChanged (bool isNowOpen) override
        {
            if (isNowOpen)
                populate();
            else
                clearSubItems();
        }

        void paintItem (Graphics& g, int w, int h) override
        {
            if (isSelected())
            {
                g.setColour (isDir ? Colours::white.withAlpha (0.35f) : Colour (0xffc4f230));
                g.fillRoundedRectangle (1.0f, 1.0f, (float) w - 2.0f, (float) h - 2.0f, 8.0f);
            }

            if (isDir)
            {
                g.setColour (Colour (0xffa6dc1c));
                g.fillRoundedRectangle (3.0f, (float) h * 0.5f - 6.0f, 17.0f, 13.0f, 3.0f);
                g.fillRoundedRectangle (3.0f, (float) h * 0.5f - 9.0f, 8.0f, 5.0f, 2.0f);
            }
            else
            {
                g.setColour (kInk);
                static const float hs[6] = { 3.0f, 8.0f, 13.0f, 9.0f, 6.0f, 2.0f };
                for (int i = 0; i < 6; ++i)
                    g.fillRoundedRectangle (4.0f + (float) i * 3.0f, (float) h * 0.5f - hs[i] * 0.5f, 1.8f, hs[i], 0.9f);
            }

            g.setColour (kInk);
            g.setFont (bFont (isDir ? 13.0f : 12.5f, isDir));
            g.drawText (file.getFileName(), 26, 0, w - 28, h, Justification::centredLeft, true);
        }

        void itemClicked (const MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                owner.showItemMenu (file);
                return;
            }

            if (isDir)
                setOpen (! isOpen());
            else
                owner.fileClicked (file);
        }

        // ---- drops: OS files and folders, and samples dragged from the tree or a layer
        bool isInterestedInFileDrag (const StringArray&) override { return true; }

        void filesDropped (const StringArray& files, int) override
        {
            const File dest = owner.dropTargetFor (file);
            owner.deferred ([o = &owner, files, dest] { o->importFiles (files, dest); });
        }

        void itemDoubleClicked (const MouseEvent& e) override
        {
            if (isDir)
                TreeViewItem::itemDoubleClicked (e);
            else
                owner.fileDoubleClicked (file);
        }

        File getFile() const { return file; }

    private:
        void populate()
        {
            clearSubItems();
            Array<File> dirs, files;
            file.findChildFiles (dirs, File::findDirectories | File::ignoreHiddenFiles, false);
            file.findChildFiles (files, File::findFiles | File::ignoreHiddenFiles, false);
            auto byName = [] (const File& a, const File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; };
            std::sort (dirs.begin(), dirs.end(), byName);
            std::sort (files.begin(), files.end(), byName);

            for (const auto& d : dirs)
                if (owner.dirVisible (d) && d != BTriggerProcessor::userPresetDir())   // presets live in the Presets tab
                    addSubItem (new LibItem (owner, d));
            for (const auto& f : files)
                if (owner.fileVisible (f))
                    addSubItem (new LibItem (owner, f));
        }

        LibraryPanel& owner;
        File file;
        bool isDir;
    };

    void openAll (TreeViewItem* item)
    {
        for (int i = 0; i < item->getNumSubItems(); ++i)
        {
            auto* sub = item->getSubItem (i);
            if (sub->mightContainSubItems())
            {
                sub->setOpen (true);
                openAll (sub);
            }
        }
    }

    void openDefaults()
    {
        for (int i = 0; i < root->getNumSubItems(); ++i)
            if (auto* item = dynamic_cast<LibItem*> (root->getSubItem (i)))
                if (item->getFile().getFileName() == "B Trigger Factory")
                {
                    item->setOpen (true);
                    for (int j = 0; j < item->getNumSubItems(); ++j)
                        if (auto* kid = dynamic_cast<LibItem*> (item->getSubItem (j)))
                            if (kid->getFile().getFileName() == "Kicks")
                                kid->setOpen (true);
                }
    }

    void showTab (bool library)
    {
        tree.setVisible (library);
        search.setVisible (library);
        importBtn.setVisible (library && job == nullptr);
        bar.setVisible (library && job != nullptr);
        presetTree.setVisible (! library);
        presetName.setVisible (! library);
        saveBtn.setVisible (! library);
        if (! library)
            rebuildPresets();
        repaint();
    }

    void say (const String& text)
    {
        msg.setText (text, dontSendNotification);
        SafePointer<LibraryPanel> sp (this);
        Timer::callAfterDelay (5000, [sp]
                               {
                                   if (auto* p = sp.getComponent())
                                       p->msg.setText ({}, dontSendNotification);
                               });
    }

    // ---- library locations
    static File myLibrary() { return BTriggerProcessor::libraryRoot().getChildFile ("My Library"); }

    // Files you drop always go into My Library (or a folder inside it). The built-in sounds stay untouched.
    File dropTargetFor (const File& item) const
    {
        const File dir = item.isDirectory() ? item : item.getParentDirectory();
        return (dir == myLibrary() || dir.isAChildOf (myLibrary())) && dir.hasWriteAccess() ? dir : myLibrary();
    }

    void deferred (std::function<void()> fn)
    {
        SafePointer<LibraryPanel> sp (this);
        MessageManager::callAsync ([sp, fn = std::move (fn)]
                                   {
                                       if (sp.getComponent() != nullptr)
                                           fn();
                                   });
    }

    // Everything in My Library and User Samples can be deleted. The built-in Factory folder never can.
    static File userSamples() { return BTriggerProcessor::libraryRoot().getChildFile ("User Samples"); }

    static bool isDeletable (const File& f)
    {
        return f.isAChildOf (myLibrary()) || f.isAChildOf (userSamples());
    }

    void deleteEntry (const File& f)
    {
        if (! isDeletable (f))
            return;
        const String name = f.getFileName();
        if (f.isDirectory())
            f.deleteRecursively (true);
        else
            f.moveToTrash() || f.deleteFile();
        rebuild();
        say ("Deleted " + name);
    }

    void askDelete (const File& f)
    {
        if (! isDeletable (f))
            say ("Factory sounds can't be deleted");
        else
            deleteEntry (f);
    }

    bool deleteSelectedLibraryItem()
    {
        if (auto* it = dynamic_cast<LibItem*> (tree.getSelectedItem (0)))
        {
            askDelete (it->getFile());
            return true;
        }
        return false;
    }

    // right-click menu
    void showItemMenu (const File& file)
    {
        PopupMenu m;
        const bool ok = isDeletable (file);
        m.addItem (3, "Rename", ok);
        m.addItem (1, file.isDirectory() ? "Delete folder" : "Delete", ok);
        if (! ok)
            m.addItem (2, "Factory sounds can't be renamed or deleted", false);
        SafePointer<LibraryPanel> sp (this);
        m.showMenuAsync (PopupMenu::Options(), [sp, file] (int r)
                         {
                             if (auto* self = sp.getComponent())
                             {
                                 if (r == 1) self->askDelete (file);
                                 if (r == 3) self->startRename (file, false);
                             }
                         });
    }

    // ---- rename: a small text box laid over the row
    void startRename (const File& f, bool preset)
    {
        auto& t = preset ? presetTree : tree;
        TreeViewItem* found = nullptr;
        const String key = preset ? "pf:" + f.getFullPathName() : f.getFullPathName();
        for (int r = 0; r < t.getNumRowsInTree(); ++r)
            if (auto* it = t.getItemOnRow (r))
                if (it->getUniqueName() == key) { found = it; break; }
        if (found == nullptr)
            return;

        renaming = f;
        renamingPreset = preset;
        auto rr = found->getItemPosition (true);
        renameEd.setBounds (t.getX() + rr.getX() + 24, t.getY() + rr.getY() + 2, juce::jmax (80, t.getWidth() - rr.getX() - 30), rr.getHeight() - 4);
        renameEd.setText (f.isDirectory() || preset ? (preset && ! f.isDirectory() ? f.getFileNameWithoutExtension() : f.getFileName())
                                                    : f.getFileNameWithoutExtension(), dontSendNotification);
        renameEd.setVisible (true);
        renameEd.toFront (true);
        renameEd.grabKeyboardFocus();
        renameEd.selectAll();
    }

    void finishRename (bool commit)
    {
        if (! renameEd.isVisible())
            return;
        const auto text = renameEd.getText();
        renameEd.setVisible (false);
        if (! commit || renaming == File())
            return;

        const auto base = File::createLegalFileName (text.trim()).substring (0, 64).trim();
        const auto f = renaming;
        renaming = File();
        if (base.isEmpty())
            return;
        const String ext = f.isDirectory() ? String() : f.getFileExtension();
        const auto target = f.getParentDirectory().getChildFile (base + ext);
        if (target == f)
            return;
        if (target.exists())
        {
            say ("That name is already used");
            return;
        }
        if (! f.moveFileTo (target))
        {
            say ("Could not rename");
            return;
        }
        if (renamingPreset)
        {
            if (proc.currentPreset == f.getFileNameWithoutExtension())
                proc.currentPreset = base;
            rebuildPresets();
        }
        else
            rebuild();
        say ("Renamed to " + base);
    }

    // ---- importing
    void startImport()
    {
        chooser = std::make_unique<FileChooser> ("Import audio into My Library", File(),
                                                 "*.wav;*.wave;*.mp3;*.flac;*.ogg;*.aif;*.aiff;*.m4a;*.tci");
        SafePointer<LibraryPanel> sp (this);
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles
                                  | FileBrowserComponent::canSelectMultipleItems,
                              [sp] (const FileChooser& fc)
                              {
                                  if (auto* self = sp.getComponent())
                                  {
                                      StringArray paths;
                                      for (const auto& f : fc.getResults())
                                          paths.add (f.getFullPathName());
                                      self->importFiles (paths, myLibrary());
                                  }
                              });
    }

    void importFiles (const StringArray& paths, const File& destIn)
    {
        const File dest = dropTargetFor (destIn);
        dest.createDirectory();

        int added = 0;
        String problem;
        Array<File> tcis;
        for (const auto& p : paths)
        {
            const File f (p);
            if (f.isDirectory())
            {
                problem = "Drop audio files, not folders";
                continue;
            }
            if (! f.existsAsFile())
                continue;
            if (f.hasFileExtension ("tci"))
            {
                tcis.add (f);
                continue;
            }
            if (! f.hasFileExtension (audioExts))
            {
                problem = "Unsupported file: " + f.getFileName();
                continue;
            }
            const auto target = dest.getNonexistentChildFile (f.getFileNameWithoutExtension(), f.getFileExtension(), false);
            if (f.copyFileTo (target))
                ++added;
        }

        if (! tcis.isEmpty())
            startTciConversion (tcis);

        if (added > 0)
        {
            search.clear();
            query = {};
            rebuild();
            for (int i = 0; i < root->getNumSubItems(); ++i)
                if (auto* item = dynamic_cast<LibItem*> (root->getSubItem (i)))
                    if (item->getFile().getFileName() == "My Library")
                        item->setOpen (true);
            say (String (added) + (added == 1 ? " file" : " files") + " added to My Library");
        }
        else if (problem.isNotEmpty())
            say (problem);
    }

    void setParam (const String& id, float value)
    {
        if (auto* prm = proc.apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    }

    // ---- presets: a tree of factory kits and your own saved presets
    class PresetItem : public TreeViewItem
    {
    public:
        enum Kind { FactoryRoot, Kit, Factory, UserRoot, UserDir, UserFile };

        PresetItem (LibraryPanel& o, Kind k, const String& l, int idx = -1, const File& f = {})
            : owner (o), kind (k), label (l), index (idx), file (f) {}

        bool isGroup() const { return kind != Factory && kind != UserFile; }
        bool mightContainSubItems() override { return isGroup(); }
        String getUniqueName() const override
        {
            switch (kind)
            {
                case FactoryRoot: return "pf";
                case Kit:         return "pk:" + label;
                case Factory:     return "pp:" + String (index);
                case UserRoot:    return "pu";
                default:          return "pf:" + file.getFullPathName();
            }
        }
        int getItemHeight() const override { return 28; }
        File getFile() const { return file; }
        Kind getKind() const { return kind; }
        bool isUser() const { return kind == UserDir || kind == UserFile; }

        void itemOpennessChanged (bool isNowOpen) override
        {
            if (isNowOpen)
                populate();
            else
                clearSubItems();
        }

        void paintItem (Graphics& g, int w, int h) override
        {
            if (isSelected())
            {
                g.setColour (isGroup() ? Colours::white.withAlpha (0.35f) : Colour (0xffc4f230));
                g.fillRoundedRectangle (1.0f, 1.0f, (float) w - 2.0f, (float) h - 2.0f, 8.0f);
            }
            if (isGroup())
            {
                g.setColour (Colour (0xffa6dc1c));
                g.fillRoundedRectangle (3.0f, (float) h * 0.5f - 6.0f, 17.0f, 13.0f, 3.0f);
                g.fillRoundedRectangle (3.0f, (float) h * 0.5f - 9.0f, 8.0f, 5.0f, 2.0f);
            }
            else
            {
                g.setColour (kInk);
                g.fillEllipse (6.0f, (float) h * 0.5f - 3.0f, 6.0f, 6.0f);
            }
            g.setColour (kInk);
            g.setFont (bFont (isGroup() ? 13.0f : 12.5f, isGroup()));
            const bool current = ! isGroup() && owner.proc.currentPreset == label;
            g.drawText (label + (current ? "  *" : ""), 26, 0, w - 28, h, Justification::centredLeft, true);
        }

        void itemClicked (const MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                owner.showPresetMenu (*this);
                return;
            }
            if (isGroup())
                setOpen (! isOpen());
            else
                owner.loadPreset (*this);
        }

        void itemDoubleClicked (const MouseEvent& e) override
        {
            if (isGroup())
                TreeViewItem::itemDoubleClicked (e);
        }

    private:
        void populate()
        {
            clearSubItems();
            const auto& all = BTPresets::factory();
            if (kind == FactoryRoot)
            {
                StringArray seen;
                for (const auto& p : all)
                    if (! seen.contains (p.kit))
                    {
                        seen.add (p.kit);
                        addSubItem (new PresetItem (owner, Kit, p.kit));
                    }
            }
            else if (kind == Kit)
            {
                for (int i = 0; i < (int) all.size(); ++i)
                    if (all[(size_t) i].kit == label)
                        addSubItem (new PresetItem (owner, Factory, all[(size_t) i].name, i));
            }
            else if (kind == UserRoot || kind == UserDir)
            {
                Array<File> dirs, files;
                file.findChildFiles (dirs, File::findDirectories | File::ignoreHiddenFiles, false);
                file.findChildFiles (files, File::findFiles | File::ignoreHiddenFiles, false, "*.btp");
                auto byName = [] (const File& a, const File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; };
                std::sort (dirs.begin(), dirs.end(), byName);
                std::sort (files.begin(), files.end(), byName);
                for (const auto& d : dirs)
                    addSubItem (new PresetItem (owner, UserDir, d.getFileName(), -1, d));
                for (const auto& f : files)
                    addSubItem (new PresetItem (owner, UserFile, f.getFileNameWithoutExtension(), -1, f));
            }
        }

        LibraryPanel& owner;
        Kind kind;
        String label;
        int index;
        File file;
    };

    void rebuildPresets()
    {
        std::unique_ptr<XmlElement> state (presetTree.getOpennessState (false));
        presetTree.setRootItem (nullptr);
        BTriggerProcessor::userPresetDir().createDirectory();
        presetRoot.reset();
        presetRoot = std::make_unique<RootItem> (*this);
        presetTree.setRootItem (presetRoot.get());
        presetRoot->setOpen (true);
        if (state != nullptr)
            presetTree.restoreOpennessState (*state, false);
        else if (presetRoot->getNumSubItems() > 0)
            presetRoot->getSubItem (0)->setOpen (true);
    }

    // invisible root: "Factory Presets" and "User Presets"
    class RootItem : public PresetItem
    {
    public:
        explicit RootItem (LibraryPanel& o) : PresetItem (o, FactoryRoot, "root"), own (o)
        {
            addSubItem (new PresetItem (o, FactoryRoot, "Factory Presets"));
            addSubItem (new PresetItem (o, UserRoot, "User Presets", -1, BTriggerProcessor::userPresetDir()));
        }
        String getUniqueName() const override { return "root"; }
        void itemOpennessChanged (bool) override {}
    private:
        LibraryPanel& own;
    };

    void loadPreset (const PresetItem& it)
    {
        if (it.getKind() == PresetItem::Factory)
            proc.applyFactoryPreset (it.getUniqueName().fromFirstOccurrenceOf (":", false, false).getIntValue());
        else if (it.getKind() == PresetItem::UserFile)
        {
            if (! proc.loadUserPreset (it.getFile()))
            {
                say ("Could not load preset");
                return;
            }
        }
        presetTree.repaint();
        say (proc.currentPreset + " loaded");
    }

    // the folder a new preset is saved into: the selected user folder, or User Presets
    File saveFolder()
    {
        if (auto* it = dynamic_cast<PresetItem*> (presetTree.getSelectedItem (0)))
        {
            if (it->getKind() == PresetItem::UserDir || it->getKind() == PresetItem::UserRoot)
                return it->getFile();
            if (it->getKind() == PresetItem::UserFile)
                return it->getFile().getParentDirectory();
        }
        return BTriggerProcessor::userPresetDir();
    }

    void saveCurrentPreset()
    {
        const auto name = presetName.getText();
        if (! proc.saveUserPreset (name, saveFolder()))
        {
            say ("Type a name for the preset first");
            return;
        }
        presetName.clear();
        rebuildPresets();
        say ("Saved " + proc.currentPreset);
    }

    void deletePreset (const File& f)
    {
        if (f.isDirectory())
            f.deleteRecursively (true);
        else
            f.deleteFile();
        rebuildPresets();
        say ("Deleted " + f.getFileNameWithoutExtension());
    }

    bool deleteSelectedPreset()
    {
        if (auto* it = dynamic_cast<PresetItem*> (presetTree.getSelectedItem (0)))
            if (it->isUser())
            {
                askDeletePreset (it->getFile());
                return true;
            }
        return false;
    }

    void askDeletePreset (const File& f) { deletePreset (f); }

    void showPresetMenu (const PresetItem& it)
    {
        PopupMenu m;
        const bool user = it.isUser();
        if (user)
        {
            m.addItem (3, "Rename");
            m.addItem (1, it.getKind() == PresetItem::UserDir ? "Delete folder" : "Delete");
        }
        else
            m.addItem (2, "Factory presets can't be renamed or deleted", false);
        SafePointer<LibraryPanel> sp (this);
        const File f = it.getFile();
        m.showMenuAsync (PopupMenu::Options(), [sp, f] (int r)
                         {
                             if (auto* self = sp.getComponent())
                             {
                                 if (r == 1) self->askDeletePreset (f);
                                 if (r == 3) self->startRename (f, true);
                             }
                         });
    }

    struct DeleteTree : public TreeView
    {
        std::function<bool()> onDelete, onRename;
        bool keyPressed (const KeyPress& k) override
        {
            if (k == KeyPress::F2Key && onRename && onRename())
                return true;
            if ((k == KeyPress::deleteKey || k == KeyPress::backspaceKey) && onDelete && onDelete())
                return true;
            return TreeView::keyPressed (k);
        }
    };

    BTriggerProcessor& proc;
    String query;
    TextButton tabLibrary { "LIBRARY" }, tabPresets { "PRESETS" };
    TextEditor search;
    DeleteTree tree, presetTree;
    TextEditor presetName;
    TextButton saveBtn { "SAVE" };
    std::unique_ptr<PresetItem> presetRoot;
    // ---- .tci conversion: runs on its own thread, shows a progress bar, drops the WAVs into My Library
    class TciJob : public Thread
    {
    public:
        TciJob (Array<File> f, File dest) : Thread ("TCI convert"), files (std::move (f)), destRoot (std::move (dest)) {}
        ~TciJob() override { stopThread (5000); }

        std::atomic<double> progress { 0.0 };
        std::atomic<bool> finished { false };
        String status() const { const ScopedLock sl (lock); return statusText; }
        String report() const { const ScopedLock sl (lock); return reportText; }
        File lastFolder;

        void run() override
        {
            int ok = 0, written = 0;
            String errors;
            const int n = files.size();
            for (int i = 0; i < n && ! threadShouldExit(); ++i)
            {
                const File f = files[i];
                const File out = destRoot.getNonexistentChildFile (f.getFileNameWithoutExtension(), {}, false);
                auto res = TCI::convertFile (f, out, [this, i, n, f] (double p, const String& t)
                {
                    progress = ((double) i + p) / (double) n;
                    setStatus ((n > 1 ? String (i + 1) + "/" + String (n) + "  " : String()) + t);
                    return ! threadShouldExit();
                });
                if (res.ok) { ++ok; written += res.written; lastFolder = out; }
                else
                {
                    out.deleteRecursively();
                    errors << f.getFileName() << ": " << res.error << "  ";
                }
            }
            const ScopedLock sl (lock);
            reportText = ok > 0 ? "Converted " + String (written) + " WAVs from " + String (ok) + (ok == 1 ? " TCI" : " TCIs") + (errors.isNotEmpty() ? ". " + errors : String())
                                : (errors.isNotEmpty() ? errors : String ("Conversion stopped"));
            progress = 1.0;
            finished = true;
        }

    private:
        void setStatus (const String& s) { const ScopedLock sl (lock); statusText = s; }
        Array<File> files;
        File destRoot;
        CriticalSection lock;
        String statusText, reportText;
    };

    void startTciConversion (const Array<File>& tcis)
    {
        if (job != nullptr)
        {
            say ("Still converting. Wait for it to finish.");
            return;
        }
        myLibrary().createDirectory();
        job = std::make_unique<TciJob> (tcis, myLibrary());
        progressValue = 0.0;
        job->startThread();
        updateBusy();
        startTimerHz (15);
    }

    void timerCallback() override
    {
        if (job == nullptr)
        {
            stopTimer();
            return;
        }
        progressValue = job->progress.load();
        msg.setText (job->status(), dontSendNotification);
        if (job->finished.load())
        {
            stopTimer();
            const auto report = job->report();
            const File folder = job->lastFolder;
            job.reset();
            updateBusy();
            search.clear();
            query = {};
            rebuild();
            for (int i = 0; i < root->getNumSubItems(); ++i)
                if (auto* item = dynamic_cast<LibItem*> (root->getSubItem (i)))
                    if (item->getFile().getFileName() == "My Library")
                    {
                        item->setOpen (true);
                        if (folder != File())
                            for (int j = 0; j < item->getNumSubItems(); ++j)
                                if (auto* kid = dynamic_cast<LibItem*> (item->getSubItem (j)))
                                    if (kid->getFile() == folder)
                                        kid->setOpen (true);
                    }
            say (report);
        }
    }

    void updateBusy()
    {
        const bool busy = job != nullptr;
        const bool lib = tabLibrary.getToggleState();
        bar.setVisible (busy && lib);
        importBtn.setVisible (! busy && lib);
    }

    std::unique_ptr<TciJob> job;
    double progressValue = 0.0;
    ProgressBar bar { progressValue };

    TextEditor renameEd;
    File renaming;
    bool renamingPreset = false;
    Label msg;
    TextButton importBtn { String::fromUTF8 ("\xef\xbc\x8b IMPORT TO MY LIBRARY") };
    std::unique_ptr<LibItem> root;
    std::unique_ptr<FileChooser> chooser;
};

} // namespace BTUI
