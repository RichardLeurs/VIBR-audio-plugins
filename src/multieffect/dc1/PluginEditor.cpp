// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginEditor.h"

namespace
{
constexpr float kBaseW = 1240.0f;                          // layout width the strip is designed for
constexpr float kHeaderH = 104.0f, kStripY = 116.0f, kStripH = 96.0f, kInnerY = 232.0f, kMargin = 24.0f;
constexpr float kTileW = 128.0f, kTileGap = 10.0f, kAddW = 64.0f;
const juce::Colour kAccent (0xff18A88A), kBg1 (0xff191c22), kBg2 (0xff0e1013), kPanel (0xff21252d), kText (0xffe9ecf3), kMuted (0xff8f98aa);

juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
} // namespace

//==============================================================================
void DC1LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const auto c = b.getCentre();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (b.translated (1.0f, 3.0f).withSizeKeepingCentre (R * 2.0f, R * 2.0f).translated (0.0f, 0.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff454850), c.x - R * 0.6f, c.y - R * 0.8f, juce::Colour (0xff08090a), c.x + R * 0.6f, c.y + R * 0.9f, false));
    g.fillEllipse (b.withSizeKeepingCentre (R * 2.0f, R * 2.0f));
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawEllipse (b.withSizeKeepingCentre (R * 2.0f, R * 2.0f), 1.0f);
    const float a = startAngle + pos * (endAngle - startAngle);
    juce::Path p;
    p.addRoundedRectangle (-R * 0.06f, -R * 0.92f, R * 0.12f, R * 0.5f, R * 0.05f);
    g.setColour (juce::Colour (0xfff2f2ee));
    g.fillPath (p, juce::AffineTransform::rotation (a).translated (c));
}

//==============================================================================
DC1AudioProcessorEditor::DC1AudioProcessorEditor (DC1AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    const auto setup = [this] (Knob& k, const char* id)
    {
        k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        k.slider.setRotaryParameters (juce::degreesToRadians (-140.0f), juce::degreesToRadians (140.0f), true);
        k.slider.setMouseDragSensitivity (160);
        k.slider.setDoubleClickReturnValue (true, 0.0);
        k.slider.setScrollWheelEnabled (false);
        k.slider.setPopupDisplayEnabled (true, true, this);
        k.slider.setTextValueSuffix (" dB");
        addAndMakeVisible (k.slider);
        k.attachment = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter (id), k.slider, nullptr);
    };
    setup (inKnob, "inGain");
    setup (outKnob, "outGain");

    proc.addChangeListener (this);
    proc.releaseSignal.addChangeListener (this);

    constrainer.setSizeLimits (760, 420, 2200, 1500);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) kBaseW, 700);
    showInner();
}

DC1AudioProcessorEditor::~DC1AudioProcessorEditor()
{
    proc.removeChangeListener (this);
    proc.releaseSignal.removeChangeListener (this);
    dropInner();
    setLookAndFeel (nullptr);
}

void DC1AudioProcessorEditor::dropInner()
{
    if (inner != nullptr) removeChildComponent (inner.get());
    inner.reset();
    innerProc = nullptr;
}

void DC1AudioProcessorEditor::showInner()
{
    dropInner();
    if (auto* sub = proc.slotProcessor (proc.selectedSlot))
    {
        inner.reset (sub->createEditorIfNeeded());
        if (inner != nullptr)
        {
            inner->setConstrainer (nullptr);               // the pedal's own resize corner is not used here
            inner->setResizable (false, false);
            innerProc = sub;
            innerRatio = (float) inner->getHeight() / (float) juce::jmax (1, inner->getWidth());
            addAndMakeVisible (inner.get());
        }
    }
    fitHeight();
    resized();
    repaint();
}

void DC1AudioProcessorEditor::fitHeight()
{
    const float w = (float) getWidth() - 2.0f * kMargin;
    const float h = inner != nullptr ? w * innerRatio : 160.0f;
    const int target = (int) std::round (kInnerY + h + kMargin);
    if (std::abs (target - getHeight()) > 2) setSize (getWidth(), target);
}

void DC1AudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster* src)
{
    if (src == &proc.releaseSignal) { dropInner(); return; }       // the pedals are about to be replaced
    if (innerProc != proc.slotProcessor (proc.selectedSlot) || (inner == nullptr && proc.numSlots() > 0)) showInner();
    repaint();
}

//==============================================================================
juce::Rectangle<float> DC1AudioProcessorEditor::tileRect (int i) const
{
    const float s = (float) getWidth() / kBaseW;
    const float x = kMargin + (float) i * (kTileW + kTileGap);
    return juce::Rectangle<float> (x * s, kStripY * s, (i >= proc.numSlots() ? kAddW : kTileW) * s, kStripH * s);
}

int DC1AudioProcessorEditor::tileAt (juce::Point<float> pt) const
{
    for (int i = 0; i < juce::jmin (proc.numSlots() + 1, DC1AudioProcessor::kMaxSlots + 1); ++i)
        if (tileRect (i).contains (pt)) return i;
    return -1;
}

void DC1AudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / kBaseW;
    const float k = 56.0f * s;
    outKnob.slider.setBounds (juce::Rectangle<float> (k, k).withCentre ({ (float) getWidth() - kMargin * s - 46.0f * s, 46.0f * s }).toNearestInt());
    inKnob.slider.setBounds (juce::Rectangle<float> (k, k).withCentre ({ (float) getWidth() - kMargin * s - 156.0f * s, 46.0f * s }).toNearestInt());
    if (inner != nullptr)
    {
        const float availW = (float) getWidth() - 2.0f * kMargin * s, availH = (float) getHeight() - (kInnerY + kMargin) * s;
        const float w = juce::jmin (availW, availH / innerRatio), h = w * innerRatio;
        inner->setBounds (juce::Rectangle<float> (w, h).withCentre ({ (float) getWidth() * 0.5f, kInnerY * s + availH * 0.5f }).toNearestInt());
    }
}

void DC1AudioProcessorEditor::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / kBaseW;
    g.setGradientFill (juce::ColourGradient (kBg1, 0.0f, 0.0f, kBg2, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // header
    g.setColour (kAccent);
    g.setFont (font (64.0f * s, true));
    g.drawText ("DC-1", juce::Rectangle<float> (kMargin * s, 14.0f * s, 260.0f * s, 76.0f * s), juce::Justification::centredLeft);
    g.setColour (kMuted);
    g.setFont (font (15.0f * s, true));
    g.drawText ("MULTI EFFECT CHAIN", juce::Rectangle<float> ((kMargin + 196.0f) * s, 36.0f * s, 320.0f * s, 20.0f * s), juce::Justification::centredLeft);
    g.setFont (font (13.0f * s));
    g.drawText ("add, remove, reorder and bypass pedals", juce::Rectangle<float> ((kMargin + 196.0f) * s, 56.0f * s, 360.0f * s, 20.0f * s), juce::Justification::centredLeft);
    g.setFont (font (12.0f * s, true));
    g.drawText ("INPUT",  juce::Rectangle<float> (inKnob.slider.getX() - 8.0f, 72.0f * s + 14.0f * s, inKnob.slider.getWidth() + 16.0f, 16.0f * s), juce::Justification::centred);
    g.drawText ("OUTPUT", juce::Rectangle<float> (outKnob.slider.getX() - 8.0f, 72.0f * s + 14.0f * s, outKnob.slider.getWidth() + 16.0f, 16.0f * s), juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawLine (kMargin * s, (kHeaderH + 2.0f) * s, (float) getWidth() - kMargin * s, (kHeaderH + 2.0f) * s, 1.0f);

    // chain strip
    int nFx = 0;
    const auto* fx = dc1::effects (nFx);
    for (int i = 0; i < proc.numSlots(); ++i)
    {
        const auto r = tileRect (i);
        const auto& e = fx[proc.slotEffect (i)];
        const bool byp = proc.slotBypassed (i), sel = i == proc.selectedSlot;
        const juce::Colour col (e.colour);
        const auto drawR = (dragged && i == dragFrom) ? r.translated (0.0f, -4.0f * s) : r;

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (drawR.translated (0.0f, 4.0f * s), 14.0f * s);
        g.setColour (sel ? kPanel.brighter (0.12f) : kPanel);
        g.fillRoundedRectangle (drawR, 14.0f * s);
        g.setColour (col.withAlpha (byp ? 0.25f : 1.0f));
        g.fillRoundedRectangle (drawR.withHeight (7.0f * s), 3.5f * s);
        if (sel) { g.setColour (col.withAlpha (byp ? 0.45f : 1.0f)); g.drawRoundedRectangle (drawR.reduced (1.0f), 14.0f * s, 2.5f * s); }

        g.setColour (byp ? kMuted : col);
        g.setFont (font (26.0f * s, true));
        g.drawText (e.name, drawR.withTrimmedTop (22.0f * s).withHeight (34.0f * s), juce::Justification::centred);
        g.setColour (kMuted);
        g.setFont (font (11.5f * s));
        g.drawText (e.description, drawR.withTrimmedTop (56.0f * s).withHeight (18.0f * s).reduced (6.0f * s, 0.0f), juce::Justification::centred);

        // bypass LED (top left) and remove cross (top right)
        const auto led = juce::Rectangle<float> (14.0f * s, 14.0f * s).withCentre ({ drawR.getX() + 17.0f * s, drawR.getY() + 21.0f * s });
        g.setColour (juce::Colour (0xff40d070).withAlpha (byp ? 0.12f : 0.25f));
        if (! byp) g.fillEllipse (led.expanded (5.0f * s));
        g.setColour (byp ? juce::Colour (0xff1b3a25) : juce::Colour (0xff4cf080));
        g.fillEllipse (led);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawEllipse (led, 1.0f);
        const auto x = juce::Rectangle<float> (10.0f * s, 10.0f * s).withCentre ({ drawR.getRight() - 17.0f * s, drawR.getY() + 21.0f * s });
        g.setColour (kMuted);
        g.drawLine (x.getX(), x.getY(), x.getRight(), x.getBottom(), 1.8f);
        g.drawLine (x.getX(), x.getBottom(), x.getRight(), x.getY(), 1.8f);

        // signal arrow to the next tile
        if (i < proc.numSlots() - 1)
        {
            const float ax = drawR.getRight() + 1.0f * s, ay = drawR.getCentreY() + 4.0f * s;
            g.setColour (kMuted.withAlpha (0.7f));
            juce::Path a; a.addTriangle (ax, ay - 5.0f * s, ax, ay + 5.0f * s, ax + 8.0f * s, ay);
            g.fillPath (a);
        }
    }
    if (proc.numSlots() < DC1AudioProcessor::kMaxSlots)
    {
        const auto r = tileRect (proc.numSlots());
        g.setColour (kAccent.withAlpha (0.12f));
        g.fillRoundedRectangle (r, 14.0f * s);
        g.setColour (kAccent.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (1.0f), 14.0f * s, 1.8f * s);
        const auto c = r.getCentre();
        g.drawLine (c.x - 12.0f * s, c.y, c.x + 12.0f * s, c.y, 3.0f * s);
        g.drawLine (c.x, c.y - 12.0f * s, c.x, c.y + 12.0f * s, 3.0f * s);
    }

    if (inner == nullptr)
    {
        g.setColour (kMuted);
        g.setFont (font (18.0f * s));
        g.drawText (proc.numSlots() == 0 ? "The chain is empty. Click + to add a pedal." : "", juce::Rectangle<float> (0.0f, kInnerY * s, (float) getWidth(), 100.0f * s), juce::Justification::centred);
    }
}

//==============================================================================
void DC1AudioProcessorEditor::addMenu()
{
    int nFx = 0, nGr = 0;
    const auto* fx = dc1::effects (nFx);
    const auto* gr = dc1::groups (nGr);
    juce::PopupMenu menu;
    for (int gi = 0; gi < nGr; ++gi)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < nFx; ++i)
            if (fx[i].group == gi)
                sub.addItem (i + 1, juce::String (fx[i].name) + "  " + fx[i].description);
        menu.addSubMenu (gr[gi].name, sub);
    }
    const auto area = juce::Rectangle<int> (tileRect (proc.numSlots()).toNearestInt());
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (area)),
                        [this, safe = juce::Component::SafePointer<DC1AudioProcessorEditor> (this)] (int result)
                        {
                            if (safe == nullptr || result <= 0) return;
                            dropInner();
                            proc.addSlot (result - 1);
                            showInner();
                        });
}

void DC1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    dragFrom = -1; dragged = false;
    const int t = tileAt (e.position);
    if (t < 0) return;
    if (t >= proc.numSlots()) { if (proc.numSlots() < DC1AudioProcessor::kMaxSlots) addMenu(); return; }

    const float s = (float) getWidth() / kBaseW;
    const auto r = tileRect (t);
    if (e.position.x < r.getX() + 36.0f * s && e.position.y < r.getY() + 40.0f * s)       // bypass LED
    {
        proc.setSlotBypassed (t, ! proc.slotBypassed (t));
        repaint();
        return;
    }
    if (e.position.x > r.getRight() - 36.0f * s && e.position.y < r.getY() + 40.0f * s)    // remove
    {
        dropInner();
        proc.removeSlot (t);
        showInner();
        return;
    }
    dragFrom = t;
    if (proc.selectedSlot != t) { proc.selectedSlot = t; showInner(); }
    repaint();
}

void DC1AudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragFrom < 0) return;
    if (std::abs (e.getDistanceFromDragStartX()) > 8) dragged = true;
    if (! dragged) return;
    const int over = tileAt ({ e.position.x, tileRect (0).getCentreY() });
    if (over >= 0 && over < proc.numSlots() && over != dragFrom)
    {
        proc.moveSlot (dragFrom, over);
        dragFrom = over;
        repaint();
    }
}

void DC1AudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    dragFrom = -1; dragged = false;
    repaint();
}
