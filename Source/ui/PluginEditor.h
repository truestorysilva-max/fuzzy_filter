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

    // In an iPad plugin there is no desktop to open a separate menu window
    // on, so menus open inside the plugin's own view.
    juce::Component* getParentComponentForMenuOptions (const juce::PopupMenu::Options& options) override
    {
       #if JUCE_IOS
        if (options.getParentComponent() == nullptr)
            if (auto* target = options.getTargetComponent())
                return target->getTopLevelComponent();
       #endif
        return juce::LookAndFeel_V4::getParentComponentForMenuOptions (options);
    }
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
// The mod grid blown up to fill the editor, for fingers. Opened by tapping
// the grid on a touch screen; DONE or a tap outside the grid closes it.
class GridZoom : public juce::Component
{
public:
    explicit GridZoom (FuzzyFilterProcessor& p);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void refresh() { grid.refresh(); }

    static constexpr int gridWidth = 360, gridHeight = 212; // the grid's own layout size

private:
    juce::Rectangle<int> cardArea() const;

    ModGrid grid;
    juce::TextButton done { "DONE" };
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
    ui::GridZoom* gridZoom = nullptr;
    ui::Segments* lfoTabs = nullptr;
    ui::Segments* envTabs = nullptr;
    ui::Segments* envView = nullptr; // ADSR or Follower

    struct LfoControls { ui::Knob* rate; ui::Knob* division; std::vector<juce::Component*> all; };
    std::array<LfoControls, 4> lfoControls;
    std::array<std::vector<juce::Component*>, 2> envControls;
    std::array<std::vector<juce::Component*>, 2> folControls;
    int currentLfo = 0, currentEnv = 0;

    std::vector<std::pair<int, ui::Knob*>> modKnobs;   // slot, knob
    std::vector<std::pair<int, ui::Fader*>> modFaders; // slot, fader

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FuzzyFilterEditor)
};
