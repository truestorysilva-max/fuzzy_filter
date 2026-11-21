// Offline checks for the DSP in Source/dsp. No JUCE and no test framework:
// build and run, it prints each check and exits non-zero if any fail.

#include "dsp/FuzzFace.h"
#include "dsp/InputStage.h"
#include "dsp/Modulation.h"
#include "dsp/QuadPeakFilter.h"

#include <cstdio>
#include <functional>
#include <random>
#include <string>

using namespace ff;

namespace
{
constexpr double kFs = 48000.0;
int failures = 0;

void check (bool ok, const std::string& what, double value = 0.0)
{
    std::printf ("[%s] %s (%.4g)\n", ok ? "PASS" : "FAIL", what.c_str(), value);
    if (! ok) ++failures;
}

double toDb (double g) { return 20.0 * std::log10 (std::max (g, 1e-12)); }

enum class Type { lp, bp, hp, np, comb };

float pick (const QuadPeakFilter::Outputs& o, Type t)
{
    switch (t)
    {
        case Type::lp: return o.lp;
        case Type::bp: return o.bp;
        case Type::hp: return o.hp;
        case Type::np: return o.np;
        case Type::comb: return o.comb;
    }
    return 0.0f;
}

// Gain of one filter type for a sine at `hz`, measured on the left side.
double sineGain (QuadPeakFilter::Params p, Type type, double hz, float amp = 0.1f)
{
    QuadPeakFilter f;
    f.prepare (kFs);
    QuadPeakFilter::Outputs l, r;
    const int settle = (int) kFs, measure = (int) kFs / 2;
    double sumIn = 0.0, sumOut = 0.0;
    for (int i = 0; i < settle + measure; ++i)
    {
        const float x = amp * (float) std::sin (2.0 * 3.14159265358979 * hz * i / kFs);
        f.process (x, x, 0, 0, 0, 0, p, l, r);
        if (i >= settle) { sumIn += (double) x * x; sumOut += (double) pick (l, type) * pick (l, type); }
    }
    return std::sqrt (sumOut / sumIn);
}

bool allFinite (const QuadPeakFilter::Outputs& o)
{
    return std::isfinite (o.lp) && std::isfinite (o.bp) && std::isfinite (o.hp)
        && std::isfinite (o.np) && std::isfinite (o.comb);
}

float maxAbs (const QuadPeakFilter::Outputs& o)
{
    return std::max ({ std::abs (o.lp), std::abs (o.bp), std::abs (o.hp), std::abs (o.np), std::abs (o.comb) });
}
} // namespace

int main()
{
    // ---- Filter: stability with loud noise at every Q, Space and Frequency extreme.
    {
        std::mt19937 rng (1);
        std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
        bool finite = true;
        float peak = 0.0f;
        for (float q : { -1.0f, 0.0f, 0.5f, 0.95f, 1.0f })
            for (float space : { 0.0f, 1.0f })
                for (float hz : { 20.0f, 1000.0f, 20000.0f })
                {
                    QuadPeakFilter f;
                    f.prepare (kFs);
                    QuadPeakFilter::Params p;
                    p.q = q; p.spaceL = p.spaceR = space; p.frequency = hz;
                    p.excite1 = p.excite2 = 1.0f; p.combDamping = 0.0f;
                    QuadPeakFilter::Outputs l, r;
                    for (int i = 0; i < 48000; ++i)
                    {
                        const float x = 4.0f * noise (rng); // +12 dBFS
                        f.process (x, -x, x, x, x, x, p, l, r);
                        finite = finite && allFinite (l) && allFinite (r);
                        peak = std::max ({ peak, maxAbs (l), maxAbs (r) });
                    }
                }
        check (finite, "filter: no NaN or inf with +12 dBFS noise at all extremes");
        check (peak <= 2.0f, "filter: outputs stay within the +6 dBFS ceiling", peak);
    }

    // ---- Filter: low pass and high pass do what they say (Q = 0, 1 kHz).
    {
        QuadPeakFilter::Params p;
        p.q = 0.0f; p.frequency = 1000.0f;
        const double lpPass = sineGain (p, Type::lp, 100.0);
        const double lpStop = sineGain (p, Type::lp, 10000.0);
        check (std::abs (toDb (lpPass)) < 1.5, "low pass: 100 Hz passes at about 0 dB", toDb (lpPass));
        check (toDb (lpStop) < -30.0, "low pass: 10 kHz cut by more than 30 dB", toDb (lpStop));

        const double hpPass = sineGain (p, Type::hp, 10000.0);
        const double hpStop = sineGain (p, Type::hp, 100.0);
        check (std::abs (toDb (hpPass)) < 1.5, "high pass: 10 kHz passes at about 0 dB", toDb (hpPass));
        check (toDb (hpStop) < -30.0, "high pass: 100 Hz cut by more than 30 dB", toDb (hpStop));

        const double bpCentre = sineGain (p, Type::bp, 1000.0);
        const double bpLow = sineGain (p, Type::bp, 100.0);
        check (bpCentre > bpLow * 5.0, "band pass: 1 kHz at least 14 dB above 100 Hz", toDb (bpCentre / bpLow));
    }

    // ---- Q: positive boosts the peak, negative cuts it.
    {
        QuadPeakFilter::Params p;
        p.frequency = 1000.0f;
        p.q = 0.0f;  const double flat = sineGain (p, Type::lp, 1000.0);
        p.q = 0.8f;  const double boosted = sineGain (p, Type::lp, 1000.0);
        p.q = -1.0f; const double cut = sineGain (p, Type::lp, 1000.0);
        check (boosted > flat * 2.0, "Q +80%: peak at least 6 dB louder than Q 0", toDb (boosted / flat));
        check (cut < flat * 0.5, "Q -100%: peak at least 6 dB quieter than Q 0", toDb (cut / flat));
    }

    // ---- Space: moves the two peaks of a side apart.
    {
        QuadPeakFilter::Params p;
        p.frequency = 1000.0f; p.q = 0.8f;
        p.spaceL = 0.0f; const double together = sineGain (p, Type::bp, 1000.0);
        p.spaceL = 1.0f; const double spread = sineGain (p, Type::bp, 1000.0);
        const double atUpperPeak = sineGain (p, Type::bp, 1000.0 * std::exp2 (QuadPeakFilter::kSpaceUpOctaves));
        check (spread < together * 0.7, "Space L 100%: less energy left at the base frequency", toDb (spread / together));
        check (atUpperPeak > spread * 1.5, "Space L 100%: a peak appears 2.5 octaves up", toDb (atUpperPeak / spread));
    }

    // ---- Notch Pass: a notch in the middle of the range, even level elsewhere.
    {
        QuadPeakFilter::Params p;
        p.q = 0.0f; p.frequency = 640.0f; // middle of the 20 Hz to 20 kHz range
        const double atCutoff = sineGain (p, Type::np, 640.0);
        const double below = sineGain (p, Type::np, 60.0);
        const double above = sineGain (p, Type::np, 8000.0);
        check (toDb (atCutoff) < -20.0, "Notch Pass: deep notch at the cutoff mid-range", toDb (atCutoff));
        check (std::abs (toDb (below)) < 3.0 && std::abs (toDb (above)) < 3.0,
               "Notch Pass: within 3 dB of unity well away from the cutoff", toDb (below));
    }

    // ---- Comb: harmonics with positive Q, odd harmonics only with negative Q.
    {
        QuadPeakFilter::Params p;
        p.frequency = 200.0f; p.combDamping = 0.0f;
        p.q = 0.8f;
        const double onHarmonic = sineGain (p, Type::comb, 400.0);
        const double between = sineGain (p, Type::comb, 500.0);
        check (onHarmonic > between * 4.0, "Comb Q +80%: 2nd harmonic 12 dB above a point between harmonics",
               toDb (onHarmonic / between));

        p.q = -0.8f;
        const double odd = sineGain (p, Type::comb, 600.0);
        const double even = sineGain (p, Type::comb, 400.0);
        check (odd > even * 4.0, "Comb Q -80%: 3rd harmonic 12 dB above the 2nd", toDb (odd / even));
    }

    // ---- Excite: a transient on the excite source makes the filter ring longer.
    {
        auto tailEnergy = [] (float exciteAmount)
        {
            QuadPeakFilter f;
            f.prepare (kFs);
            QuadPeakFilter::Params p;
            p.frequency = 500.0f; p.q = 0.6f; p.excite1 = p.excite2 = exciteAmount;
            QuadPeakFilter::Outputs l, r;
            double energy = 0.0;
            for (int i = 0; i < 24000; ++i)
            {
                const float click = (i < 32) ? 0.8f : 0.0f;
                f.process (click, click, click, click, click, click, p, l, r);
                if (i > 2400) energy += (double) l.bp * l.bp; // after 50 ms
            }
            return energy;
        };
        const double off = tailEnergy (0.0f), on = tailEnergy (1.0f);
        check (on > off * 2.0, "Excite 100%: ringing after a click at least 3 dB stronger", toDb (std::sqrt (on / off)));
    }


    // ---- LFO: right rate, stays within -1..1, stereo offset shifts the right side.
    {
        Lfo lfo;
        lfo.prepare (kFs);
        int upCrossings = 0;
        float prev = 0.0f, lo = 0.0f, hi = 0.0f;
        bool bounded = true;
        for (int w = 0; w < Lfo::numWaves; ++w)
        {
            lfo.reset (0.0);
            for (int i = 0; i < 48000; ++i)
            {
                float l, r;
                lfo.tick (1500.0f, (Lfo::Wave) w, 0.0f, 0.25f, l, r); // audio rate
                bounded = bounded && std::isfinite (l) && std::abs (l) <= 1.1f && std::abs (r) <= 1.1f;
            }
        }
        check (bounded, "LFO: every wave stays within -1..1 at 1.5 kHz");

        lfo.reset (0.0);
        for (int i = 0; i < 48000 + 10; ++i) // one second, plus a few samples to catch the 5th rising zero
        {
            float l, r;
            lfo.tick (5.0f, Lfo::sine, 0.0f, 0.0f, l, r);
            if (prev < 0.0f && l >= 0.0f) ++upCrossings;
            prev = l; lo = std::min (lo, l); hi = std::max (hi, l);
        }
        check (upCrossings == 5, "LFO: 5 Hz sine makes 5 cycles in one second", upCrossings);
        check (lo < -0.99f && hi > 0.99f, "LFO: sine spans the full -1..1 range", hi);

        lfo.reset (0.0);
        float l, r;
        lfo.tick (1.0f, Lfo::sine, 0.0f, 0.25f, l, r); // 90 degrees
        check (std::abs (l) < 0.01f && r > 0.99f, "LFO: Stereo 90 degrees puts the right side a quarter cycle ahead", r);

        lfo.reset (0.0);
        double slowPhase = 0.0;
        for (int i = 0; i < 48000; ++i) { lfo.tick (0.01f, Lfo::sawUp, 0.0f, 0.0f, l, r); }
        slowPhase = lfo.getPhase();
        check (std::abs (slowPhase - 0.01) < 1.0e-4, "LFO: 0.01 Hz moves 1% of a cycle per second", slowPhase);
    }

    // ---- ADSR: attack time, sustain level, release to silence.
    {
        Adsr env;
        env.prepare (kFs);
        env.gate (true);
        int toPeak = 0;
        while (env.tick (0.01f, 0.1f, 0.5f, 0.2f) < 1.0f && toPeak < 48000) ++toPeak;
        check (std::abs (toPeak - 480) <= 2, "ADSR: 10 ms attack reaches the top in 480 samples", toPeak);
        for (int i = 0; i < 24000; ++i) env.tick (0.01f, 0.1f, 0.5f, 0.2f);
        check (std::abs (env.getValue() - 0.5f) < 0.01f, "ADSR: settles at Sustain 50%", env.getValue());
        env.gate (false);
        for (int i = 0; i < 9600; ++i) env.tick (0.01f, 0.1f, 0.5f, 0.2f); // 200 ms
        check (env.getValue() < 0.01f, "ADSR: 200 ms release falls below 1%", env.getValue());
        for (int i = 0; i < 48000; ++i) env.tick (0.01f, 0.1f, 0.5f, 0.2f);
        check (env.getStage() == Adsr::idle, "ADSR: returns to idle after release");
    }

    // ---- Follower and gate: opens on a loud burst, closes after it, with hysteresis.
    {
        EnvelopeFollower follower;
        follower.prepare (kFs);
        FollowerGate gate;
        int opens = 0;
        bool openDuringBurst = false, closedAfter = false;
        for (int i = 0; i < 48000; ++i)
        {
            const float x = (i >= 4800 && i < 9600) ? 0.5f * (float) std::sin (0.1 * i) : 0.0f; // -6 dBFS burst
            const float level = follower.process (x);
            if (gate.update (level, -20.0f)) ++opens;
            if (i == 9000) openDuringBurst = gate.isOpen();
        }
        closedAfter = ! gate.isOpen();
        check (opens == 1 && openDuringBurst && closedAfter, "follower gate: opens once on a burst and closes after", opens);
    }

    // ---- Follower times: hold keeps the peak, release sets the fall.
    {
        auto levelAfter = [] (float holdS, float releaseS, int samplesAfter)
        {
            EnvelopeFollower f;
            f.prepare (kFs);
            f.setTimes (0.001f, holdS, releaseS);
            for (int i = 0; i < 4800; ++i) f.process (0.5f);
            float v = 0.0f;
            for (int i = 0; i < samplesAfter; ++i) v = f.process (0.0f);
            return v;
        };
        check (levelAfter (0.2f, 0.05f, 4800) > 0.49f, "follower: 200 ms hold keeps the level for 100 ms", levelAfter (0.2f, 0.05f, 4800));
        check (levelAfter (0.0f, 0.05f, 4800) < 0.2f, "follower: no hold falls straight away", levelAfter (0.0f, 0.05f, 4800));
        check (levelAfter (0.0f, 1.0f, 4800) > levelAfter (0.0f, 0.05f, 4800) * 2.0f, "follower: longer release falls slower");
    }

    // ---- Smooth random LFO: keeps moving through each new point. (A cosine
    //      ease between points stops dead at the end of every cycle.)
    {
        Lfo lfo;
        lfo.prepare (kFs);
        float l = 0.0f, r = 0.0f, prev = 0.0f;
        double nearEdge = 0.0, overall = 0.0;
        int nearCount = 0, count = 0;
        bool bounded = true;
        for (int i = 0; i < 48000 * 30; ++i)
        {
            const double ph = lfo.getPhase();
            lfo.tick (2.0f, Lfo::smoothRandom, 0.0f, 0.0f, l, r);
            bounded = bounded && std::abs (l) <= 1.0f;
            if (i > 0)
            {
                const double d = std::abs (l - prev);
                overall += d; ++count;
                if (ph < 0.02 || ph > 0.98) { nearEdge += d; ++nearCount; }
            }
            prev = l;
        }
        const double ratio = (nearEdge / nearCount) / (overall / count);
        check (bounded, "smooth random: stays within -1..1");
        check (ratio > 0.4, "smooth random: still moving at the end of each cycle", ratio);
    }

    // ---- Fuzz: bounded, gain rises with Fuzz, low Bias gates quiet signals.
    {
        auto rmsOut = [] (FuzzFace::Params fp, float amp, double hz = 220.0)
        {
            const double fs = kFs * 4.0;
            FuzzFace fz;
            fz.prepare (fs);
            fz.setParams (fp);
            double sum = 0.0;
            float peak = 0.0f;
            const int n = (int) fs;
            for (int i = 0; i < n; ++i)
            {
                const float y = fz.process (amp * (float) std::sin (2.0 * 3.14159265358979 * hz * i / fs));
                if (i > n / 2) { sum += (double) y * y; peak = std::max (peak, std::abs (y)); }
            }
            return std::make_pair (std::sqrt (sum / (n / 2)), peak);
        };

        FuzzFace::Params fp;
        fp.fuzz = 1.0f;
        const auto loud = rmsOut (fp, 4.0f);
        check (std::isfinite (loud.first) && loud.second < 1.5f, "fuzz: +12 dBFS input stays bounded", loud.second);

        fp.fuzz = 0.0f; const auto low = rmsOut (fp, 0.05f);
        fp.fuzz = 1.0f; const auto high = rmsOut (fp, 0.05f);
        check (high.first > low.first * 3.0, "fuzz: Fuzz 100% much louder than 0% on a quiet signal",
               toDb (high.first / low.first));

        check (std::abs (FuzzFace::driveFor (1.0f) / 300.0f - 5.0f) < 0.01f, "fuzz: max drive is 5x the old 300x", FuzzFace::driveFor (1.0f));
        check (FuzzFace::driveFor (0.5f) / (2.0f * std::sqrt (150.0f)) < 1.3f, "fuzz: mid knob drive barely changes", FuzzFace::driveFor (0.5f));
        fp.fuzz = 0.2f; fp.bias = 0.5f; const auto normal = rmsOut (fp, 0.005f);
        fp.bias = 0.0f; const auto gated = rmsOut (fp, 0.005f);
        check (gated.first < normal.first * 0.25, "fuzz: Bias 0% gates a quiet signal", toDb (gated.first / normal.first));

        fp = FuzzFace::Params();
        fp.mix = 0.0f;
        const auto clean = rmsOut (fp, 0.1f, 1000.0);
        check (std::abs (toDb (clean.first / (0.1 / std::sqrt (2.0)))) < 0.5, "fuzz: Mix 0% is the clean signal",
               toDb (clean.first / (0.1 / std::sqrt (2.0))));
    }

    // ---- Input stage: clean at 0 dB, adds level and saturation above.
    {
        const float sat0 = InputStage::saturationAmount (0.0f);
        const float y0 = InputStage::process (0.5f, 1.0f, sat0);
        check (sat0 == 0.0f && y0 == 0.5f, "input: 0 dB is clean", y0);
        const float gain = std::pow (10.0f, 12.0f / 20.0f);
        const float y12 = InputStage::process (0.5f, gain, InputStage::saturationAmount (12.0f));
        check (y12 > 0.5f && y12 < 0.5f * gain, "input: +12 dB adds level but rounds the peak", y12);
    }

    std::printf ("\n%s: %d failed\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
