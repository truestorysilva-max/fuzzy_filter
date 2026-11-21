#pragma once

#include "../PluginProcessor.h"
#include "Style.h"

namespace ui
{
// The mod grid: 8 source rows by 14 destination columns. Click a crossing to
// select it and drag up or down to set its amount; double-click clears it.
// Red dots are positive amounts, black bars negative. Click an Assign column's
// header (A1 to A3) to choose what it controls.
//
// On a touch screen the cells are too small for a finger, so when onTouchTap
// is set a tap only calls it (the editor then opens a full size copy of the
// grid, which works with touch as described above: tap, drag, double-tap).
class ModGrid : public juce::Component, public juce::TooltipClient
{
public:
    explicit ModGrid (FuzzyFilterProcessor& p);

    void refresh(); // from the editor's timer: amounts can change from automation
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    juce::String getTooltip() override;

    std::function<void()> onTouchTap;
    float dragPixelsPerUnit = 150.0f; // drag distance for a change of 1 (100%)

private:
    struct Cell { int row = -1, col = -1; };
    Cell cellAt (juce::Point<int> p) const;
    int assignHeaderAt (juce::Point<int> p) const; // -1 when not on an A1-A3 header
    juce::Rectangle<float> cellBounds (int row, int col) const;
    juce::RangedAudioParameter& amountParam (int row, int col) const;
    juce::String destinationName (int col, bool full) const;
    void showAssignMenu (int assign);
    void setAmount (int row, int col, float amount);

    FuzzyFilterProcessor& proc;
    Cell selected { 0, Mod::frequency };
    float dragStart = 0.0f;
    bool dragging = false;
    std::array<float, Mod::numSources * Mod::numDestinations> lastAmounts {};
    std::array<int, Mod::numAssigns> lastTargets {};
    std::array<float, Mod::numSources> lastSources {};

    juce::Rectangle<float> gridArea, readoutArea;
    float labelWidth = 44.0f, cellW = 22.0f, cellH = 19.0f, headerH = 14.0f;
};
} // namespace ui
