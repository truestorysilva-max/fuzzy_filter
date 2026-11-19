#pragma once

#include "../PluginProcessor.h"
#include "Style.h"

namespace ui
{
// A parameter's value right now in its own units: the modulated value when
// the grid is moving it, otherwise the knob position.
float currentPlain (FuzzyFilterProcessor& p, int slot);

// The filter's response, left side filled red and right side outlined in
// black, with the current Frequency marked. Follows modulation live.
class FilterDisplay : public juce::Component
{
public:
    explicit FilterDisplay (FuzzyFilterProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
    void refresh(); // called by the editor's timer; repaints only on change
    void paint (juce::Graphics& g) override;

private:
    FuzzyFilterProcessor& proc;
    std::array<float, 12> last {};
};

// One cycle of an LFO's wave, with markers where the left and right
// channels are right now.
class LfoDisplay : public juce::Component
{
public:
    explicit LfoDisplay (FuzzyFilterProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
    void setLfo (int index) { lfo = index; repaint(); }
    void refresh() { repaint(); }
    void paint (juce::Graphics& g) override;

private:
    FuzzyFilterProcessor& proc;
    int lfo = 0;
};

// An envelope's ADSR shape, its follower threshold and the live follower level.
class EnvDisplay : public juce::Component
{
public:
    explicit EnvDisplay (FuzzyFilterProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
    void setEnv (int index) { env = index; repaint(); }
    void refresh() { repaint(); }
    void paint (juce::Graphics& g) override;

private:
    FuzzyFilterProcessor& proc;
    int env = 0;
};

// Two input level bars (left and right) after the input gain.
class InputMeter : public juce::Component
{
public:
    explicit InputMeter (FuzzyFilterProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
    void refresh();
    void paint (juce::Graphics& g) override;

private:
    FuzzyFilterProcessor& proc;
    float level[2] {};
};
} // namespace ui
