#include "PluginEditor.h"

using namespace ui;

namespace
{
// Small text, e.g. "FUZZ POSITION" or "CHARACTER".
struct SmallLabel : juce::Component
{
    SmallLabel (juce::String t, juce::Colour c, float s, juce::Justification j = juce::Justification::centredLeft)
        : text (std::move (t)), colour (c), size (s), just (j)
    {
        setInterceptsMouseClicks (false, false);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (colour);
        g.setFont (fonts::label (size, true, 2.0f));
        g.drawText (text, getLocalBounds(), just, false);
    }
    juce::String text;
    juce::Colour colour;
    float size;
    juce::Justification just;
};

// The torn red strip across the fuzz panel.
struct JaggedBand : juce::Component
{
    JaggedBand() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override
    {
        static const float pts[][2] = { { 0, .5f }, { .06f, 0 }, { .12f, .7f }, { .18f, .1f }, { .23f, .9f }, { .30f, .2f },
                                        { .36f, 1 }, { .41f, .15f }, { .48f, .85f }, { .54f, 0 }, { .60f, .75f }, { .67f, .25f },
                                        { .72f, 1 }, { .79f, .05f }, { .85f, .8f }, { .91f, .2f }, { .96f, .9f }, { 1, .4f },
                                        { 1, .6f }, { .96f, 1 }, { 0, 1 } };
        const float w = (float) getWidth(), h = (float) getHeight();
        juce::Path p;
        p.startNewSubPath (pts[0][0] * w, pts[0][1] * h);
        for (size_t i = 1; i < std::size (pts); ++i)
            p.lineTo (pts[i][0] * w, pts[i][1] * h);
        p.closeSubPath();
        g.setColour (colours::red);
        g.fillPath (p);
    }
};

const juce::Identifier uiScaleProp ("uiScale");

juce::Rectangle<int> rect (int x, int y, int w, int h) { return { x, y, w, h }; }
} // namespace

// ================================================================ Look and feel

CutPaperLookAndFeel::CutPaperLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, colours::cream);
    setColour (juce::PopupMenu::textColourId, colours::ink);
    setColour (juce::PopupMenu::headerTextColourId, colours::red);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::ink);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::cream);
    setColour (juce::TooltipWindow::backgroundColourId, colours::ink);
    setColour (juce::TooltipWindow::textColourId, colours::cream);
    setColour (juce::TooltipWindow::outlineColourId, colours::ink);
    setColour (juce::AlertWindow::backgroundColourId, colours::cream);
    setColour (juce::AlertWindow::textColourId, colours::ink);
    setColour (juce::AlertWindow::outlineColourId, colours::ink);
    setColour (juce::TextEditor::backgroundColourId, colours::tan);
    setColour (juce::TextEditor::textColourId, colours::ink);
    setColour (juce::TextEditor::outlineColourId, colours::ink);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::red);
    setColour (juce::TextEditor::highlightColourId, colours::red.withAlpha (0.3f));
    setColour (juce::CaretComponent::caretColourId, colours::red);
    setColour (juce::TextButton::buttonColourId, colours::ink);
    setColour (juce::TextButton::textColourOffId, colours::cream);
    setColour (juce::TextButton::buttonOnColourId, colours::red);
    setColour (juce::TextButton::textColourOnId, colours::cream);
}

// ================================================================ Preset bar

PresetBar::PresetBar (PresetManager& pm) : presets (pm)
{
    setTooltip ("Click the name for every preset, Save and Delete. Arrows step through them.");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PresetBar::refresh()
{
    const auto name = presets.getCurrentName();
    const int index = presets.getCurrentIndex();
    const bool factory = index < 0 || presets.getEntry (index).factory;
    if (name != shownName || factory != shownFactory)
    {
        shownName = name;
        shownFactory = factory;
        repaint();
    }
}

void PresetBar::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (colours::ink);
    g.fillPath (cutShape (b, { 0.0f, 0.06f, 0.0f, 0.0f, 0.01f, 0.0f, 0.01f, 0.06f }));

    g.setColour (colours::red);
    drawTriangle (g, b.removeFromLeft (44.0f).withSizeKeepingCentre (18.0f, 18.0f), -1);
    drawTriangle (g, b.removeFromRight (44.0f).withSizeKeepingCentre (18.0f, 18.0f), 1);

    const auto folder = juce::String (shownFactory ? "FACTORY /" : "USER /");
    const auto name = shownName.toUpperCase();
    auto folderFont = fonts::label (12.0f, true, 2.0f);
    auto nameFont = fonts::title (24.0f);
    juce::GlyphArrangement a, n;
    a.addLineOfText (folderFont, folder, 0.0f, 0.0f);
    n.addLineOfText (nameFont, name, 0.0f, 0.0f);
    const float fw = a.getBoundingBox (0, -1, true).getWidth();
    const float nw = std::min (n.getBoundingBox (0, -1, true).getWidth(), b.getWidth() - fw - 20.0f);
    float x = b.getCentreX() - (fw + 8.0f + nw) * 0.5f;
    g.setColour (colours::cream.withAlpha (0.75f));
    g.setFont (folderFont);
    g.drawText (folder, juce::Rectangle<float> (x, b.getY(), fw + 2.0f, b.getHeight()).translated (0.0f, 3.0f),
                juce::Justification::centredLeft, false);
    x += fw + 8.0f;
    g.setColour (colours::cream);
    g.setFont (nameFont);
    g.drawFittedText (name, juce::Rectangle<float> (x, b.getY(), nw + 4.0f, b.getHeight()).toNearestInt(),
                      juce::Justification::centredLeft, 1, 0.8f);
}

void PresetBar::mouseDown (const juce::MouseEvent& e)
{
    if (e.x < 44)
        presets.step (-1);
    else if (e.x > getWidth() - 44)
        presets.step (1);
    else
        showMenu();
    refresh();
}

void PresetBar::showMenu()
{
    presets.refresh();
    const int current = presets.getCurrentIndex();
    juce::PopupMenu factory, user;
    for (int i = 0; i < presets.getNumEntries(); ++i)
    {
        const auto& entry = presets.getEntry (i);
        (entry.factory ? factory : user).addItem (i + 1, entry.name, true, i == current);
    }

    juce::PopupMenu menu;
    menu.addSectionHeader ("Factory");
    for (juce::PopupMenu::MenuItemIterator it (factory); it.next();)
        menu.addItem (it.getItem());
    menu.addSectionHeader ("User");
    if (user.getNumItems() == 0)
        menu.addItem (-100, "(none saved yet)", false);
    for (juce::PopupMenu::MenuItemIterator it (user); it.next();)
        menu.addItem (it.getItem());
    menu.addSeparator();
    static constexpr int saveId = 10000, deleteId = 10001, folderId = 10002;
    menu.addItem (saveId, "Save as...");
    const bool canDelete = current >= 0 && ! presets.getEntry (current).factory;
    menu.addItem (deleteId, "Delete \"" + presets.getCurrentName() + "\"", canDelete);
    menu.addItem (folderId, "Show preset folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safe = juce::Component::SafePointer<PresetBar> (this), current] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;
                            auto& pm = safe->presets;
                            if (result == saveId)
                                safe->askToSave();
                            else if (result == deleteId)
                                pm.deleteUser (current);
                            else if (result == folderId)
                            {
                                PresetManager::getUserFolder().createDirectory();
                                PresetManager::getUserFolder().startAsProcess();
                            }
                            else if (result > 0)
                                pm.load (result - 1);
                            safe->refresh();
                        });
}

void PresetBar::askToSave()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name your preset. It is saved in your user presets.",
                                     juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&getLookAndFeel());
    w->addTextEditor ("name", shownFactory ? juce::String() : shownName, "Name");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create (
                                  [safe = juce::Component::SafePointer<PresetBar> (this), w] (int result)
                                  {
                                      if (safe == nullptr || result != 1)
                                          return;
                                      const auto name = w->getTextEditorContents ("name");
                                      if (! safe->presets.saveUser (name))
                                          juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                                  "Preset not saved",
                                                                                  "Couldn't write the preset file. Check the name and try again.");
                                      safe->refresh();
                                  }),
                        true);
}

// ================================================================ Editor

void FuzzyFilterEditor::Content::paint (juce::Graphics& g)
{
    g.fillAll (colours::cream);

    // "FUZZY FILTER", each letter tipped a little, like cut-out paper.
    static const float rot[] = { -4, 3, -2, 5, -3, 2 };
    static const float dy[] = { 1, -2, 2, -1, 0, 2 };
    auto font = fonts::title (44.0f, true);
    g.setFont (font);
    float x = 14.0f;
    auto word = [&] (const juce::String& text, juce::Colour colour, int seed)
    {
        g.setColour (colour);
        for (int i = 0; i < text.length(); ++i)
        {
            const auto ch = juce::String::charToString (text[i]);
            juce::GlyphArrangement ga;
            ga.addLineOfText (font, ch, 0.0f, 0.0f);
            const float w = ga.getBoundingBox (0, -1, true).getWidth() + 1.0f;
            const int k = (i + seed) % 6;
            juce::Graphics::ScopedSaveState s (g);
            const auto box = juce::Rectangle<float> (x, 14.0f + dy[k], w + 4.0f, 58.0f);
            g.addTransform (juce::AffineTransform::rotation (juce::degreesToRadians (rot[k]), box.getCentreX(), box.getCentreY()));
            g.drawText (ch, box, juce::Justification::centredLeft, false);
            x += w;
        }
    };
    word ("FUZZY", colours::ink, 0);
    x += 10.0f;
    word ("FILTER", colours::red, 3);
}

FuzzyFilterEditor::FuzzyFilterEditor (FuzzyFilterProcessor& p) : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);
    content.setBounds (0, 0, designWidth, designHeight);

    buildHeader();
    buildSoundRow();
    buildModRow();
    showLfo (0);
    showEnv (0);

    const double scale = juce::jlimit (0.65, 1.5, (double) proc.apvts.state.getProperty (uiScaleProp, 1.0));
    setResizable (true, true);
    setResizeLimits (650, 423, 1500, 975);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    setSize (juce::roundToInt (designWidth * scale), juce::roundToInt (designHeight * scale));

    timerCallback();
    startTimerHz (30);
}

FuzzyFilterEditor::~FuzzyFilterEditor()
{
    stopTimer();
    owned.clear();
    setLookAndFeel (nullptr);
}

void FuzzyFilterEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::cream);
}

void FuzzyFilterEditor::resized()
{
    const float scale = (float) getWidth() / (float) designWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    proc.apvts.state.setProperty (uiScaleProp, scale, nullptr);
}

void FuzzyFilterEditor::buildHeader()
{
    auto& s = proc.apvts;
    presetBar = &add<PresetBar> (rect (266, 21, 516, 44), proc.presets);

    add<SmallLabel> (rect (798, 16, 120, 14), "FUZZ POSITION", colours::ink, 10.0f, juce::Justification::centred);
    auto& pos = add<Segments> (rect (802, 32, 112, 28), juce::StringArray { "PRE", "POST" },
                               Segments::Style { colours::red, colours::cream, colours::ink, juce::Colours::transparentBlack, true, 18.0f, true });
    pos.attachTo (*s.getParameter (ParamIDs::fuzzPosition));

    add<BypassButton> (rect (934, 17, 52, 52), s, ParamIDs::bypass);

   #ifndef FF_BUILD_ID
    #define FF_BUILD_ID "local"
   #endif
    add<SmallLabel> (rect (798, 64, 120, 10), juce::String ("build ") + FF_BUILD_ID,
                     colours::ink, 8.0f, juce::Justification::centred);
}

void FuzzyFilterEditor::buildSoundRow()
{
    auto& s = proc.apvts;
    const int top = 82, h = 318;
    using LL = Knob::LabelLayout;

    // ---- Input
    {
        const int x = 14, w = 100;
        add<CutPanel> (rect (x, top, w, h), schemes::onRed, std::array<float, 8> { 0, .01f, 0, 0, .03f, 0, .03f, .01f });
        add<TitleLabel> (rect (x, top + 14, w, 32), "IN", colours::cream, 30.0f).setBounds (x + 36, top + 14, 50, 32);
        inputMeter = &add<InputMeter> (rect (x + 10, top + 56, w - 20, 110), proc);
        auto& gain = add<Knob> (rect (x + 6, top + 180, w - 12, 96), s, ParamIDs::inputGain, schemes::onRed, "GAIN", 64.0f, LL::twoLine);
        modKnobs.push_back ({ Mod::inputGain, &gain });
    }

    // ---- Fuzz
    {
        const int x = 124, w = 270, px = x + 16, iw = w - 32;
        add<CutPanel> (rect (x, top, w, h), schemes::onInk, std::array<float, 8> { 0, 0, .01f, .01f, 0, .01f, .01f, 0 });
        add<TitleLabel> (rect (px, top + 14, 120, 34), "FUZZ", colours::cream, 34.0f);
        add<ToggleTag> (rect (px + iw - 54, top + 18, 54, 26), s, ParamIDs::fuzzOn, colours::red, colours::cream, colours::cream);
        add<JaggedBand> (rect (x, top + 54, w, 30));

        const char* ids[] = { ParamIDs::fuzz, ParamIDs::fuzzVolume, ParamIDs::fuzzBias, ParamIDs::fuzzMix };
        const char* labels[] = { "FUZZ", "VOLUME", "BIAS", "MIX" };
        const int slotsFor[] = { Mod::fuzz, Mod::fuzzVolume, Mod::fuzzBias, Mod::fuzzMix };
        for (int i = 0; i < 4; ++i)
        {
            const int col = i % 2, row = i / 2;
            auto& k = add<Knob> (rect (px + col * (iw / 2), top + 96 + row * 88, iw / 2, 84), s, ids[i], schemes::onInk, labels[i], 64.0f);
            modKnobs.push_back ({ slotsFor[i], &k });
        }

        add<SmallLabel> (rect (px, top + h - 40, 84, 26), "CHARACTER", colours::cream.withAlpha (0.8f), 10.0f);
        auto& ch = add<Segments> (rect (px + 84, top + h - 40, iw - 84, 26), juce::StringArray { "GERMANIUM", "SILICON" },
                                  Segments::Style { colours::cream, colours::ink, colours::cream, colours::cream.withAlpha (0.4f), false, 12.0f, false });
        ch.attachTo (*s.getParameter (ParamIDs::fuzzCharacter));
    }

    // ---- Filter
    {
        const int x = 404, w = 330, px = x + 14, iw = w - 28;
        add<CutPanel> (rect (x, top, w, h), schemes::onCream, std::array<float, 8> { .01f, 0, 0, .01f, .01f, 0, 0, .01f });
        add<TitleLabel> (rect (px, top + 12, 140, 34), "FILTER", colours::ink, 34.0f);
        add<ValueBox> (rect (px + iw - 84, top + 18, 84, 24), s, ParamIDs::combDamping, "DAMP", colours::ink);
        filterDisplay = &add<FilterDisplay> (rect (px, top + 52, iw, 84), proc);

        const char* ids[] = { ParamIDs::frequency, ParamIDs::spaceL, ParamIDs::spaceR, ParamIDs::q, ParamIDs::excite1, ParamIDs::excite2 };
        const char* labels[] = { "FREQ", "SPACE L", "SPACE R", "REZ", "EXCITE 1", "EXCITE 2" };
        const int slotsFor[] = { Mod::frequency, Mod::spaceL, Mod::spaceR, Mod::q, Mod::excite1, Mod::excite2 };
        const int colW = iw / 3;
        for (int i = 0; i < 6; ++i)
        {
            const int col = i % 3, row = i / 3;
            auto& k = add<Knob> (rect (px + col * colW, top + 144 + row * 76, colW, 70), s, ids[i], schemes::onCream, labels[i], 50.0f);
            modKnobs.push_back ({ slotsFor[i], &k });
        }
        for (int e = 0; e < 2; ++e)
            add<ChoiceMenuButton> (rect (px + (1 + e) * colW + 6, top + 290, colW - 12, 17), s,
                                   e == 0 ? ParamIDs::excite1Source : ParamIDs::excite2Source, "", colours::ink, colours::cream)
                .setShortNames ({ "Input", "Sidechain", "Grid only" });
        add<SmallLabel> (rect (px, top + 290, colW, 17), "EXCITE FROM", colours::ink, 9.0f, juce::Justification::centredRight);
    }

    // ---- Output
    {
        const int x = 744, w = 242, px = x + 14, iw = w - 28;
        add<CutPanel> (rect (x, top, w, h), schemes::onInk, std::array<float, 8> { 0, .01f, 0, 0, .01f, .01f, .02f, 0 });
        add<TitleLabel> (rect (px, top + 14, 70, 34), "OUT", colours::cream, 34.0f);

        const char* ids[] = { ParamIDs::levelLP, ParamIDs::levelBP, ParamIDs::levelHP, ParamIDs::levelNP, ParamIDs::levelComb };
        const char* labels[] = { "LP", "BP", "HP", "NP", "CMB" };
        const int fw = 42;
        const float gap = (float) (iw - 5 * fw) / 4.0f;
        for (int i = 0; i < 5; ++i)
        {
            auto& f = add<Fader> (rect (px + juce::roundToInt ((float) i * (fw + gap)), top + 56, fw, 196), s, ids[i], schemes::onInk, labels[i]);
            modFaders.push_back ({ Mod::levelLP + i, &f });
        }
        auto& mix = add<Knob> (rect (px, top + h - 58, 150, 44), s, ParamIDs::filterMix, schemes::onInk, "FILTER MIX", 44.0f, LL::right);
        modKnobs.push_back ({ Mod::filterMix, &mix });
    }
}

void FuzzyFilterEditor::buildModRow()
{
    auto& s = proc.apvts;
    const int top = 410, h = 226;
    using LL = Knob::LabelLayout;

    // ---- LFOs
    {
        const int x = 14, w = 300, px = x + 14, iw = w - 28;
        add<CutPanel> (rect (x, top, w, h), schemes::onInk, std::array<float, 8> { 0, .02f, 0, 0, .01f, 0, .01f, .02f });
        add<TitleLabel> (rect (px, top + 10, 80, 26), "LFO", colours::cream, 26.0f);
        lfoTabs = &add<Segments> (rect (px + iw - 120, top + 10, 120, 26), juce::StringArray { "1", "2", "3", "4" },
                                  Segments::Style { colours::red, colours::cream, colours::cream, juce::Colours::transparentBlack, true, 18.0f, false });
        lfoTabs->onChange = [this] (int i) { showLfo (i); };
        lfoDisplay = &add<LfoDisplay> (rect (px, top + 44, iw, 58), proc);

        const int knobArea = iw - 88, colW = knobArea / 3, ky = top + 112;
        for (int l = 0; l < 4; ++l)
        {
            auto& c = lfoControls[(size_t) l];
            c.rate = &add<Knob> (rect (px, ky, colW, 76), s, ModParamIds::lfo (l + 1, "Rate"), schemes::onInk, "RATE", 44.0f, LL::twoLine);
            c.division = &add<Knob> (rect (px, ky, colW, 76), s, ModParamIds::lfo (l + 1, "Division"), schemes::onInk, "DIV", 44.0f, LL::twoLine);
            auto& phase = add<Knob> (rect (px + colW, ky, colW, 76), s, ModParamIds::lfo (l + 1, "Phase"), schemes::onInk, "PHASE", 44.0f, LL::twoLine);
            auto& stereo = add<Knob> (rect (px + 2 * colW, ky, colW, 76), s, ModParamIds::lfo (l + 1, "Stereo"), schemes::onInk, "STEREO", 44.0f, LL::twoLine);
            const int bx = px + iw - 84;
            auto& wave = add<ChoiceMenuButton> (rect (bx, ky, 84, 22), s, ModParamIds::lfo (l + 1, "Wave"), "", colours::cream, colours::ink);
            auto& sync = add<ChoiceMenuButton> (rect (bx, ky + 27, 84, 22), s, ModParamIds::lfo (l + 1, "Sync"), "", juce::Colours::transparentBlack,
                                                colours::cream, colours::cream.withAlpha (0.4f));
            auto& retrig = add<ChoiceMenuButton> (rect (px, ky + 80, iw, 22), s, ModParamIds::lfo (l + 1, "Retrigger"), "RESTART",
                                                  juce::Colours::transparentBlack, colours::cream, colours::cream.withAlpha (0.4f));
            retrig.setShortNames ({ "Off", "When Env 1 fires", "When Env 2 fires" });
            retrig.setMenuNote ("Restart the LFO from its Phase when an envelope fires");
            retrig.setTooltip ("Restart: jumps this LFO back to its Phase each time the chosen envelope fires. "
                               "An envelope fires when its follower goes above THR in the Env panel.");
            c.all = { c.rate, c.division, &phase, &stereo, &wave, &sync, &retrig };
            modKnobs.push_back ({ Mod::lfo1Rate + l, c.rate });
        }
    }

    // ---- Envelopes
    {
        const int x = 324, w = 270, px = x + 14, iw = w - 28;
        add<CutPanel> (rect (x, top, w, h), schemes::onRed, std::array<float, 8> { .01f, 0, 0, .02f, .02f, 0, 0, .01f });
        add<TitleLabel> (rect (px, top + 10, 80, 26), "ENV", colours::cream, 26.0f);
        envTabs = &add<Segments> (rect (px + iw - 60, top + 10, 60, 26), juce::StringArray { "1", "2" },
                                  Segments::Style { colours::ink, colours::cream, colours::cream, juce::Colours::transparentBlack, true, 18.0f, false });
        envTabs->onChange = [this] (int i) { showEnv (i); };
        envView = &add<Segments> (rect (px + 56, top + 12, 122, 22), juce::StringArray { "ADSR", "FOLLOWER" },
                                  Segments::Style { colours::cream, colours::red, colours::cream, colours::cream.withAlpha (0.5f), false, 10.0f, false });
        envView->onChange = [this] (int) { showEnv (currentEnv); };
        envDisplay = &add<EnvDisplay> (rect (px, top + 44, iw, 58), proc);

        const char* suffixes[] = { "Attack", "Decay", "Sustain", "Release", "Threshold", "Sensitivity" };
        const char* labels[] = { "ATK", "DEC", "SUS", "REL", "THR", "SENS" };
        const int colW = iw / 6;
        for (int e = 0; e < 2; ++e)
        {
            auto& list = envControls[(size_t) e];
            for (int i = 0; i < 6; ++i)
            {
                auto& k = add<Knob> (rect (px + i * colW - 3, top + 110, colW + 6, 68), s, ModParamIds::env (e + 1, suffixes[i]),
                                     schemes::onRed, labels[i], 36.0f, LL::twoLine);
                list.push_back (&k);
                const int base = e == 0 ? Mod::env1Attack : Mod::env2Attack;
                if (i < 4) modKnobs.push_back ({ base + i, &k });
                if (i == 4) modKnobs.push_back ({ e == 0 ? Mod::env1Threshold : Mod::env2Threshold, &k });
            }
            auto& source = add<ChoiceMenuButton> (rect (px, top + 186, 190, 22), s, ModParamIds::env (e + 1, "Trigger"),
                                                  "FIRED BY", colours::ink, colours::cream);
            source.setShortNames ({ "Follower", "MIDI notes", "Audio" });
            list.push_back (&source);

            // The follower view: how the follower rises, holds and falls,
            // plus the controls it shares with the envelope view.
            auto& fol = folControls[(size_t) e];
            const char* folSuffixes[] = { "FolAttack", "FolHold", "FolRelease", "Threshold", "Sensitivity" };
            const char* folLabels[] = { "ATK", "HOLD", "REL", "THR", "SENS" };
            const int folW = iw / 5;
            for (int i = 0; i < 5; ++i)
            {
                auto& k = add<Knob> (rect (px + i * folW - 3, top + 110, folW + 6, 68), s, ModParamIds::env (e + 1, folSuffixes[i]),
                                     schemes::onRed, folLabels[i], 36.0f, LL::twoLine);
                fol.push_back (&k);
                if (i == 3) modKnobs.push_back ({ e == 0 ? Mod::env1Threshold : Mod::env2Threshold, &k });
            }
            auto& folSource = add<ChoiceMenuButton> (rect (px, top + 186, 190, 22), s, ModParamIds::env (e + 1, "Source"),
                                                     "LISTENS TO", colours::ink, colours::cream);
            folSource.setShortNames ({ "Pre gain", "Post gain", "Sidechain" });
            fol.push_back (&folSource);
        }
    }

    // ---- Mod grid
    {
        const int x = 604, w = 382;
        add<CutPanel> (rect (x, top, w, h), schemes::onCream, std::array<float, 8> { 0, 0, 0, .02f, .01f, 0, .01f, .01f });
        modGrid = &add<ModGrid> (rect (x + 12, top + 10, w - 24, h - 20), proc);
    }
}

void FuzzyFilterEditor::showLfo (int index)
{
    currentLfo = index;
    lfoTabs->setSelected (index);
    lfoDisplay->setLfo (index);
    for (int l = 0; l < 4; ++l)
        for (auto* c : lfoControls[(size_t) l].all)
            c->setVisible (l == index);
    timerCallback(); // picks Rate or Division
}

void FuzzyFilterEditor::showEnv (int index)
{
    currentEnv = index;
    envTabs->setSelected (index);
    const bool follower = envView->getSelected() == 1;
    envDisplay->setEnv (index);
    envDisplay->setFollowerView (follower);
    for (int e = 0; e < 2; ++e)
    {
        for (auto* c : envControls[(size_t) e])
            c->setVisible (e == index && ! follower);
        for (auto* c : folControls[(size_t) e])
            c->setVisible (e == index && follower);
    }
}

std::pair<float, float> FuzzyFilterEditor::modRange (int slot) const
{
    float lo = 0.0f, hi = 0.0f;
    auto amount = [this] (int src, int dst) { return proc.apvts.getRawParameterValue (ModParamIds::amount (src, dst))->load(); };
    auto addColumn = [&] (int col)
    {
        for (int src = 0; src < Mod::numSources; ++src)
        {
            const float a = amount (src, col);
            if (juce::exactlyEqual (a, 0.0f))
                continue;
            if (src <= Mod::lfo4) { lo -= std::abs (a); hi += std::abs (a); } // LFOs swing both ways
            else { lo += std::min (0.0f, a); hi += std::max (0.0f, a); }     // envelopes and followers go one way
        }
    };

    if (slot < Mod::numFixedDestinations)
    {
        addColumn (slot);
        if (slot == Mod::spaceR)
        {
            bool own = false;
            for (int src = 0; src < Mod::numSources; ++src)
                own = own || ! juce::exactlyEqual (amount (src, Mod::spaceR), 0.0f);
            if (! own)
                addColumn (Mod::spaceL); // Space R follows Space L, as on the QPAS
        }
    }
    for (int a = 0; a < Mod::numAssigns; ++a)
    {
        const int choice = juce::roundToInt (proc.apvts.getRawParameterValue (Mod::assignTargetIds[(size_t) a])->load());
        if (Mod::assignChoiceToSlot (choice) == slot)
            addColumn (Mod::numFixedDestinations + a);
    }

    if (juce::exactlyEqual (lo, 0.0f) && juce::exactlyEqual (hi, 0.0f))
        return { -1.0f, -1.0f };
    const float base = proc.apvts.getParameter (Mod::slotParamIds[(size_t) slot])->getValue();
    return { juce::jlimit (0.0f, 1.0f, base + lo), juce::jlimit (0.0f, 1.0f, base + hi) };
}

void FuzzyFilterEditor::updateModulationDisplays()
{
    for (auto& [slot, knob] : modKnobs)
    {
        if (! knob->isVisible())
            continue;
        const auto [lo, hi] = modRange (slot);
        knob->setModulation (lo, hi, lo >= 0.0f ? proc.getSlotLive (slot) : -1.0f);
    }
    for (auto& [slot, fader] : modFaders)
    {
        const auto [lo, hi] = modRange (slot);
        fader->setModulation (lo, hi, lo >= 0.0f ? proc.getSlotLive (slot) : -1.0f);
    }
}

void FuzzyFilterEditor::timerCallback()
{
    // Rate or Division, depending on the LFO's Sync setting.
    const bool synced = proc.apvts.getRawParameterValue (ModParamIds::lfo (currentLfo + 1, "Sync"))->load() > 0.5f;
    auto& c = lfoControls[(size_t) currentLfo];
    c.rate->setVisible (! synced);
    c.division->setVisible (synced);

    updateModulationDisplays();
    presetBar->refresh();
    filterDisplay->refresh();
    lfoDisplay->refresh();
    envDisplay->refresh();
    inputMeter->refresh();
    modGrid->refresh();
}
