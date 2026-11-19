#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Style.h"

namespace ui
{
using APVTS = juce::AudioProcessorValueTreeState;

// A panel cut at slight angles. Child controls are added to it directly.
class CutPanel : public juce::Component
{
public:
    CutPanel (const Scheme& s, std::array<float, 8> insetFractions) : scheme (s), inset (insetFractions)
    {
        setInterceptsMouseClicks (false, true);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (scheme.fill);
        g.fillPath (cutShape (getLocalBounds().toFloat(), inset));
    }
    const Scheme& scheme;

private:
    std::array<float, 8> inset;
};

// Draws a panel title in Londrina Solid.
class TitleLabel : public juce::Component
{
public:
    TitleLabel (juce::String t, juce::Colour c, float size) : text (std::move (t)), colour (c), fontSize (size)
    {
        setInterceptsMouseClicks (false, false);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (colour);
        g.setFont (fonts::title (fontSize));
        g.drawText (text, getLocalBounds(), juce::Justification::centredLeft, false);
    }

private:
    juce::String text;
    juce::Colour colour;
    float fontSize;
};

// A knob: ring track, red modulation range, disc with a wedge pointer, and
// its label and value underneath. Double-click returns it to its default.
class Knob : public juce::Component
{
public:
    enum class LabelLayout { inline_, twoLine, right };

    Knob (APVTS& state, const juce::String& paramID, const Scheme& scheme, juce::String label,
          float diameter, LabelLayout layout = LabelLayout::inline_);

    // Modulation display, all normalised 0..1. lo < 0 hides the range; live < 0 hides the dot.
    void setModulation (float lo, float hi, float live);

    juce::RangedAudioParameter& getParameter() const { return param; }
    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    struct Dial : juce::Slider
    {
        explicit Dial (Knob& k) : owner (k) {}
        void paint (juce::Graphics& g) override;
        Knob& owner;
    };

    juce::RangedAudioParameter& param;
    const Scheme& scheme;
    juce::String label;
    float diameter;
    LabelLayout layout;
    Dial dial { *this };
    APVTS::SliderAttachment attachment;
    float modLo = -1.0f, modHi = -1.0f, modLive = -1.0f;
};

// A vertical level fader with its dB value above and its name below.
class Fader : public juce::Component
{
public:
    Fader (APVTS& state, const juce::String& paramID, const Scheme& scheme, juce::String label);
    void setModulation (float lo, float hi, float live);
    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    struct Track : juce::Slider
    {
        explicit Track (Fader& f) : owner (f) {}
        void paint (juce::Graphics& g) override;
        Fader& owner;
    };

    juce::RangedAudioParameter& param;
    const Scheme& scheme;
    juce::String label;
    Track track { *this };
    APVTS::SliderAttachment attachment;
    float modLo = -1.0f, modHi = -1.0f, modLive = -1.0f;
};

// A small box showing "NAME value" that you drag up and down, like "DAMP 30%".
class ValueBox : public juce::Slider
{
public:
    ValueBox (APVTS& state, const juce::String& paramID, juce::String name, juce::Colour ink);
    void paint (juce::Graphics& g) override;

private:
    juce::RangedAudioParameter& param;
    juce::String name;
    juce::Colour colour;
    APVTS::SliderAttachment attachment;
};

// Side-by-side buttons where one is selected: for a choice parameter
// (Pre/Post, Germanium/Silicon) or for tabs that only change the view.
class Segments : public juce::Component
{
public:
    struct Style
    {
        juce::Colour selectedFill, selectedText, text, outline; // outline transparent = none
        bool titleFont = false;
        float fontSize = 12.0f;
        bool cutSelected = false;
    };

    Segments (juce::StringArray options, Style style);

    // Binds the selection to a choice or bool parameter.
    void attachTo (juce::RangedAudioParameter& param);

    void setSelected (int index);
    int getSelected() const { return selected; }
    std::function<void (int)> onChange;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    juce::StringArray options;
    Style style;
    int selected = 0;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// "PREFIX < VALUE v": opens a menu of a choice parameter's options.
class ChoiceMenuButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChoiceMenuButton (APVTS& state, const juce::String& paramID, juce::String prefix,
                      juce::Colour fill, juce::Colour text, juce::Colour outline = juce::Colours::transparentBlack);
    // Shorter names to show on the button; the menu keeps the full ones.
    void setShortNames (juce::StringArray names) { shortNames = std::move (names); repaint(); }
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    juce::AudioParameterChoice* param;
    juce::String prefix;
    juce::StringArray shortNames;
    juce::Colour fill, text, outline;
    int index = 0;
    juce::ParameterAttachment attachment;
};

// A one-word on/off tag for a bool parameter, like the Fuzz "ON".
class ToggleTag : public juce::Component, public juce::SettableTooltipClient
{
public:
    ToggleTag (APVTS& state, const juce::String& paramID, juce::Colour onFill, juce::Colour onText, juce::Colour offText);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& param;
    juce::Colour onFill, onText, offText;
    bool on = false;
    juce::ParameterAttachment attachment;
};

// The round Bypass button in the header.
class BypassButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    BypassButton (APVTS& state, const juce::String& paramID);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& param;
    bool on = false;
    juce::ParameterAttachment attachment;
};

// Draws a small triangle pointing left, right or down, centred in `r`.
void drawTriangle (juce::Graphics& g, juce::Rectangle<float> r, int direction /* -1 left, 1 right, 2 down */);
} // namespace ui
