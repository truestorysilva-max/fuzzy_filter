#pragma once

#include "DspUtils.h"

namespace ff
{
// A first-pass model of a two-transistor Fuzz Face, one instance per channel.
// It is meant to run oversampled (the plugin runs it at 4x).
//
// Signal path:
//   input coupling cap (high pass) -> gain set by Fuzz -> bias dead zone
//   (gating at low Bias) -> asymmetric clipper (germanium soft, silicon hard)
//   -> DC blocker -> output low pass (germanium darker) -> Volume, Mix
//
// Because the gain comes before the clipper, turning the input down cleans the
// fuzz up, much like rolling back a guitar's volume knob into a real pedal.
class FuzzFace
{
public:
    struct Params
    {
        float fuzz = 0.7f;      // 0..1
        float bias = 0.5f;      // 0..1, 0.5 = normal, lower = gated/sputtery
        bool silicon = false;   // false = germanium
        float volume = 1.0f;    // linear output gain
        float mix = 1.0f;       // 0 = clean, 1 = all fuzz
    };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        inputHighPass.setCutoff (70.0f, fs);
        dcBlock.setCutoff (8.0f, fs);
        updateTone (false);
        reset();
    }

    void reset()
    {
        inputHighPass.reset();
        dcBlock.reset();
        toneLowPass.reset();
    }

    // Gain into the transistors. The base curve runs 2x to 300x; the extra
    // (1 + 4 fuzz^4) term leaves the lower half of the knob almost unchanged
    // and makes the top end 5x hotter (1500x at 100%).
    static float driveFor (float fuzz)
    {
        const float f2 = fuzz * fuzz;
        return 2.0f * std::pow (150.0f, fuzz) * (1.0f + 4.0f * f2 * f2);
    }

    void setParams (const Params& p)
    {
        params = p;
        if (p.silicon != toneIsSilicon)
            updateTone (p.silicon);
    }

    float process (float x)
    {
        float v = inputHighPass.process (x) * driveFor (params.fuzz);

        // Low Bias starves the transistors: small signals fall into a dead
        // zone, which gives the gated, sputtery sound.
        const float gate = std::max (0.0f, 0.5f - params.bias) * 1.2f;
        if (gate > 0.0f)
            v = v - clampf (v, -gate, gate);

        // Bias also shifts the operating point, which makes the clipping
        // lopsided (more even harmonics).
        v += (params.bias - 0.5f) * 0.8f;

        float y = params.silicon ? clipSilicon (v) : clipGermanium (v);
        y = dcBlock.process (y);
        y = toneLowPass.process (y) * 0.6f;

        const float wet = y * params.volume;
        return params.mix * wet + (1.0f - params.mix) * x;
    }

private:
    // Germanium: soft knee, and the negative side clips later than the positive.
    static float clipGermanium (float v)
    {
        return v >= 0.0f ? std::tanh (v) : 1.3f * std::tanh (v / 1.3f);
    }

    // Silicon: a much harder knee and less asymmetry.
    static float hardKnee (float x)
    {
        const float x2 = x * x;
        return x / std::pow (1.0f + x2 * x2, 0.25f);
    }

    static float clipSilicon (float v)
    {
        return v >= 0.0f ? hardKnee (v) : 1.1f * hardKnee (v / 1.1f);
    }

    void updateTone (bool silicon)
    {
        toneIsSilicon = silicon;
        toneLowPass.setCutoff (silicon ? 8500.0f : 5500.0f, fs);
    }

    double fs = 44100.0;
    Params params;
    bool toneIsSilicon = false;
    OnePoleHighPass inputHighPass, dcBlock;
    OnePoleLowPass toneLowPass;
};
} // namespace ff
