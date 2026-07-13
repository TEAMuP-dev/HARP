cmake_minimum_required(VERSION 3.21)

file(STRINGS VERSION CURRENT_VERSION)

project(HARP VERSION ${CURRENT_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(JUCE_SILENCE_XCODE_DEPRECATION_WARNINGS ON)

add_subdirectory(JUCE)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    add_compile_options(-Wall -Wextra)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W4)
endif()

set(HARP_COMPANY_NAME "TEAMuP")
set(HARP_ICON "${CMAKE_SOURCE_DIR}/resources/icons/harp_logo_1.png")

set(HARP_COMMON_SOURCES
    src/Model.h
    src/clients/Client.h
    src/clients/GradioClient.h
    src/utils/Logging.h
    src/utils/Settings.h
    src/utils/Interface.h
    src/utils/Controls.h
    src/utils/Labels.h
    src/utils/Clients.h
    src/utils/Errors.h
    src/utils/Enums.h
)

set(HARP_APP_SOURCES
    src/Main.cpp
    src/Application.cpp
    src/MainComponent.cpp
    src/ModelTab.h
    src/windows/AboutWindow.h
    src/windows/WelcomeWindow.h
    src/windows/settings/SettingsWindow.h
    src/windows/settings/GeneralSettingsTab.cpp
    src/windows/settings/LoginTab.cpp
    src/widgets/ModelSelectionWidget.h
    src/widgets/ModelInfoWidget.h
    src/widgets/ControlAreaWidget.h
    src/widgets/TrackAreaWidget.h
    src/widgets/StatusAreaWidget.h
    src/widgets/MediaClipboardWidget.h
    src/gui/MultiButton.cpp
    src/gui/HoverableLabel.h
    src/gui/TextBoxWithLabel.h
    src/gui/SliderWithLabel.h
    src/gui/ComboBoxWithLabel.h
    src/gui/HoverHandler.h
    src/gui/ControlComponent.h
    src/gui/ToggleWithLabel.h
    src/media/MediaDisplayComponent.cpp
    src/media/AudioDisplayComponent.cpp
    src/media/MidiDisplayComponent.cpp
    src/media/OutputLabelComponent.cpp
    src/media/pianoroll/KeyboardComponent.cpp
    src/media/pianoroll/NoteGridComponent.cpp
    src/media/pianoroll/PianoRollComponent.cpp
    src/media/pianoroll/SynthAudioSource.h
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

set(HARP_JUCE_MODULES
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_gui_basics
    juce::juce_gui_extra
)

set(HARP_JUCE_FLAGS
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

juce_add_gui_app(${PROJECT_NAME}
    ICON_BIG "${HARP_ICON}"
    ICON_SMALL "${HARP_ICON}"
    DOCUMENT_EXTENSIONS wav mp3 aiff
    COMPANY_NAME "${HARP_COMPANY_NAME}"
    PRODUCT_NAME "HARP"
)

juce_generate_juce_header(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        ${HARP_APP_SOURCES}
        ${HARP_COMMON_SOURCES}
)

if(APPLE)
    target_sources(${PROJECT_NAME} PRIVATE src/utils/copy/CopyMacOS.mm)
elseif(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE src/utils/copy/CopyWindows.cpp)
elseif(LINUX)
    target_sources(${PROJECT_NAME} PRIVATE src/utils/copy/CopyLinux.cpp)
endif()

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=1
        JUCE_LOAD_CURL_SYMBOLS_LAZILY=1
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${PROJECT_NAME},JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${PROJECT_NAME},JUCE_VERSION>"
        JUCE_USE_FLAC=1
        JUCE_USE_OGGVORBIS=1
        JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_USE_WINDOWS_MEDIA_FORMAT=1
        APP_VERSION="${CURRENT_VERSION}"
        APP_COMPANY="${HARP_COMPANY_NAME}"
        APP_NAME="HARP"
        APP_COPYRIGHT="Copyright 2026 TEAMuP. All rights reserved."
)

juce_add_binary_data(stability_controls
    SOURCES
        "${CMAKE_SOURCE_DIR}/src/clients/providers/stability/models/text-to-audio.json"
        "${CMAKE_SOURCE_DIR}/src/clients/providers/stability/models/audio-to-audio.json"
        "${CMAKE_SOURCE_DIR}/resources/models/model_registry.json"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${HARP_JUCE_MODULES}
        stability_controls
    PUBLIC
        ${HARP_JUCE_FLAGS}
)

if(LINUX)
    find_package(X11 REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE X11::X11)
endif()

if(WIN32)
    install(TARGETS ${PROJECT_NAME}
        RUNTIME_DEPENDENCIES
            PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
            POST_EXCLUDE_REGEXES "system32"
        RUNTIME DESTINATION .
        RESOURCE DESTINATION .
    )
endif()

# Validate the bundled model registry at configure time so a bad edit fails the
# build instead of silently degrading the in-app model list.
file(READ "${CMAKE_SOURCE_DIR}/resources/models/model_registry.json" HARP_MODEL_REGISTRY_CONTENTS)
string(JSON HARP_MODEL_REGISTRY_MODEL_COUNT
    ERROR_VARIABLE HARP_MODEL_REGISTRY_ERROR
    LENGTH "${HARP_MODEL_REGISTRY_CONTENTS}" models)

if(HARP_MODEL_REGISTRY_ERROR)
    message(FATAL_ERROR "resources/models/model_registry.json is invalid: ${HARP_MODEL_REGISTRY_ERROR}")
elseif(HARP_MODEL_REGISTRY_MODEL_COUNT LESS 1)
    message(FATAL_ERROR "resources/models/model_registry.json contains no models.")
endif()

# The validation runner performs live network calls against deployed models, so
# it is opt-in rather than part of the default build and test suite.
option(HARP_BUILD_MODEL_VALIDATION "Build the remote model validation runner (live-network tests)" OFF)

if(HARP_BUILD_MODEL_VALIDATION)
    include(CTest)

    juce_add_console_app(HARPRemoteModelTests
        PRODUCT_NAME "HARPRemoteModelTests"
    )

    juce_generate_juce_header(HARPRemoteModelTests)

    target_sources(HARPRemoteModelTests
        PRIVATE
            tests/ModelValidationMain.cpp
            ${HARP_COMMON_SOURCES}
    )

    target_include_directories(HARPRemoteModelTests
        PRIVATE
            "${CMAKE_SOURCE_DIR}/src"
    )

    target_compile_definitions(HARPRemoteModelTests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=1
            JUCE_LOAD_CURL_SYMBOLS_LAZILY=1
            JUCE_USE_OGGVORBIS=1
            JUCE_USE_MP3AUDIOFORMAT=1
            HARP_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    target_link_libraries(HARPRemoteModelTests
        PRIVATE
            ${HARP_JUCE_MODULES}
            stability_controls
        PUBLIC
            ${HARP_JUCE_FLAGS}
    )

    if(BUILD_TESTING)
        add_test(NAME HARPRemoteModelTests COMMAND HARPRemoteModelTests)

        # The serial sweep can legitimately run for over an hour; ctest's default
        # 1500s timeout would kill it mid-run before any report is written.
        set_tests_properties(HARPRemoteModelTests PROPERTIES TIMEOUT 7200)
    endif()
endif()

if(APPLE)
    set_property(TARGET ${PROJECT_NAME} PROPERTY BUILD_RPATH "@loader_path/../Frameworks")
    set(CMAKE_SKIP_RPATH "NO" CACHE INTERNAL "")
endif()
