cmake_minimum_required(VERSION 3.22)

project(FuzzyFilter VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FF_BUILD_PLUGIN "Build the plugin (downloads JUCE)" ON)

enable_testing()

# DSP tests: plain C++, no JUCE needed. Not built for iOS, where they would
# have to be packaged as an app to run.
if(NOT IOS)
    add_executable(dsp_tests tests/dsp_tests.cpp)
    target_include_directories(dsp_tests PRIVATE Source)
    add_test(NAME dsp_tests COMMAND dsp_tests)
endif()

if(FF_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)

    if(IOS)
        # iPad: an AUv3 extension inside a small standalone app. The App Store
        # only installs plugins that arrive inside an app, and the app doubles
        # as a way to try the effect on the microphone or a connected input.
        set(FF_FORMATS AUv3 Standalone)
    else()
        set(FF_FORMATS VST3)
        if(APPLE)
            list(APPEND FF_FORMATS AU)
        endif()
        # AAX for Pro Tools, built with the AAX SDK that ships inside JUCE. The
        # build is unsigned: Pro Tools Developer loads it as is, while the retail
        # Pro Tools only loads AAX plugins signed with PACE's wraptool.
        if(WIN32 OR APPLE)
            list(APPEND FF_FORMATS AAX)
        endif()
    endif()

    # CFBundleVersion. TestFlight needs a higher one on every upload, so the
    # iOS workflow passes the run number.
    set(FF_BUILD_NUMBER "${PROJECT_VERSION}" CACHE STRING "Bundle build number")

    # iPad app settings: iPad only, any orientation, keep playing in the
    # background, microphone access for the standalone app, and no export
    # compliance question on each TestFlight upload.
    set(FF_IOS_ARGS "")
    if(IOS)
        set(FF_IOS_ARGS
            ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon/FuzzyFilterIcon.png"
            TARGETED_DEVICE_FAMILY 2
            IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
                                     UIInterfaceOrientationPortrait UIInterfaceOrientationPortraitUpsideDown
            REQUIRES_FULL_SCREEN FALSE
            BACKGROUND_AUDIO_ENABLED TRUE
            STATUS_BAR_HIDDEN TRUE
            MICROPHONE_PERMISSION_ENABLED TRUE
            MICROPHONE_PERMISSION_TEXT "Fuzzy Filter processes the sound from your microphone or audio interface."
            PLIST_TO_MERGE "<plist><dict><key>ITSAppUsesNonExemptEncryption</key><false/></dict></plist>")
    endif()

    # Signing team for the iPad build. CI passes it in from the APPLE_TEAM_ID
    # secret; empty means an unsigned build.
    set(FF_IOS_TEAM_ID "" CACHE STRING "Apple developer team ID for the iOS build")

    juce_add_plugin(FuzzyFilter
        PRODUCT_NAME "Fuzzy Filter"
        COMPANY_NAME "Fuzzy Filter"
        BUNDLE_ID com.fuzzyfilter.fuzzyfilter
        PLUGIN_MANUFACTURER_CODE Fzfl
        PLUGIN_CODE Fzf1
        FORMATS ${FF_FORMATS}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES Fx Filter Distortion
        AU_MAIN_TYPE kAudioUnitType_Effect
        AAX_CATEGORY AAX_ePlugInCategory_EQ AAX_ePlugInCategory_Harmonic
        COPY_PLUGIN_AFTER_BUILD FALSE
        BUILD_VERSION "${FF_BUILD_NUMBER}"
        ${FF_IOS_ARGS})

    if(IOS AND FF_IOS_TEAM_ID)
        foreach(t FuzzyFilter_Standalone FuzzyFilter_AUv3)
            set_target_properties(${t} PROPERTIES
                XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${FF_IOS_TEAM_ID}"
                XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic")
        endforeach()
    endif()

    # Fonts for the UI (both SIL Open Font License, see Resources/fonts).
    juce_add_binary_data(FuzzyFilterAssets SOURCES
        Resources/fonts/LondrinaSolid-Regular.ttf
        Resources/fonts/LondrinaSolid-Black.ttf
        Resources/fonts/ArchivoNarrow-Medium.ttf
        Resources/fonts/ArchivoNarrow-Bold.ttf)

    set(FF_SOURCES
        Source/PluginProcessor.cpp
        Source/Parameters.cpp
        Source/PresetManager.cpp
        Source/ui/Style.cpp
        Source/ui/Widgets.cpp
        Source/ui/Displays.cpp
        Source/ui/ModGrid.cpp
        Source/ui/PluginEditor.cpp)

    target_sources(FuzzyFilter PRIVATE ${FF_SOURCES})

    if(IOS)
        # Keep the static libraries out of the archive, or Xcode treats it as
        # a generic archive that can't be exported to the App Store.
        set_target_properties(FuzzyFilter FuzzyFilterAssets PROPERTIES XCODE_ATTRIBUTE_SKIP_INSTALL YES)
    endif()

    target_include_directories(FuzzyFilter PRIVATE Source)

    target_compile_definitions(FuzzyFilter PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(FuzzyFilter
        PRIVATE
            FuzzyFilterAssets
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    # Runs the real processor and editor without a host (bus layouts,
    # outputs, state, presets, UI). Desktop only.
    if(IOS)
        return()
    endif()
    juce_add_console_app(PluginSmokeTest PRODUCT_NAME "PluginSmokeTest")
    target_sources(PluginSmokeTest PRIVATE tests/plugin_smoke_test.cpp ${FF_SOURCES})
    target_include_directories(PluginSmokeTest PRIVATE Source)
    target_compile_definitions(PluginSmokeTest PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JucePlugin_Name="Fuzzy Filter")
    target_link_libraries(PluginSmokeTest
        PRIVATE
            FuzzyFilterAssets
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags)
    add_test(NAME plugin_smoke_test COMMAND PluginSmokeTest)
endif()
