#pragma once

#include "DspUtils.h"

namespace ff
{
// Input gain with a gentle saturation that fades in above 0 dB.
// At 0 dB and below the stage is clean; at +24 dB about 60% of the signal
// goes through a tanh curve, which adds harmonics and rounds off peaks.
class InputStage
{
public:
    static float saturationAmount (float gainDb)
    {
        return 0.6f * clampf (gainDb / 24.0f, 0.0f, 1.0f);
    }

    static float process (float x, float gain, float satAmount)
    {
        const float y = x * gain;
        if (satAmount <= 0.0f)
            return y;
        return (1.0f - satAmount) * y + satAmount * std::tanh (y);
    }
};
} // namespace ff
