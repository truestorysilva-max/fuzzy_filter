#include "Displays.h"
#include "../ModDefs.h"
#include "../dsp/QuadPeakFilter.h"

namespace ui
{
float currentPlain (FuzzyFilterProcessor& p, int slot)
{
    auto* param = p.apvts.getParameter (Mod::slotParamIds[(size_t) slot]);
    const float live = p.getSlotLive (slot);
    return param->convertFrom0to1 (live >= 0.0f ? live : param->getValue());
}

namespace
{
float plainOf (FuzzyFilterProcessor& p, const juce::String& id)
{
    return p.apvts.getRawParameterValue (id)->load();
}

float levelGain (FuzzyFilterProcessor& p, int slot)
{
    const float db = currentPlain (p, slot);
    return db <= kLevelOffDb ? 0.0f : juce::Decibels::decibelsToGain (db);
}

using Complex = std::complex<float>;

// The response of one side (two cores) at frequency `f`, mirroring
// QuadPeakFilter's maths with the analog prototype filters.
struct SideModel
{
    float fUp, fDown, k, inputScale, bpScale, fb, notchW, dampHz;
    float gLP, gBP, gHP, gNP, gComb;

    Complex core (float f, float fc) const
    {
        const Complex s (0.0f, f / fc);
        const Complex den = s * s + k * s + 1.0f;
        const Complex lp = inputScale / den;
        const Complex bp = inputScale * s / den * bpScale;
        const Complex hp = inputScale * s * s / den;
        const float w = notchW;
        const Complex np = (1.0f - w) * hp + w * lp + std::min (w, 1.0f - w) * (lp + hp);

        Complex out = gLP * lp + gBP * bp + gHP * hp + gNP * np;
        if (gComb > 0.0f)
        {
            const float period = (1.0f / fc) * (fb < 0.0f ? 0.5f : 1.0f);
            const Complex e = std::polar (1.0f, -juce::MathConstants<float>::twoPi * f * period);
            const Complex damp = 1.0f / Complex (1.0f, f / dampHz);
            const Complex delayed = e * damp / (1.0f - fb * e * damp);
            out += gComb * 0.5f * (1.0f + (fb < 0.0f ? -delayed : delayed)) * (1.0f - 0.5f * std::abs (fb));
        }
        return out;
    }

    float magnitudeDb (float f) const
    {
        const Complex h = 0.5f * (core (f, fUp) + core (f, fDown));
        return juce::Decibels::gainToDecibels (std::abs (h), -60.0f);
    }
};

SideModel makeSide (FuzzyFilterProcessor& p, float space)
{
    SideModel m {};
    const float f = juce::jlimit (20.0f, 20000.0f, currentPlain (p, Mod::frequency));
    const float q = juce::jlimit (-1.0f, 1.0f, currentPlain (p, Mod::q));
    constexpr float bw = 1.41421356f;
    m.k = q >= 0.0f ? bw * std::pow (1.0f - q, 1.6f) + 0.01f : bw * (1.0f - 3.0f * q);
    m.inputScale = std::min (1.0f, std::pow (m.k / bw, 0.25f));
    m.bpScale = std::sqrt (m.k * bw);
    m.fb = q >= 0.0f ? 0.97f * std::pow (q, 0.8f) : -0.97f * std::pow (-q, 0.8f);
    m.notchW = juce::jlimit (0.0f, 1.0f, std::log2 (f / 20.0f) / 10.0f);
    const float damping = juce::jlimit (0.0f, 1.0f, currentPlain (p, Mod::combDamping));
    m.dampHz = 200.0f + 19800.0f * (1.0f - damping) * (1.0f - damping);
    const float s = juce::jlimit (0.0f, 1.0f, space);
    m.fUp = juce::jlimit (20.0f, 21000.0f, f * std::exp2 (ff::QuadPeakFilter::kSpaceUpOctaves * s));
    m.fDown = juce::jlimit (20.0f, 21000.0f, f * std::exp2 (-ff::QuadPeakFilter::kSpaceDownOctaves * s));
    m.gLP = levelGain (p, Mod::levelLP);
    m.gBP = levelGain (p, Mod::levelBP);
    m.gHP = levelGain (p, Mod::levelHP);
    m.gNP = levelGain (p, Mod::levelNP);
    m.gComb = levelGain (p, Mod::levelComb);
    return m;
}

float xToHz (float x, float width) { return 20.0f * std::pow (1000.0f, x / width); }
float hzToX (float hz, float width) { return width * std::log (hz / 20.0f) / std::log (1000.0f); }
} // namespace

// ---------------------------------------------------------------- FilterDisplay

void FilterDisplay::refresh()
{
    const std::array<float, 12> now {
        currentPlain (proc, Mod::frequency), currentPlain (proc, Mod::q), currentPlain (proc, Mod::spaceL),
        currentPlain (proc, Mod::spaceR), currentPlain (proc, Mod::combDamping), currentPlain (proc, Mod::levelLP),
        currentPlain (proc, Mod::levelBP), currentPlain (proc, Mod::levelHP), currentPlain (proc, Mod::levelNP),
        currentPlain (proc, Mod::levelComb), 0.0f, 0.0f
    };
    if (! std::equal (now.begin(), now.end(), last.begin(), [] (float a, float b) { return juce::exactlyEqual (a, b); }))
    {
        last = now;
        repaint();
    }
}

void FilterDisplay::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float w = b.getWidth(), h = b.getHeight();
    g.setColour (colours::cream);
    g.fillRect (b);

    constexpr float topDb = 30.0f, bottomDb = -36.0f;
    auto dbToY = [&] (float db) { return juce::jmap (juce::jlimit (bottomDb, topDb, db), topDb, bottomDb, 0.0f, h); };

    auto curve = [&] (const SideModel& m, bool closed)
    {
        juce::Path p;
        if (closed) p.startNewSubPath (0.0f, h);
        for (int i = 0; (float) i * 1.5f <= w; ++i)
        {
            const float x = (float) i * 1.5f;
            const float y = dbToY (m.magnitudeDb (xToHz (x, w)));
            if (! closed && i == 0) p.startNewSubPath (x, y);
            else p.lineTo (x, y);
        }
        if (closed) { p.lineTo (w, h); p.closeSubPath(); }
        return p;
    };

    const auto left = makeSide (proc, currentPlain (proc, Mod::spaceL));
    const auto right = makeSide (proc, currentPlain (proc, Mod::spaceR));
    g.setColour (colours::red);
    g.fillPath (curve (left, true));
    g.setColour (colours::ink);
    g.strokePath (curve (right, false), juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Frequency marker and labels.
    const float f = currentPlain (proc, Mod::frequency);
    const float fx = hzToX (juce::jlimit (20.0f, 20000.0f, f), w);
    g.setColour (colours::ink);
    g.fillRect (juce::Rectangle<float> (fx - 1.0f, 0.0f, 2.0f, h));
    g.setFont (fonts::label (10.0f, true, 1.0f));
    const auto hzText = f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz"
                                     : juce::String (juce::roundToInt (f)) + " Hz";
    const bool labelLeft = fx > w - 60.0f;
    g.drawText (hzText, juce::Rectangle<float> (labelLeft ? fx - 64.0f : fx + 5.0f, 2.0f, 58.0f, 12.0f),
                labelLeft ? juce::Justification::centredRight : juce::Justification::centredLeft, false);
    g.setColour (colours::red);
    g.drawText ("L", juce::Rectangle<float> (6.0f, 2.0f, 12.0f, 12.0f), juce::Justification::centredLeft, false);
    g.setColour (colours::ink);
    g.drawText ("R", juce::Rectangle<float> (w - 18.0f, 2.0f, 12.0f, 12.0f), juce::Justification::centredRight, false);
}

// ---------------------------------------------------------------- LfoDisplay

namespace
{
float waveAt (int wave, float p)
{
    static const float steps[8] = { 0.6f, -0.3f, 0.9f, -0.8f, 0.2f, -0.5f, 0.7f, -0.1f };
    p -= std::floor (p);
    switch (wave)
    {
        case 0: return std::sin (juce::MathConstants<float>::twoPi * p);
        case 1: return 1.0f - 4.0f * std::abs (p - 0.5f);
        case 2: return 2.0f * p - 1.0f;
        case 3: return 1.0f - 2.0f * p;
        case 4: return p < 0.5f ? 1.0f : -1.0f;
        case 5: return steps[(int) (p * 8.0f) & 7];
        case 6:
        {
            const float x = p * 8.0f;
            const int i = (int) x;
            const float t = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * (x - (float) i)));
            return steps[i & 7] + (steps[(i + 1) & 7] - steps[i & 7]) * t;
        }
        default: return 0.0f;
    }
}
} // namespace

void LfoDisplay::refresh()
{
    const int restarts = proc.getLfoRestarts (lfo);
    if (lastRestarts >= 0 && restarts != lastRestarts)
        restartFlash = 1.0f;
    else
        restartFlash = std::max (0.0f, restartFlash - 0.12f);
    lastRestarts = restarts;
    repaint();
}

void LfoDisplay::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float stroke = 6.0f;
    const float w = b.getWidth(), h = b.getHeight();
    const float amp = (h - stroke) * 0.5f, mid = h * 0.5f;
    const int wave = juce::roundToInt (currentPlain (proc, Mod::lfo1Wave + lfo)); // follows the grid
    const float stereo = plainOf (proc, ModParamIds::lfo (lfo + 1, "Stereo")) / 360.0f;

    // Two cycles across the display, like the design.
    juce::Path p;
    for (int i = 0; (float) i <= w; ++i)
    {
        const float x = (float) i;
        const float y = mid - amp * waveAt (wave, 2.0f * x / w);
        if (i == 0) p.startNewSubPath (x, y);
        else p.lineTo (x, y);
    }
    g.setColour (colours::cream);
    g.strokePath (p, juce::PathStrokeType (stroke, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));

    // Markers: right channel (cream ring) first, then left (red).
    const float phase = (float) proc.getLfoPhase (lfo);
    auto marker = [&] (float ph, bool left)
    {
        ph -= std::floor (ph);
        const float x = ph * w * 0.5f;
        const float y = mid - amp * waveAt (wave, ph);
        if (left)
        {
            g.setColour (colours::red);
            g.fillRect (juce::Rectangle<float> (x - 1.5f, 0.0f, 3.0f, h));
            g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ x, y }));
        }
        else
        {
            g.setColour (colours::ink);
            g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre ({ x, y }));
            g.setColour (colours::red);
            g.drawEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre ({ x, y }), 2.0f);
        }
    };
    if (stereo > 0.001f)
        marker (phase + stereo, false);
    marker (phase, true);

    // A "RESTART" tag that flashes each time the LFO is restarted.
    if (restartFlash > 0.0f)
    {
        const auto tag = juce::Rectangle<float> (w - 62.0f, 0.0f, 62.0f, 16.0f);
        g.setColour (colours::red.withAlpha (restartFlash));
        g.fillRect (tag);
        g.setColour (colours::cream.withAlpha (restartFlash));
        g.setFont (fonts::label (10.0f, true, 1.0f));
        g.drawText ("RESTART", tag, juce::Justification::centred, false);
    }
}

// ---------------------------------------------------------------- EnvDisplay

void EnvDisplay::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float w = b.getWidth(), h = b.getHeight();
    auto norm = [&] (const char* suffix)
    {
        auto* param = proc.apvts.getParameter (ModParamIds::env (env + 1, suffix));
        return param->getValue();
    };
    juce::Path shape;
    if (followerView)
    {
        // How the follower answers a short burst: rise, hold, then fall.
        const float a = 3.0f + 50.0f * norm ("FolAttack");
        const float hold = 60.0f * norm ("FolHold");
        const float r = 8.0f + (w * 0.6f) * norm ("FolRelease");
        const float x0 = 6.0f;
        shape.startNewSubPath (x0, h);
        for (int i = 0; i <= 12; ++i) // attack curve
        {
            const float t = (float) i / 12.0f;
            shape.lineTo (x0 + a * t, h - (h - 2.0f) * (1.0f - std::exp (-4.0f * t)) / (1.0f - std::exp (-4.0f)));
        }
        shape.lineTo (x0 + a + hold, 2.0f);
        for (int i = 1; i <= 24; ++i) // release curve
        {
            const float t = (float) i / 24.0f;
            shape.lineTo (std::min (w, x0 + a + hold + r * t), 2.0f + (h - 2.0f) * (1.0f - std::exp (-4.6f * t)));
        }
        shape.lineTo (std::min (w, x0 + a + hold + r), h);
        shape.closeSubPath();
    }
    else
    {
        const float a = 4.0f + 50.0f * norm ("Attack");
        const float d = 4.0f + 50.0f * norm ("Decay");
        const float r = 4.0f + 60.0f * norm ("Release");
        const float s = plainOf (proc, ModParamIds::env (env + 1, "Sustain"));
        const float sustainY = 2.0f + (h - 2.0f) * (1.0f - s);
        const float hold = std::max (10.0f, w - a - d - r - 4.0f);
        shape.startNewSubPath (0.0f, h);
        shape.lineTo (a, 2.0f);
        shape.lineTo (a + d, sustainY);
        shape.lineTo (a + d + hold, sustainY);
        shape.lineTo (std::min (w, a + d + hold + r), h);
        shape.closeSubPath();
    }
    g.setColour (colours::ink);
    g.fillPath (shape);

    // Follower threshold: 0 dB at the top, -60 dB at the bottom.
    const float thr = plainOf (proc, ModParamIds::env (env + 1, "Threshold"));
    const float ty = juce::jlimit (1.0f, h - 3.0f, h * (-thr / 60.0f));
    g.setColour (colours::cream);
    g.fillRect (juce::Rectangle<float> (0.0f, ty, w, 2.0f));
    g.setFont (fonts::label (9.0f, true, 1.0f));
    g.drawText ("THRESH", juce::Rectangle<float> (w - 52.0f, ty - 13.0f, 50.0f, 11.0f), juce::Justification::centredRight, false);

    // Live: the follower level as a tick on the right, the envelope as a bar on the left.
    const float fol = proc.getSourceValue (Mod::follower1 + env);
    const float folDb = juce::Decibels::gainToDecibels (fol, -60.0f);
    const float fy = juce::jlimit (0.0f, h, h * (-folDb / 60.0f));
    juce::Path tick;
    tick.addTriangle (w, fy - 5.0f, w, fy + 5.0f, w - 8.0f, fy);
    g.fillPath (tick);
    const float ev = juce::jlimit (0.0f, 1.0f, proc.getSourceValue (Mod::env1 + env));
    g.setColour (colours::cream);
    g.fillRect (juce::Rectangle<float> (0.0f, h * (1.0f - ev), 4.0f, h * ev));
}

// ---------------------------------------------------------------- InputMeter

void InputMeter::refresh()
{
    bool changed = false;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float peak = proc.takeInputPeak (ch);
        const float db = juce::Decibels::gainToDecibels (peak, -60.0f);
        const float target = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
        const float next = target > level[ch] ? target : std::max (target, level[ch] - 0.04f);
        changed = changed || std::abs (next - level[ch]) > 0.001f;
        level[ch] = next;
    }
    if (changed)
        repaint();
}

void InputMeter::paint (juce::Graphics& g)
{
    const float barW = 12.0f, gap = 6.0f;
    const float h = (float) getHeight();
    float x = ((float) getWidth() - 2.0f * barW - gap) * 0.5f;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto bar = juce::Rectangle<float> (x, 0.0f, barW, h);
        g.setColour (colours::ink.withAlpha (0.35f));
        g.fillRect (bar);
        g.setColour (colours::cream);
        g.fillRect (bar.withTop (h * (1.0f - level[ch])));
        x += barW + gap;
    }
}
} // namespace ui
