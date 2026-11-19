// Offline checks for the DSP in Source/dsp. No JUCE and no test framework:
// build and run, it prints each check and exits non-zero if any fail.

#include "dsp/FuzzFace.h"
#include "dsp/InputStage.h"
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
