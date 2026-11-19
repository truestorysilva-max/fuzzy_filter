#include "PresetManager.h"
#include "ModDefs.h"
#include "Parameters.h"

namespace
{
using Values = std::vector<std::pair<juce::String, float>>; // parameter ID, plain value

struct FactoryPreset
{
    const char* name;
    Values values; // anything not listed is at its default
};

juce::String amt (int source, int destination) { return ModParamIds::amount (source, destination); }

// The Assign target choice that points at a slot.
float assignChoiceFor (int slot) { return (float) (slot - Mod::numFixedDestinations + 1); }

const std::vector<FactoryPreset>& factoryPresets()
{
    using namespace ParamIDs;
    static const std::vector<FactoryPreset> presets {
        { "Init", {} },

        { "Sputter Peaks", {
            { fuzz, 0.75f }, { fuzzBias, 0.18f }, { q, 0.7f }, { frequency, 900.0f },
            { spaceL, 0.35f }, { spaceR, 0.55f }, { levelBP, -6.0f },
            { ModParamIds::lfo (1, "Rate"), 0.4f }, { amt (Mod::lfo1, Mod::frequency), 0.25f } } },

        { "Vowel Drift", {
            { fuzzOn, 0.0f }, { q, 0.82f }, { frequency, 700.0f }, { spaceL, 0.5f }, { spaceR, 0.62f },
            { levelLP, kLevelOffDb }, { levelBP, 0.0f },
            { ModParamIds::lfo (1, "Rate"), 0.15f }, { ModParamIds::lfo (1, "Stereo"), 90.0f },
            { ModParamIds::lfo (2, "Rate"), 0.11f }, { ModParamIds::lfo (2, "Wave"), 1.0f },
            { amt (Mod::lfo1, Mod::spaceL), 0.35f }, { amt (Mod::lfo2, Mod::spaceR), 0.3f } } },

        { "Stereo Swirl", {
            { fuzz, 0.5f }, { q, 0.6f }, { frequency, 1200.0f }, { levelBP, -9.0f },
            { ModParamIds::lfo (1, "Rate"), 0.8f }, { ModParamIds::lfo (1, "Stereo"), 90.0f },
            { amt (Mod::lfo1, Mod::frequency), 0.3f } } },

        { "Envelope Wah", {
            { fuzz, 0.6f }, { q, 0.75f }, { frequency, 350.0f }, { levelLP, kLevelOffDb }, { levelBP, 0.0f },
            { ModParamIds::env (1, "Threshold"), -36.0f }, { ModParamIds::env (1, "Attack"), 0.005f },
            { ModParamIds::env (1, "Decay"), 0.25f }, { ModParamIds::env (1, "Sustain"), 0.3f },
            { ModParamIds::env (1, "Release"), 0.2f }, { amt (Mod::env1, Mod::frequency), 0.55f } } },

        { "Sidechain Pluck", {
            { fuzzOn, 0.0f }, { q, 0.92f }, { frequency, 220.0f }, { spaceL, 0.4f }, { spaceR, 0.5f },
            { excite1, 0.8f }, { excite1Source, (float) Choices::sidechain },
            { excite2, 0.6f }, { excite2Source, (float) Choices::sidechain },
            { levelLP, -12.0f }, { levelComb, -6.0f }, { combDamping, 0.45f } } },

        { "Comb Shimmer", {
            { fuzz, 0.4f }, { q, 0.85f }, { frequency, 440.0f }, { spaceL, 0.2f }, { spaceR, 0.3f },
            { levelLP, kLevelOffDb }, { levelComb, 0.0f }, { combDamping, 0.25f }, { filterMix, 0.7f },
            { ModParamIds::lfo (1, "Rate"), 0.07f }, { amt (Mod::lfo1, Mod::frequency), 0.08f } } },

        { "Silicon Scream", {
            { fuzzPosition, (float) Choices::postFilter }, { fuzzCharacter, (float) Choices::silicon },
            { fuzz, 1.0f }, { fuzzVolume, -12.0f }, { q, 0.55f }, { frequency, 1800.0f },
            { levelLP, kLevelOffDb }, { levelBP, 0.0f },
            { ModParamIds::lfo (1, "Rate"), 5.0f }, { amt (Mod::lfo1, Mod::frequency), 0.1f } } },

        { "Tempo Gate", {
            { fuzz, 0.6f }, { q, 0.5f },
            { ModParamIds::lfo (1, "Sync"), (float) Choices::tempo }, { ModParamIds::lfo (1, "Division"), 9.0f },
            { ModParamIds::lfo (1, "Wave"), 4.0f },
            { ModParamIds::lfo (2, "Sync"), (float) Choices::tempo }, { ModParamIds::lfo (2, "Division"), 3.0f },
            { ModParamIds::lfo (2, "Wave"), 3.0f },
            { Mod::assignTargetIds[0], assignChoiceFor (Mod::levelLP) },
            { amt (Mod::lfo1, Mod::numFixedDestinations), -0.8f }, { amt (Mod::lfo2, Mod::frequency), 0.3f } } },

        { "Notch Sweep", {
            { fuzz, 0.55f }, { q, 0.4f }, { levelLP, kLevelOffDb }, { levelNP, 0.0f },
            { ModParamIds::lfo (1, "Rate"), 0.25f }, { ModParamIds::lfo (1, "Wave"), 1.0f },
            { amt (Mod::lfo1, Mod::frequency), 0.4f } } },

        { "Audio Rate Growl", {
            { fuzz, 0.85f }, { q, 0.7f }, { frequency, 600.0f },
            { ModParamIds::lfo (1, "Rate"), 110.0f }, { ModParamIds::lfo (1, "Stereo"), 30.0f },
            { amt (Mod::lfo1, Mod::frequency), 0.15f } } },

        { "Fat Low", {
            { fuzz, 0.85f }, { fuzzBias, 0.4f }, { q, -0.5f }, { frequency, 180.0f }, { levelHP, -12.0f } } },
    };
    return presets;
}

const juce::Identifier presetNameProp ("presetName");
const juce::Identifier presetFactoryProp ("presetFactory");
} // namespace

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    refresh();
}

juce::File PresetManager::getUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Fuzzy Filter").getChildFile ("Presets");
}

int PresetManager::getNumFactoryPresets() { return (int) factoryPresets().size(); }
juce::String PresetManager::getFactoryName (int index) { return factoryPresets()[(size_t) index].name; }

void PresetManager::refresh()
{
    entries.clear();
    for (int i = 0; i < getNumFactoryPresets(); ++i)
        entries.add ({ getFactoryName (i), true, i, {} });

    auto files = getUserFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()) < 0; });
    for (auto& f : files)
        entries.add ({ f.getFileNameWithoutExtension(), false, -1, f });
}

juce::String PresetManager::getCurrentName() const
{
    return apvts.state.getProperty (presetNameProp, "Init").toString();
}

int PresetManager::getCurrentIndex() const
{
    const auto name = getCurrentName();
    const bool factory = apvts.state.getProperty (presetFactoryProp, true);
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].factory == factory && entries[i].name == name)
            return i;
    return -1;
}

void PresetManager::setCurrent (const juce::String& name, bool factory)
{
    apvts.state.setProperty (presetNameProp, name, nullptr);
    apvts.state.setProperty (presetFactoryProp, factory, nullptr);
}

void PresetManager::applyValues (const std::function<float (juce::RangedAudioParameter&)>& plainValueFor)
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (ranged->paramID == ParamIDs::bypass)
                continue; // a preset never bypasses the plugin
            const float target = ranged->convertTo0to1 (plainValueFor (*ranged));
            if (std::abs (ranged->getValue() - target) > 1.0e-6f)
                ranged->setValueNotifyingHost (target);
        }
}

void PresetManager::load (int index)
{
    if (! juce::isPositiveAndBelow (index, entries.size()))
        return;

    const auto entry = entries[index];
    if (entry.factory)
    {
        const auto& values = factoryPresets()[(size_t) entry.factoryIndex].values;
        applyValues ([&] (juce::RangedAudioParameter& p)
        {
            for (auto& [id, v] : values)
                if (id == p.paramID)
                    return v;
            return p.convertFrom0to1 (p.getDefaultValue());
        });
    }
    else
    {
        auto xml = juce::XmlDocument::parse (entry.file);
        if (xml == nullptr)
            return;
        applyValues ([&] (juce::RangedAudioParameter& p)
        {
            if (auto* e = xml->getChildByAttribute ("id", p.paramID))
                return (float) e->getDoubleAttribute ("value");
            return p.convertFrom0to1 (p.getDefaultValue());
        });
    }
    setCurrent (entry.name, entry.factory);
}

void PresetManager::step (int delta)
{
    if (entries.isEmpty())
        return;
    int i = getCurrentIndex();
    if (i < 0)
        i = delta > 0 ? -1 : 0;
    load ((i + delta + entries.size()) % entries.size());
}

bool PresetManager::saveUser (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;

    juce::XmlElement xml ("FuzzyFilterPreset");
    xml.setAttribute ("name", name);
    xml.setAttribute ("version", 1);
    for (auto* p : apvts.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (ranged->paramID == ParamIDs::bypass)
                continue;
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", ranged->paramID);
            e->setAttribute ("value", ranged->convertFrom0to1 (ranged->getValue()));
        }

    auto folder = getUserFolder();
    if (! folder.createDirectory())
        return false;
    if (! xml.writeTo (folder.getChildFile (name + fileExtension)))
        return false;

    refresh();
    setCurrent (name, false);
    return true;
}

bool PresetManager::deleteUser (int index)
{
    if (! juce::isPositiveAndBelow (index, entries.size()) || entries[index].factory)
        return false;
    const bool ok = entries[index].file.deleteFile();
    refresh();
    return ok;
}
