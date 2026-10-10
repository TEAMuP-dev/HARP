cmake_minimum_required(VERSION 3.22)

# Must be set before project(). This is the oldest macOS that current Xcode can target.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

file(STRINGS VERSION CURRENT_VERSION)

project(HARP VERSION ${CURRENT_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Link the MSVC runtime statically, so HARP.exe runs without the Visual C++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(COMPANY_NAME "TEAMuP")
set(COPYRIGHT "Copyright 2026 TEAMuP. All rights reserved.")

add_subdirectory(JUCE)

# See JUCE/docs/CMake API.md for all options
juce_add_gui_app(${PROJECT_NAME}
    PRODUCT_NAME "${PROJECT_NAME}"
    COMPANY_NAME "${COMPANY_NAME}"
    COMPANY_COPYRIGHT "${COPYRIGHT}"
    BUNDLE_ID "com.TEAMuP.HARP"
    ICON_BIG "${CMAKE_SOURCE_DIR}/resources/icons/harp_logo_1.png"
    DOCUMENT_EXTENSIONS wav mp3 aiff)

# Creates JuceHeader.h, which includes all linked JUCE modules
juce_generate_juce_header(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        src/Main.cpp
        src/Application.cpp
        src/MainComponent.cpp
        src/HomeTab.h
        src/ModelTabContainer.h
        src/ModelTab.h
        src/Model.h

        src/windows/AboutWindow.h
        src/windows/CustomPathWindow.h

        src/windows/tutorial/TutorialTargets.h
        src/windows/tutorial/TutorialOverlay.h
        src/windows/tutorial/TutorialWindow.h

        src/windows/settings/SettingsWindow.h
        src/windows/settings/GeneralSettingsTab.cpp
        src/windows/settings/LoginTab.cpp

        src/widgets/ModelInfoWidget.h
        src/widgets/ModelStyle.h
        src/widgets/ControlAreaWidget.h
        src/widgets/TrackAreaWidget.h
        src/widgets/StatusAreaWidget.h
        src/widgets/MediaClipboardWidget.h

        src/gui/MultiButton.cpp
        src/gui/HoverableLabel.h
        src/gui/TextBoxWithLabel.h
        src/gui/NumberBoxWithLabel.h
        src/gui/SliderWithLabel.h
        src/gui/ComboBoxWithLabel.h
        src/gui/MultiSelectWithLabel.h
        src/gui/HoverHandler.h
        src/gui/ControlComponent.h
        src/gui/ToggleWithLabel.h
        src/gui/FileChooserWithLabel.h

        src/media/MediaDisplayComponent.cpp
        src/media/AudioDisplayComponent.cpp
        src/media/MidiDisplayComponent.cpp
        src/media/FileDisplayComponent.cpp
        src/media/OutputLabelComponent.cpp

        src/media/pianoroll/KeyboardComponent.cpp
        src/media/pianoroll/NoteGridComponent.cpp
        src/media/pianoroll/PianoRollComponent.cpp
        src/media/pianoroll/SynthAudioSource.h

        src/clients/Client.h
        src/clients/GradioClient.h
        src/clients/providers/stability/StabilityClient.h

        src/utils/Logging.h
        src/utils/Messages.h
        src/utils/Settings.h
        src/utils/Interface.h
        src/utils/ModelCatalog.h
        src/utils/ModelTags.h
        src/utils/Controls.h
        src/utils/Labels.h
        src/utils/Clients.h
        src/utils/Errors.h
        src/utils/Enums.h

        src/external/magic_enum.hpp
        src/external/fontawesome/src/FontAwesome.h
        src/external/fontawesome/src/FontAwesome.cpp
        src/external/fontawesome/data/FontAwesomeData.h
        src/external/fontawesome/data/FontAwesomeData.cpp
        src/external/fontawesome/data/FontAwesomeIcons.h
        src/external/fontaudio/src/FontAudio.h
        src/external/fontaudio/src/FontAudio.cpp
        src/external/fontaudio/data/FontAudioData.h
        src/external/fontaudio/data/FontAudioData.cpp
        src/external/fontaudio/data/FontAudioIcons.h
)

if(APPLE)
    target_sources(${PROJECT_NAME} PRIVATE src/utils/copy/CopyMacOS.mm)
elseif(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE src/utils/copy/CopyWindows.cpp)
else()
    target_sources(${PROJECT_NAME} PRIVATE src/utils/copy/CopyLinux.cpp)

    find_package(X11 REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE X11::X11)
endif()

# These are visible to both our code and the JUCE modules (see each module's header for flags)
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        # Unused, and would otherwise require WebKit/GTK on Linux
        JUCE_WEB_BROWSER=0
        # Load libcurl (Linux only) at runtime, so the binary does not link a specific version
        JUCE_LOAD_CURL_SYMBOLS_LAZILY=1
        # Off by default, unlike FLAC and Ogg Vorbis
        JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${PROJECT_NAME},JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${PROJECT_NAME},JUCE_VERSION>"
        APP_VERSION="${PROJECT_VERSION}"
        APP_COMPANY="${COMPANY_NAME}"
        APP_NAME="${PROJECT_NAME}"
        APP_COPYRIGHT="${COPYRIGHT}"
)

juce_add_binary_data(stability_controls
    SOURCES
        ${CMAKE_SOURCE_DIR}/src/clients/providers/stability/models/text-to-audio.json
        ${CMAKE_SOURCE_DIR}/src/clients/providers/stability/models/audio-to-audio.json
)

# The model taxonomy is defined once, in pyharp, where models declare their tags
juce_add_binary_data(model_taxonomy
    HEADER_NAME TaxonomyData.h
    NAMESPACE TaxonomyData
    SOURCES
        ${CMAKE_SOURCE_DIR}/pyharp/pyharp/taxonomy.json
)

# Dependencies between JUCE modules are resolved automatically
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_extra
        stability_controls
        model_taxonomy
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
