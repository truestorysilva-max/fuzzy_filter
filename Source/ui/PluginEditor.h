#pragma once

#include "../PluginProcessor.h"
#include "Displays.h"
#include "ModGrid.h"
#include "Widgets.h"

namespace ui
{
// Popup menus, tooltips and dialogs in the Cut Paper colours.
class CutPaperLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CutPaperLookAndFeel();
    juce::Font getPopupMenuFont() override { return fonts::label (14.0f, true); }
    juce::Font getAlertWindowTitleFont() override { return fonts::title (22.0f); }
    juce::Font getAlertWindowMessageFont() override { return fonts::label (14.0f, false); }
    juce::Font getAlertWindowFont() override { return fonts::label (14.0f, false); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return fonts::label (14.0f, true, 1.0f); }
};

// The preset strip: previous and next arrows around the preset name, which
// opens the full list plus Save, Delete and the folder.
class PresetBar : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PresetBar (PresetManager& pm);
    void refresh();
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    void showMenu();
    void askToSave();

    PresetManager& presets;
    juce::String shownName;
    bool shownFactory = true;
};
} // namespace ui

class FuzzyFilterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FuzzyFilterEditor (FuzzyFilterProcessor& p);
    ~FuzzyFilterEditor() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

    static constexpr int designWidth = 1000, designHeight = 650;

private:
    void timerCallback() override;
    void buildHeader();
    void buildSoundRow();
    void buildModRow();
    void showLfo (int index);
    void showEnv (int index);
    void updateModulationDisplays();
    std::pair<float, float> modRange (int slot) const;

    // Everything is laid out at the design size inside this component, which
    // is scaled to the window.
    struct Content : juce::Component
    {
        void paint (juce::Graphics& g) override;
    };

    FuzzyFilterProcessor& proc;
    ui::CutPaperLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };
    Content content;
    std::vector<std::unique_ptr<juce::Component>> owned; // everything else on screen

    template <typename T, typename... Args>
    T& add (juce::Rectangle<int> bounds, Args&&... args)
    {
        auto c = std::make_unique<T> (std::forward<Args> (args)...);
        auto& ref = *c;
        content.addAndMakeVisible (ref);
        ref.setBounds (bounds);
        owned.push_back (std::move (c));
        return ref;
    }

    ui::PresetBar* presetBar = nullptr;
    ui::FilterDisplay* filterDisplay = nullptr;
    ui::LfoDisplay* lfoDisplay = nullptr;
    ui::EnvDisplay* envDisplay = nullptr;
    ui::InputMeter* inputMeter = nullptr;
    ui::ModGrid* modGrid = nullptr;
    ui::Segments* lfoTabs = nullptr;
    ui::Segments* envTabs = nullptr;

    struct LfoControls { ui::Knob* rate; ui::Knob* division; std::vector<juce::Component*> all; };
    std::array<LfoControls, 4> lfoControls;
    std::array<std::vector<juce::Component*>, 2> envControls;
    int currentLfo = 0, currentEnv = 0;

    std::vector<std::pair<int, ui::Knob*>> modKnobs;   // slot, knob
    std::vector<std::pair<int, ui::Fader*>> modFaders; // slot, fader

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FuzzyFilterEditor)
};
