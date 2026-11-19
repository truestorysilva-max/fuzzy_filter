#pragma once

// Small, dependency-free helpers shared by the DSP classes.
// Nothing in Source/dsp includes JUCE, so the DSP can be tested on its own
// (see tests/dsp_tests.cpp).

#include <algorithm>
#include <cmath>

namespace ff
{
constexpr float kPi = 3.14159265358979323846f;

inline float clampf (float x, float lo, float hi) { return std::min (hi, std::max (lo, x)); }

inline float dbToGain (float db, float floorDb = -60.0f)
{
    return db <= floorDb ? 0.0f : std::pow (10.0f, db * 0.05f);
}

// Gentle limiter used inside feedback loops and on outputs so that high
// resonance or feedback can never run away.
inline float softClip (float x, float ceiling)
{
    return ceiling * std::tanh (x / ceiling);
}

// One-pole low pass, y += a * (x - y).
struct OnePoleLowPass
{
    float a = 1.0f, y = 0.0f;

    void setCutoff (float hz, double sampleRate)
    {
        const float w = 2.0f * kPi * hz / (float) sampleRate;
        a = 1.0f - std::exp (-w);
    }
    void reset() { y = 0.0f; }
    float process (float x) { y += a * (x - y); return y; }
};

// One-pole high pass (leaky differentiator), also used as a DC blocker.
struct OnePoleHighPass
{
    float r = 0.995f, x1 = 0.0f, y1 = 0.0f;

    void setCutoff (float hz, double sampleRate)
    {
        r = std::exp (-2.0f * kPi * hz / (float) sampleRate);
    }
    void reset() { x1 = y1 = 0.0f; }
    float process (float x)
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }
};

} // namespace ff
