#include "Widgets.h"

namespace ui
{
namespace
{
constexpr float kStartAngle = -juce::MathConstants<float>::pi * 0.75f; // -135 degrees
constexpr float kAngleRange = juce::MathConstants<float>::pi * 1.5f;   // 270 degrees

juce::RangedAudioParameter& getParam (APVTS& state, const juce::String& id)
{
    auto* p = state.getParameter (id);
    jassert (p != nullptr);
    return *p;
}

float defaultPlain (const juce::RangedAudioParameter& p) { return p.convertFrom0to1 (p.getDefaultValue()); }

void setupDragSlider (juce::Slider& s, juce::Slider::SliderStyle style)
{
    s.setSliderStyle (style);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setMouseDragSensitivity (220);
    s.setPopupDisplayEnabled (false, false, nullptr);
}
} // namespace

void drawTriangle (juce::Graphics& g, juce::Rectangle<float> r, int direction)
{
    juce::Path p;
    const auto c = r.getCentre();
    const float s = std::min (r.getWidth(), r.getHeight()) * 0.5f;
    if (direction == -1)     p.addTriangle (c.x - s * 0.8f, c.y, c.x + s * 0.6f, c.y - s, c.x + s * 0.6f, c.y + s);
    else if (direction == 1) p.addTriangle (c.x + s * 0.8f, c.y, c.x - s * 0.6f, c.y - s, c.x - s * 0.6f, c.y + s);
    else                     p.addTriangle (c.x - s, c.y - s * 0.5f, c.x + s, c.y - s * 0.5f, c.x, c.y + s * 0.6f);
    g.fillPath (p);
}

// ---------------------------------------------------------------- Knob

Knob::Knob (APVTS& state, const juce::String& paramID, const Scheme& s, juce::String l, float d, LabelLayout lay)
    : param (getParam (state, paramID)), scheme (s), label (std::move (l)), diameter (d), layout (lay),
      attachment (state, paramID, dial)
{
    setupDragSlider (dial, juce::Slider::RotaryHorizontalVerticalDrag);
    dial.setDoubleClickReturnValue (true, defaultPlain (param));
    dial.setTooltip (param.getName (64));
    dial.onValueChange = [this] { repaint(); };
    addAndMakeVisible (dial);
}

void Knob::setModulation (float lo, float hi, float live)
{
    if (juce::exactlyEqual (lo, modLo) && juce::exactlyEqual (hi, modHi) && juce::exactlyEqual (live, modLive))
        return;
    modLo = lo; modHi = hi; modLive = live;
    dial.repaint();
}

void Knob::resized()
{
    const int d = juce::roundToInt (diameter);
    if (layout == LabelLayout::right)
        dial.setBounds (0, (getHeight() - d) / 2, d, d);
    else
        dial.setBounds ((getWidth() - d) / 2, 0, d, d);
}

void Knob::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().withTrimmedTop (diameter + 3.0f);
    const auto value = valueText (param);
    const float size = diameter < 40.0f ? 10.0f : 11.0f;

    if (layout == LabelLayout::right)
    {
        auto r = getLocalBounds().toFloat().withTrimmedLeft (diameter + 10.0f).withSizeKeepingCentre (
            (float) getWidth() - diameter - 10.0f, 28.0f);
        g.setColour (scheme.text);
        g.setFont (fonts::label (11.0f, true, 1.0f));
        g.drawText (label, r.removeFromTop (13.0f), juce::Justification::centredLeft, false);
        g.setFont (fonts::label (13.0f, true));
        g.drawText (value, r, juce::Justification::centredLeft, false);
        return;
    }

    if (layout == LabelLayout::inline_)
    {
        // "LABEL value" on one line, the value a little lighter.
        auto labelFont = fonts::label (size, true, 1.0f);
        auto valueFont = fonts::label (size, true);
        juce::AttributedString s;
        s.append (label + " ", labelFont, scheme.text);
        s.append (value, valueFont, scheme.text.withAlpha (0.72f));
        s.setJustification (juce::Justification::centredTop);
        s.setWordWrap (juce::AttributedString::none);
        s.draw (g, area.withHeight (16.0f));
    }
    else
    {
        g.setColour (scheme.text);
        g.setFont (fonts::label (size, true, 1.0f));
        g.drawText (label, area.removeFromTop (13.0f), juce::Justification::centredTop, false);
        g.setColour (scheme.text.withAlpha (0.8f));
        g.setFont (fonts::label (size + 1.0f, true));
        g.drawText (value, area.removeFromTop (14.0f), juce::Justification::centredTop, false);
    }
}

void Knob::Dial::paint (juce::Graphics& g)
{
    const auto& sc = owner.scheme;
    const auto b = getLocalBounds().toFloat();
    const float d = std::min (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float ring = d * 7.0f / 64.0f;
    const float rr = d * 0.5f - ring * 0.5f;
    const float v = (float) valueToProportionOfLength (getValue());

    auto arc = [&] (float from, float to, juce::Colour col)
    {
        juce::Path p;
        p.addCentredArc (c.x, c.y, rr, rr, 0.0f, kStartAngle + kAngleRange * from, kStartAngle + kAngleRange * to, true);
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (ring, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
    };

    arc (0.0f, 1.0f, sc.track);
    if (owner.modLo >= 0.0f && owner.modHi > owner.modLo + 0.002f)
        arc (owner.modLo, owner.modHi, sc.accent);

    // Disc and wedge pointer.
    const auto disc = juce::Rectangle<float> (d, d).withCentre (c).reduced (ring);
    juce::Path discPath;
    discPath.addEllipse (disc);
    g.setColour (sc.knobDisc);
    g.fillPath (discPath);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (discPath);
        const float dd = disc.getWidth();
        juce::Path wedge;
        wedge.addTriangle (c.x, c.y + dd * 0.02f, c.x - dd * 0.11f, disc.getY() - 1.0f, c.x + dd * 0.11f, disc.getY() - 1.0f);
        g.setColour (sc.knobPointer);
        g.fillPath (wedge, juce::AffineTransform::rotation (kStartAngle + kAngleRange * v, c.x, c.y));
    }

    // Where the modulation has the knob right now.
    if (owner.modLive >= 0.0f)
    {
        const float a = kStartAngle + kAngleRange * owner.modLive;
        const auto p = c.getPointOnCircumference (rr, a);
        g.setColour (sc.text);
        g.fillEllipse (juce::Rectangle<float> (ring * 0.9f, ring * 0.9f).withCentre (p));
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (sc.accent);
        g.drawEllipse (b.reduced (0.5f), 1.0f);
    }
}

// ---------------------------------------------------------------- Fader

Fader::Fader (APVTS& state, const juce::String& paramID, const Scheme& s, juce::String l)
    : param (getParam (state, paramID)), scheme (s), label (std::move (l)), attachment (state, paramID, track)
{
    setupDragSlider (track, juce::Slider::LinearVertical);
    track.setDoubleClickReturnValue (true, defaultPlain (param));
    track.setTooltip (param.getName (64));
    track.onValueChange = [this] { repaint(); };
    addAndMakeVisible (track);
}

void Fader::setModulation (float lo, float hi, float live)
{
    if (juce::exactlyEqual (lo, modLo) && juce::exactlyEqual (hi, modHi) && juce::exactlyEqual (live, modLive))
        return;
    modLo = lo; modHi = hi; modLive = live;
    track.repaint();
}

void Fader::resized()
{
    track.setBounds (getLocalBounds().withTrimmedTop (16).withTrimmedBottom (22));
}

void Fader::paint (juce::Graphics& g)
{
    g.setColour (scheme.text);
    g.setFont (fonts::label (10.0f));
    auto text = param.getCurrentValueAsText().upToFirstOccurrenceOf (" ", false, false);
    if (text.equalsIgnoreCase ("off"))
        text = juce::String::fromUTF8 ("-\xe2\x88\x9e"); // -infinity
    g.drawText (text, getLocalBounds().removeFromTop (14), juce::Justification::centred, false);
    g.setFont (fonts::title (16.0f));
    g.drawText (label, getLocalBounds().removeFromBottom (20), juce::Justification::centred, false);
}

void Fader::Track::paint (juce::Graphics& g)
{
    const auto& sc = owner.scheme;
    auto b = getLocalBounds().toFloat();
    const float w = 26.0f;
    auto col = juce::Rectangle<float> (w, b.getHeight() - 8.0f).withCentre (b.getCentre());
    const float v = (float) valueToProportionOfLength (getValue());

    g.setColour (sc.text.withAlpha (0.14f));
    g.fillRect (col);
    g.setColour (colours::red);
    g.fillRect (col.withTop (col.getBottom() - col.getHeight() * v));

    if (owner.modLo >= 0.0f && owner.modHi > owner.modLo + 0.002f)
    {
        g.setColour (sc.text);
        const float y0 = col.getBottom() - col.getHeight() * owner.modHi;
        const float y1 = col.getBottom() - col.getHeight() * owner.modLo;
        g.fillRect (juce::Rectangle<float> (col.getX() - 7.0f, y0, 3.0f, y1 - y0));
    }
    if (owner.modLive >= 0.0f)
    {
        g.setColour (sc.text);
        const float y = col.getBottom() - col.getHeight() * owner.modLive;
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ col.getX() - 5.5f, y }));
    }

    const float capY = col.getBottom() - col.getHeight() * v;
    g.setColour (sc.text);
    g.fillRect (juce::Rectangle<float> (col.getX() - 4.0f, capY - 4.0f, w + 8.0f, 8.0f));
}

// ---------------------------------------------------------------- ValueBox

ValueBox::ValueBox (APVTS& state, const juce::String& paramID, juce::String n, juce::Colour ink)
    : param (getParam (state, paramID)), name (std::move (n)), colour (ink), attachment (state, paramID, *this)
{
    // A vertical-drag rotary style: the value moves with how far you drag,
    // like the knobs. (A linear slider maps the box's 24 pixels to the whole
    // range, so it could only reach 0% or 100%.)
    setupDragSlider (*this, juce::Slider::RotaryHorizontalVerticalDrag);
    setDoubleClickReturnValue (true, defaultPlain (param));
    setTooltip (param.getName (64) + " (drag up or down)");
    onValueChange = [this] { repaint(); };
}

void ValueBox::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (colour);
    g.drawRect (b, 2.0f);
    g.setFont (fonts::label (11.0f, true, 1.0f));
    g.drawText (name + " " + valueText (param), b, juce::Justification::centred, false);
}

// ---------------------------------------------------------------- Segments

Segments::Segments (juce::StringArray opts, Style s) : options (std::move (opts)), style (s) {}

void Segments::attachTo (juce::RangedAudioParameter& param)
{
    attachment = std::make_unique<juce::ParameterAttachment> (param, [this] (float v)
    {
        selected = juce::jlimit (0, options.size() - 1, juce::roundToInt (v));
        repaint();
    });
    onChange = [this] (int i) { attachment->setValueAsCompleteGesture ((float) i); };
    attachment->sendInitialUpdate();
}

void Segments::setSelected (int index)
{
    selected = index;
    repaint();
}

void Segments::paint (juce::Graphics& g)
{
    const float w = (float) getWidth() / (float) options.size();
    for (int i = 0; i < options.size(); ++i)
    {
        auto r = juce::Rectangle<float> (w * (float) i, 0.0f, w, (float) getHeight());
        const bool sel = i == selected;
        if (sel)
        {
            g.setColour (style.selectedFill);
            if (style.cutSelected)
                g.fillPath (cutShape (r, { 0.0f, 0.0f, 0.0f, 0.08f, 0.04f, 0.0f, 0.03f, 0.08f }));
            else
                g.fillRect (r);
        }
        else if (! style.outline.isTransparent())
        {
            g.setColour (style.outline);
            g.drawRect (r, 2.0f);
        }
        g.setColour (sel ? style.selectedText : style.text);
        g.setFont (style.titleFont ? fonts::title (style.fontSize) : fonts::label (style.fontSize, true, 1.0f));
        g.drawText (options[i], r, juce::Justification::centred, false);
    }
}

void Segments::mouseDown (const juce::MouseEvent& e)
{
    const int i = juce::jlimit (0, options.size() - 1, (int) ((float) e.x / (float) getWidth() * (float) options.size()));
    if (options.size() == 2 && i == selected && attachment != nullptr)
    {
        // Clicking the lit side of a two-way switch flips it, like a toggle.
        setSelected (1 - i);
        if (onChange) onChange (1 - i);
        return;
    }
    setSelected (i);
    if (onChange) onChange (i);
}

// ---------------------------------------------------------------- ChoiceMenuButton

ChoiceMenuButton::ChoiceMenuButton (APVTS& state, const juce::String& paramID, juce::String p,
                                    juce::Colour f, juce::Colour t, juce::Colour o)
    : param (dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramID))),
      prefix (std::move (p)), fill (f), text (t), outline (o),
      attachment (*state.getParameter (paramID), [this] (float v) { index = juce::roundToInt (v); repaint(); })
{
    jassert (param != nullptr);
    setTooltip (param->getName (64));
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

void ChoiceMenuButton::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    if (! fill.isTransparent())
    {
        g.setColour (fill);
        g.fillRect (b);
    }
    if (! outline.isTransparent())
    {
        g.setColour (outline);
        g.drawRect (b, 2.0f);
    }

    g.setColour (text);
    g.setFont (fonts::label (b.getHeight() < 20.0f ? 10.0f : 11.0f, true, 1.0f));
    auto area = b.reduced (7.0f, 0.0f);
    const auto value = (index < shortNames.size() ? shortNames[index] : param->choices[index]).toUpperCase();
    if (prefix.isNotEmpty())
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (g.getCurrentFont(), prefix, 0.0f, 0.0f);
        const float pw = ga.getBoundingBox (0, -1, true).getWidth();
        g.drawText (prefix, area.removeFromLeft (pw + 2.0f), juce::Justification::centredLeft, false);
        drawTriangle (g, area.removeFromLeft (10.0f).withSizeKeepingCentre (6.0f, 6.0f), -1);
        area.removeFromLeft (3.0f);
    }
    drawTriangle (g, area.removeFromRight (8.0f).withSizeKeepingCentre (7.0f, 7.0f), 2);
    g.drawText (value, area, juce::Justification::centredLeft, true);
}

void ChoiceMenuButton::mouseDown (const juce::MouseEvent&)
{
    juce::PopupMenu menu;
    for (int i = 0; i < param->choices.size(); ++i)
        menu.addItem (i + 1, param->choices[i], true, i == index);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safe = juce::Component::SafePointer<ChoiceMenuButton> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->attachment.setValueAsCompleteGesture ((float) (result - 1));
                        });
}

// ---------------------------------------------------------------- ToggleTag

ToggleTag::ToggleTag (APVTS& state, const juce::String& paramID, juce::Colour f, juce::Colour t, juce::Colour off)
    : param (getParam (state, paramID)), onFill (f), onText (t), offText (off),
      attachment (param, [this] (float v) { on = v > 0.5f; repaint(); })
{
    setTooltip (param.getName (64));
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

void ToggleTag::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    if (on)
    {
        g.setColour (onFill);
        g.fillPath (cutShape (b, { 0.0f, 0.1f, 0.0f, 0.0f, 0.05f, 0.0f, 0.04f, 0.1f }));
    }
    else
    {
        g.setColour (offText.withAlpha (0.5f));
        g.drawRect (b, 2.0f);
    }
    g.setColour (on ? onText : offText);
    g.setFont (fonts::label (12.0f, true, 2.0f));
    g.drawText (on ? "ON" : "OFF", b, juce::Justification::centred, false);
}

void ToggleTag::mouseDown (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

// ---------------------------------------------------------------- BypassButton

BypassButton::BypassButton (APVTS& state, const juce::String& paramID)
    : param (getParam (state, paramID)), attachment (param, [this] (float v) { on = v > 0.5f; repaint(); })
{
    setTooltip ("Bypass");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

void BypassButton::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float d = std::min (b.getWidth(), b.getHeight());
    auto circle = juce::Rectangle<float> (d, d).withCentre (b.getCentre());
    g.setColour (on ? colours::red : colours::ink);
    g.fillEllipse (circle);
    g.setColour (on ? colours::ink : colours::red);
    g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ circle.getCentreX(), circle.getY() + d * 0.36f }));
    g.setColour (colours::cream);
    g.setFont (fonts::label (10.0f, true, 1.0f));
    g.drawText ("BYPASS", circle.withTrimmedTop (d * 0.52f).withHeight (14.0f), juce::Justification::centred, false);
}

void BypassButton::mouseDown (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}
} // namespace ui
