#pragma once

// The Cut Paper look: cream paper, black ink, one red, and panels cut out at
// slight angles. Every UI file takes its colours and fonts from here.

#include <juce_audio_processors/juce_audio_processors.h>

namespace ui
{
namespace colours
{
inline const juce::Colour cream { 0xffEFE6D2 };
inline const juce::Colour tan   { 0xffE3D5B6 }; // lighter panels on the cream
inline const juce::Colour ink   { 0xff161412 };
inline const juce::Colour red   { 0xffC2291D };
} // namespace colours

// A panel's colour scheme: its fill, the text on it, and the knob colours.
struct Scheme
{
    juce::Colour fill, text, knobDisc, knobPointer, track, accent;
};

namespace schemes
{
inline const Scheme onInk   { colours::ink,   colours::cream, colours::cream, colours::red,   colours::cream.withAlpha (0.22f), colours::red };
inline const Scheme onCream { colours::tan,   colours::ink,   colours::ink,   colours::cream, colours::ink.withAlpha (0.2f),    colours::red };
inline const Scheme onRed   { colours::red,   colours::cream, colours::ink,   colours::cream, colours::ink.withAlpha (0.3f),    colours::ink };
} // namespace schemes

namespace fonts
{
// Londrina Solid for titles, Archivo Narrow for labels. Sizes are in CSS
// pixels at the 1000 x 650 design size.
juce::Font title (float size, bool black = false);
juce::Font label (float size, bool bold = true, float letterSpacing = 0.0f);
} // namespace fonts

// A rectangle cut at slight angles. `inset` lists how far each corner moves
// in, as a fraction of the width or height, clockwise from the top left:
// {tlx, tly, trx, try, brx, bry, blx, bly}.
juce::Path cutShape (juce::Rectangle<float> r, std::array<float, 8> inset);

// Upper-cases a parameter's display text the way the design shows it.
juce::String displayText (juce::String text);

// A parameter's value as shown under its knob, with its unit.
juce::String valueText (const juce::RangedAudioParameter& p);
} // namespace ui
