cmake_minimum_required(VERSION 3.22)

project(SolarStudio VERSION 2.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS deployment version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build macOS Universal Binary")
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Embedded Binary Assets (SolarStudio Logo used throughout UI & icons)
# ---------------------------------------------------------------------------
juce_add_binary_data(SolarStudioAssets
    HEADER_NAME SolarStudioAssets.h
    NAMESPACE   SolarAssets
    SOURCES
        Assets/SolarStudio_logo.png
        Assets/SolarStudio_logo_512.png
        Assets/SolarStudio_logo_256.png
        Assets/SolarStudio_logo_128.png
        Assets/SolarStudio_logo_64.png
)

# ---------------------------------------------------------------------------
# Target 1: SolarStudio Standalone AI DAW Application
# ---------------------------------------------------------------------------
juce_add_gui_app(SolarStudio
    COMPANY_NAME        "SolarAudio"
    PRODUCT_NAME        "SolarStudio"
    BUNDLE_ID           "com.solaraudio.solarstudio"
    VERSION             2.1.0
    ICON_BIG            "${CMAKE_CURRENT_SOURCE_DIR}/Assets/SolarStudio_logo_512.png"
    ICON_SMALL          "${CMAKE_CURRENT_SOURCE_DIR}/Assets/SolarStudio_logo_128.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "SolarStudio uses your microphone for vocal recording, voice cloning, and voice-controlled AI Agent commands."
)

target_sources(SolarStudio
    PRIVATE
        Source/Main.cpp
        Source/Core/ProjectModel.h
        Source/Core/AIBackendBridge.h
        Source/Core/AudioEngine.h
        Source/Engines/VocalEngine.h
        Source/Engines/InstrumentEngine.h
        Source/Engines/GenerativeEngine.h
        Source/Engines/AgentEngine.h
        Source/Engines/VideoEngine.h
        Source/UI/SolarLookAndFeel.h
        Source/UI/CanvasTimeline.h
        Source/UI/SidePanels.h
        Source/UI/MainComponent.h
)

if(WIN32)
    target_sources(SolarStudio PRIVATE Assets/SolarStudio.rc)
endif()

target_include_directories(SolarStudio
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

target_compile_definitions(SolarStudio
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:SolarStudio,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:SolarStudio,JUCE_VERSION>"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_PLUGINHOST_VST3=1
        $<$<PLATFORM_ID:Darwin>:JUCE_PLUGINHOST_AU=1>
        JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_USE_FLAC=1
        JUCE_USE_OGGVORBIS=1
)

target_link_libraries(SolarStudio
    PRIVATE
        SolarStudioAssets
        juce::juce_gui_extra
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(APPLE)
    set_target_properties(SolarStudio PROPERTIES
        MACOSX_BUNDLE_ICON_FILE "SolarStudio.icns"
    )
    add_custom_command(TARGET SolarStudio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Assets/SolarStudio.icns"
            "$<TARGET_BUNDLE_CONTENT_DIR:SolarStudio>/Resources/SolarStudio.icns"
    )
endif()

# ---------------------------------------------------------------------------
# Target 2: SolarStudio Bridge Companion Plugin (VST3 / AU / Standalone)
# Syncs & streams AI vocals and instruments directly into external host DAWs
# ---------------------------------------------------------------------------
if(APPLE)
    set(BRIDGE_FORMATS AU VST3 Standalone)
else()
    set(BRIDGE_FORMATS VST3 Standalone)
endif()

juce_add_plugin(SolarStudioBridge
    COMPANY_NAME                "SolarAudio"
    PRODUCT_NAME                "SolarStudio Bridge"
    PLUGIN_MANUFACTURER_CODE    Sola
    PLUGIN_CODE                 SlBr
    BUNDLE_ID                   "com.solaraudio.solarstudiobridge"
    FORMATS                     ${BRIDGE_FORMATS}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    ICON_BIG                    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/SolarStudio_logo_512.png"
    ICON_SMALL                  "${CMAKE_CURRENT_SOURCE_DIR}/Assets/SolarStudio_logo_128.png"
)

target_sources(SolarStudioBridge
    PRIVATE
        Bridge/BridgeProcessor.h
        Bridge/BridgeProcessor.cpp
        Bridge/BridgeEditor.h
        Bridge/BridgeEditor.cpp
)

target_compile_definitions(SolarStudioBridge
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SolarStudioBridge
    PRIVATE
        SolarStudioAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
