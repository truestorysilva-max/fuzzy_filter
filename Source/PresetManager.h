#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Factory presets (built in) and user presets (.ffpreset files on disk).
// A preset holds every parameter, mod grid included. Loading one sets each
// parameter through the host, so automation and undo see the change.
class PresetManager
{
public:
    struct Entry
    {
        juce::String name;
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;
    };

    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    static juce::File getUserFolder();
    static constexpr const char* fileExtension = ".ffpreset";

    // Rebuilds the list: factory presets first, then user presets A to Z.
    void refresh();
    int getNumEntries() const { return entries.size(); }
    const Entry& getEntry (int index) const { return entries.getReference (index); }

    // Index of the loaded preset, or -1 if it has been deleted or renamed.
    int getCurrentIndex() const;
    juce::String getCurrentName() const;

    void load (int index);
    void step (int delta); // previous (-1) or next (+1), wrapping around

    // Saves the current settings as a user preset, overwriting one with the
    // same name. Returns false if the file can't be written.
    bool saveUser (const juce::String& name);
    bool deleteUser (int index);

    static int getNumFactoryPresets();
    static juce::String getFactoryName (int index);

private:
    void applyValues (const std::function<float (juce::RangedAudioParameter&)>& plainValueFor);
    void setCurrent (const juce::String& name, bool factory);

    juce::AudioProcessorValueTreeState& apvts;
    juce::Array<Entry> entries;
};
