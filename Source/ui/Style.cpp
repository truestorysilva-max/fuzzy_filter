#include "Style.h"
#include "BinaryData.h"

namespace ui
{
namespace
{
juce::Typeface::Ptr load (const char* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
}

struct Typefaces
{
    juce::Typeface::Ptr londrina = load (BinaryData::LondrinaSolidRegular_ttf, BinaryData::LondrinaSolidRegular_ttfSize);
    juce::Typeface::Ptr londrinaBlack = load (BinaryData::LondrinaSolidBlack_ttf, BinaryData::LondrinaSolidBlack_ttfSize);
    juce::Typeface::Ptr archivo = load (BinaryData::ArchivoNarrowMedium_ttf, BinaryData::ArchivoNarrowMedium_ttfSize);
    juce::Typeface::Ptr archivoBold = load (BinaryData::ArchivoNarrowBold_ttf, BinaryData::ArchivoNarrowBold_ttfSize);
};

const Typefaces& typefaces()
{
    static Typefaces t;
    return t;
}

juce::Font make (juce::Typeface::Ptr face, float size, float letterSpacing)
{
    // CSS sizes the em box; JUCE's point height is the closest match.
    auto options = juce::FontOptions (face).withPointHeight (size);
    if (! juce::exactlyEqual (letterSpacing, 0.0f))
        options = options.withKerningFactor (letterSpacing / size);
    return juce::Font (options);
}
} // namespace

namespace fonts
{
juce::Font title (float size, bool black)
{
    return make (black ? typefaces().londrinaBlack : typefaces().londrina, size, 0.0f);
}

juce::Font label (float size, bool bold, float letterSpacing)
{
    return make (bold ? typefaces().archivoBold : typefaces().archivo, size, letterSpacing);
}
} // namespace fonts

juce::Path cutShape (juce::Rectangle<float> r, std::array<float, 8> in)
{
    const float w = r.getWidth(), h = r.getHeight(), x = r.getX(), y = r.getY();
    juce::Path p;
    p.startNewSubPath (x + in[0] * w, y + in[1] * h);
    p.lineTo (x + w - in[2] * w, y + in[3] * h);
    p.lineTo (x + w - in[4] * w, y + h - in[5] * h);
    p.lineTo (x + in[6] * w, y + h - in[7] * h);
    p.closeSubPath();
    return p;
}

juce::String valueText (const juce::RangedAudioParameter& p)
{
    auto text = p.getCurrentValueAsText();
    const auto unit = p.getLabel();
    if (unit == "deg")
        text << juce::String::fromUTF8 ("\xc2\xb0");
    else if (unit.isNotEmpty())
        text << " " << unit;
    return displayText (text);
}

juce::String displayText (juce::String text)
{
    return text.toUpperCase().replace (" HZ", " Hz").replace ("KHZ", "kHz").replace (" MS", " ms").replace (" DB", " dB");
}
} // namespace ui
