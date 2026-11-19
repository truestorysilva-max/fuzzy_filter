#pragma once

// Modulation sources: LFOs, envelope followers and ADSR envelopes.
// Plain C++, no JUCE, so tests/dsp_tests.cpp can check them directly.

#include "DspUtils.h"
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
        float held = 0.0f, from = 0.0f, to = 0.0f;
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
                if (wrapped) { c.from = c.to; c.to = c.rng.nextBipolar(); }
                return c.from + (c.to - c.from) * 0.5f * (1.0f - std::cos (kPi * (float) p));
            case numWaves:
            default:
                return 0.0f;
        }
    }

    double fs = 44100.0, phase = 0.0;
    Channel chans[2];
};

// Peak envelope follower with a fast attack and a slower release.
// Output is the signal level, linear (1.0 = 0 dBFS).
class EnvelopeFollower
{
public:
    void prepare (double sampleRate)
    {
        attack = 1.0f - std::exp (-1.0f / (0.001f * (float) sampleRate)); // 1 ms
        release = 1.0f - std::exp (-1.0f / (0.08f * (float) sampleRate)); // 80 ms
        level = 0.0f;
    }
    void reset() { level = 0.0f; }

    float process (float x)
    {
        const float a = std::abs (x);
        level += (a > level ? attack : release) * (a - level);
        return level;
    }

private:
    float attack = 1.0f, release = 1.0f, level = 0.0f;
};

// ADSR envelope, 0..1. Linear attack, exponential decay and release.
class Adsr
{
public:
    enum Stage { idle, attackStage, decayStage, sustainStage, releaseStage };

    void prepare (double sampleRate) { fs = (float) sampleRate; reset(); }
    void reset() { stage = idle; value = 0.0f; }

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
