#pragma once

// Modulation sources: LFOs, envelope followers and ADSR envelopes.
// Plain C++, no JUCE, so tests/dsp_tests.cpp can check them directly.

#include "DspUtils.h"
#include <algorithm>
#include <cstdint>

namespace ff
{
// A simple fast random generator, so the audio thread never touches std::rand.
struct FastRandom
{
    uint32_t state = 0x12345678u;
    float nextBipolar()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) (state >> 8) / 8388608.0f - 1.0f; // -1 .. 1
    }
};

// LFO that runs from 0.01 Hz up to audio rate. Saw and square are band
// limited (polyBLEP) so they stay clean at audio rates.
// It produces a left and a right value; the right one can be phase shifted
// for stereo movement.
class Lfo
{
public:
    enum Wave { sine = 0, triangle, sawUp, sawDown, square, sampleHold, smoothRandom, numWaves };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        reset (0.0);
    }

    void reset (double phase01)
    {
        phase = phase01 - std::floor (phase01);
        for (auto& c : chans)
            c = Channel {};
        chans[1].rng.state = 0x9e3779b9u;
        for (auto& c : chans)
            for (auto& pt : c.pts)
                pt = 0.8f * c.rng.nextBipolar();
    }

    // Jumps to a phase (used for tempo sync and retrigger).
    void setPhase (double phase01) { phase = phase01 - std::floor (phase01); }
    double getPhase() const { return phase; }

    // Advances one sample. `phaseOffset` is the Phase control (0..1),
    // `stereoOffset` shifts the right channel (0..0.5 = 0..180 degrees).
    void tick (float hz, Wave wave, float phaseOffset, float stereoOffset, float& outL, float& outR)
    {
        const double dt = std::min (0.5, (double) hz / fs);
        outL = valueAt (chans[0], phase + phaseOffset, dt, wave);
        outR = valueAt (chans[1], phase + phaseOffset + stereoOffset, dt, wave);
        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
    }

private:
    struct Channel
    {
        double lastP = 0.0;
        float held = 0.0f;
        float pts[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // smooth random: the curve runs from pts[1] to pts[2]
        FastRandom rng;
    };

    static float polyBlep (double t, double dt)
    {
        if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    static float valueAt (Channel& c, double p, double dt, Wave wave)
    {
        p -= std::floor (p);
        const bool wrapped = p < c.lastP;
        c.lastP = p;

        switch (wave)
        {
            case sine:     return std::sin (2.0f * kPi * (float) p);
            case triangle: return 1.0f - 4.0f * std::abs ((float) p - 0.5f);
            case sawUp:    return (2.0f * (float) p - 1.0f) - polyBlep (p, dt);
            case sawDown:  return -((2.0f * (float) p - 1.0f) - polyBlep (p, dt));
            case square:
            {
                double p2 = p + 0.5; p2 -= std::floor (p2);
                return (p < 0.5 ? 1.0f : -1.0f) + polyBlep (p, dt) - polyBlep (p2, dt);
            }
            case sampleHold:
                if (wrapped) c.held = c.rng.nextBipolar();
                return c.held;
            case smoothRandom:
            {
                // A Catmull-Rom curve through random points. Unlike a cosine
                // ease, it keeps moving through each point instead of
                // stopping there, so it never stalls at the end of a cycle.
                if (wrapped)
                {
                    c.pts[0] = c.pts[1]; c.pts[1] = c.pts[2]; c.pts[2] = c.pts[3];
                    c.pts[3] = 0.8f * c.rng.nextBipolar(); // 0.8 keeps the curve's overshoot inside -1..1
                }
                const float t = (float) p, t2 = t * t, t3 = t2 * t;
                const float v = 0.5f * ((2.0f * c.pts[1]) + (-c.pts[0] + c.pts[2]) * t
                                        + (2.0f * c.pts[0] - 5.0f * c.pts[1] + 4.0f * c.pts[2] - c.pts[3]) * t2
                                        + (-c.pts[0] + 3.0f * c.pts[1] - 3.0f * c.pts[2] + c.pts[3]) * t3);
                return std::clamp (v, -1.0f, 1.0f);
            }
            case numWaves:
            default:
                return 0.0f;
        }
    }

    double fs = 44100.0, phase = 0.0;
    Channel chans[2];
};

// Peak envelope follower with attack, hold and release times.
// Output is the signal level, linear (1.0 = 0 dBFS).
class EnvelopeFollower
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        setTimes (0.001f, 0.0f, 0.08f);
        reset();
    }
    void reset() { level = 0.0f; holdLeft = 0; }

    // Attack: how fast it rises. Hold: how long it stays at a peak before
    // falling. Release: how fast it falls. All in seconds.
    void setTimes (float attackS, float holdS, float releaseS)
    {
        attack = 1.0f - std::exp (-1.0f / std::max (1.0f, attackS * fs));
        release = 1.0f - std::exp (-1.0f / std::max (1.0f, releaseS * fs));
        holdSamples = (int) (std::max (0.0f, holdS) * fs);
    }

    float process (float x)
    {
        const float a = std::abs (x);
        if (a >= level)
        {
            level += attack * (a - level);
            holdLeft = holdSamples;
        }
        else if (holdLeft > 0)
            --holdLeft;
        else
            level += release * (a - level);
        return level;
    }

private:
    float fs = 44100.0f, attack = 1.0f, release = 1.0f, level = 0.0f;
    int holdSamples = 0, holdLeft = 0;
};

// ADSR envelope, 0..1. Linear attack, exponential decay and release.
class Adsr
{
public:
    enum Stage { idle, attackStage, decayStage, sustainStage, releaseStage };

    void prepare (double sampleRate) { fs = (float) sampleRate; reset(); }
    void reset() { stage = idle; value = 0.0f; }

    // Starts the attack again from wherever the envelope is now (a new
    // MIDI note while others are held), so there is no jump to zero.
    void retrigger() { stage = attackStage; }

    void gate (bool on)
    {
        if (on && (stage == idle || stage == releaseStage)) stage = attackStage;
        else if (! on && stage != idle && stage != releaseStage) stage = releaseStage;
    }

    // Times in seconds, sustain 0..1.
    float tick (float attackS, float decayS, float sustain, float releaseS)
    {
        switch (stage)
        {
            case attackStage:
                value += 1.0f / std::max (1.0f, attackS * fs);
                if (value >= 1.0f) { value = 1.0f; stage = decayStage; }
                break;
            case decayStage:
                value = sustain + (value - sustain) * coef (decayS);
                if (std::abs (value - sustain) < 1.0e-4f) { value = sustain; stage = sustainStage; }
                break;
            case sustainStage:
                value = sustain; // follows the Sustain knob live
                break;
            case releaseStage:
                value *= coef (releaseS);
                if (value < 1.0e-5f) { value = 0.0f; stage = idle; }
                break;
            case idle:
            default:
                break;
        }
        return value;
    }

    Stage getStage() const { return stage; }
    float getValue() const { return value; }

private:
    // Reaches 99% of the way to its target in `seconds`.
    float coef (float seconds) const { return std::exp (-4.6f / std::max (1.0f, seconds * fs)); }

    float fs = 44100.0f, value = 0.0f;
    Stage stage = idle;
};

// Turns a follower level into a gate, with 3 dB of hysteresis so it doesn't
// chatter around the threshold.
class FollowerGate
{
public:
    // Returns true on the sample the gate opens.
    bool update (float levelLinear, float thresholdDb)
    {
        const float db = 20.0f * std::log10 (std::max (levelLinear, 1.0e-6f));
        const bool wasOpen = open;
        if (! open && db > thresholdDb) open = true;
        else if (open && db < thresholdDb - 3.0f) open = false;
        return open && ! wasOpen;
    }
    bool isOpen() const { return open; }
    void reset() { open = false; }

private:
    bool open = false;
};
} // namespace ff
