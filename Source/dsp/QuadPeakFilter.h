#pragma once

#include "DspUtils.h"
#include <vector>

namespace ff
{
// The QPAS-inspired filter.
//
// Four cores: A and B feed the left side, C and D feed the right.
// Every core is a 12 dB/octave state variable filter that produces low pass,
// band pass and high pass at once, plus a comb filter tuned to the same
// frequency. Notch Pass (the QPAS's Smile Pass) is built from the low and high
// pass outputs. Each side's output is the sum of its two cores, so every
// output carries two peaks.
//
// Frequency sets all four cores. Space L spreads A and B apart, Space R
// spreads C and D. Q is shared. Excite 1 pushes cores A and C (one per side),
// Excite 2 pushes B and D.
class QuadPeakFilter
{
public:
    struct Params
    {
        float frequency = 1000.0f; // Hz
        float q = 0.3f;            // -1..1, 0 = no resonance, < 0 cuts the peaks
        float spaceL = 0.0f;       // 0..1
        float spaceR = 0.0f;       // 0..1
        float excite1 = 0.0f;      // 0..1
        float excite2 = 0.0f;      // 0..1
        float combDamping = 0.3f;  // 0..1
    };

    // One output sample of every filter type, for one side.
    struct Outputs
    {
        float lp = 0.0f, bp = 0.0f, hp = 0.0f, np = 0.0f, comb = 0.0f;
    };

    // How far each core moves away from Frequency at full Space, in octaves.
    // The two cores of a side move by different amounts, so the peaks spread
    // away from the main cutoff and from each other.
    static constexpr float kSpaceUpOctaves = 2.5f;
    static constexpr float kSpaceDownOctaves = 1.5f;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const int combSize = (int) (fs / kMinFrequency) + 8;
        for (auto& c : cores)
            c.comb.prepare (combSize);
        exciteRelease = std::exp (-1.0f / (0.05f * (float) fs)); // 50 ms
        reset();
    }

    void reset()
    {
        for (auto& c : cores)
            c.reset();
        exciteEnv1 = exciteEnv2 = 0.0f;
    }

    // Processes one stereo sample.
    // ex1L/ex1R and ex2L/ex2R are the audio that drives Excite 1 and Excite 2
    // (the input audio or the sidechain, chosen by the plugin).
    void process (float inL, float inR,
                  float ex1L, float ex1R, float ex2L, float ex2R,
                  const Params& p, Outputs& outL, Outputs& outR)
    {
        const float maxF = 0.45f * (float) fs;
        const float f = clampf (p.frequency, kMinFrequency, maxF);

        // Q mapping: 0 = flat (Butterworth), +1 = near self-oscillation,
        // negative = peaks are cut instead of boosted.
        const float q = clampf (p.q, -1.0f, 1.0f);
        // Below zero the damping goes up instead, which rounds the peaks off
        // and pulls the response down around the cutoff.
        const float k = q >= 0.0f ? kButterworth * std::pow (1.0f - q, 1.6f) + 0.01f
                                  : kButterworth * (1.0f - 3.0f * q);
        const float inputScale = std::min (1.0f, std::pow (k / kButterworth, 0.25f));
        const float bpScale = std::sqrt (k * kButterworth);

        // Comb feedback follows Q. Negative feedback gives the hollow,
        // odd-harmonics-only sound.
        const float fb = q >= 0.0f ? 0.97f * std::pow (q, 0.8f) : -0.97f * std::pow (-q, 0.8f);
        const float dampHz = 200.0f + 19800.0f * (1.0f - p.combDamping) * (1.0f - p.combDamping);
        const float dampCoef = std::exp (-2.0f * kPi * std::min (dampHz, maxF) / (float) fs);

        // Excite envelopes: fast attack, 50 ms release. A transient briefly
        // raises the resonance of the excited cores.
        exciteEnv1 = std::max (0.5f * (std::abs (ex1L) + std::abs (ex1R)), exciteEnv1 * exciteRelease);
        exciteEnv2 = std::max (0.5f * (std::abs (ex2L) + std::abs (ex2R)), exciteEnv2 * exciteRelease);
        const float k1 = std::max (0.005f, k * (1.0f - 0.85f * p.excite1 * std::min (exciteEnv1, 1.0f)));
        const float k2 = std::max (0.005f, k * (1.0f - 0.85f * p.excite2 * std::min (exciteEnv2, 1.0f)));

        // Notch Pass blend: high pass at the bottom of the range, a notch in
        // the middle, low pass at the top, so the level stays fairly even.
        const float w = clampf (std::log2 (f / 20.0f) / 10.0f, 0.0f, 1.0f);

        const float fA = clampf (f * std::exp2 ( kSpaceUpOctaves   * p.spaceL), kMinFrequency, maxF);
        const float fB = clampf (f * std::exp2 (-kSpaceDownOctaves * p.spaceL), kMinFrequency, maxF);
        const float fC = clampf (f * std::exp2 ( kSpaceUpOctaves   * p.spaceR), kMinFrequency, maxF);
        const float fD = clampf (f * std::exp2 (-kSpaceDownOctaves * p.spaceR), kMinFrequency, maxF);

        const Ctx base { inputScale, bpScale, fb, dampCoef, w };

        Outputs a, b, c, d;
        cores[0].tick (inL, ex1L * p.excite1, fA, k1, base, fs, a);
        cores[1].tick (inL, ex2L * p.excite2, fB, k2, base, fs, b);
        cores[2].tick (inR, ex1R * p.excite1, fC, k1, base, fs, c);
        cores[3].tick (inR, ex2R * p.excite2, fD, k2, base, fs, d);

        sumSide (a, b, outL);
        sumSide (c, d, outR);
    }

    static constexpr float kMinFrequency = 20.0f;

private:
    static constexpr float kButterworth = 1.41421356f;

    struct Ctx
    {
        float inputScale, bpScale, fb, dampCoef, notchBlend;
    };

    struct Comb
    {
        std::vector<float> buf;
        int size = 0, writePos = 0;
        float damped = 0.0f;

        void prepare (int n) { buf.assign ((size_t) n, 0.0f); size = n; writePos = 0; damped = 0.0f; }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); writePos = 0; damped = 0.0f; }

        // Returns the delayed, damped signal (one period of the comb).
        float tick (float x, float delaySamples, float fb, float dampCoef)
        {
            delaySamples = clampf (delaySamples, 2.0f, (float) (size - 2));
            float rp = (float) writePos - delaySamples;
            if (rp < 0.0f) rp += (float) size;
            const int i0 = (int) rp;
            const int i1 = (i0 + 1 == size) ? 0 : i0 + 1;
            const float frac = rp - (float) i0;
            const float r = buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);

            damped = r + dampCoef * (damped - r);
            buf[(size_t) writePos] = softClip (x + fb * damped, 4.0f);
            if (++writePos == size) writePos = 0;
            return damped;
        }
    };

    struct Core
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        Comb comb;

        void reset() { ic1 = ic2 = 0.0f; comb.reset(); }

        void tick (float x, float excite, float hz, float k, const Ctx& c, double fs, Outputs& o)
        {
            // State variable filter (trapezoidal / zero-delay feedback form).
            const float g = std::tan (kPi * hz / (float) fs);
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;

            const float v0 = x * c.inputScale + excite * 1.5f;
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = softClip (2.0f * v1 - ic1, 6.0f); // keeps self-oscillation bounded
            ic2 = 2.0f * v2 - ic2;

            const float lp = v2;
            const float bp = v1;
            const float hp = v0 - k * v1 - v2;
            o.lp = lp;
            o.hp = hp;
            o.bp = bp * c.bpScale;

            const float wl = c.notchBlend;
            o.np = (1.0f - wl) * o.hp + wl * o.lp + std::min (wl, 1.0f - wl) * (o.lp + o.hp);

            // With negative feedback the comb's peaks sit at odd multiples of
            // half the delay rate, so the delay is halved to keep the pitch at
            // Frequency.
            const float period = (float) fs / hz * (c.fb < 0.0f ? 0.5f : 1.0f);
            const float delayed = comb.tick (x + excite * 1.5f, period, c.fb, c.dampCoef);
            // The direct path takes the same sign as the feedback, so with
            // negative Q the even harmonics cancel.
            o.comb = 0.5f * (x + (c.fb < 0.0f ? -delayed : delayed)) * (1.0f - 0.5f * std::abs (c.fb));
        }
    };

    static void sumSide (const Outputs& a, const Outputs& b, Outputs& out)
    {
        constexpr float ceiling = 2.0f;
        out.lp   = softClip (0.5f * (a.lp + b.lp), ceiling);
        out.bp   = softClip (0.5f * (a.bp + b.bp), ceiling);
        out.hp   = softClip (0.5f * (a.hp + b.hp), ceiling);
        out.np   = softClip (0.5f * (a.np + b.np), ceiling);
        out.comb = softClip (0.5f * (a.comb + b.comb), ceiling);
    }

    double fs = 44100.0;
    Core cores[4];
    float exciteEnv1 = 0.0f, exciteEnv2 = 0.0f, exciteRelease = 0.999f;
};
} // namespace ff
