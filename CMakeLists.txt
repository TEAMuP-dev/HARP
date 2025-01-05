# The first line of any CMake project should be a call to `cmake_minimum_required`, which checks
# that the installed CMake will be able to understand the following CMakeLists, and ensures that
# CMake's behaviour is compatible with the named version. This is a standard CMake command, so more
# information can be found in the CMake docs.
cmake_minimum_required(VERSION 3.21)
set(PROJECT_NAME "HARP")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(STRINGS VERSION CURRENT_VERSION)
# add_definitions(-DAPP_VERSION="${CURRENT_VERSION}")
# add_definitions(-DAPP_NAME="${PROJECT_NAME}")
# add_definitions(-DAPP_COPYRIGHT="TEAMuP")



# The top-level CMakeLists.txt file for a project must contain a literal, direct call to the
# `project()` command. `project()` semats up some helpful variables that describe source/binary
# directories, and the current project version. This is a standard CMake command.

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})
add_subdirectory(JUCE)                    # If you've put JUCE in a subdirectory called JUCE


# `juce_add_gui_app` adds an executable target with the name passed as the first argument
# (${PROJECT_NAME} here). This target is a normal CMake target, but has a lot of extra properties set
# up by default. This function accepts many optional arguments. Check the readme at
# `docs/CMake API.md` in the JUCE repo for the full list.
# include(cmake/gradio_client.cmake)

juce_add_gui_app(${PROJECT_NAME}
    # VERSION ...                       # Set this if the app version is different to the project version
    # ICON_* arguments specify a path to an image file to use as an icon
    ICON_BIG "${CMAKE_SOURCE_DIR}/icons/harp_logo_1.png"  # Specify a big icon for the app
    ICON_SMALL "${CMAKE_SOURCE_DIR}/icons/harp_logo_1.png"  # Specify a small icon for the app
    
    DOCUMENT_EXTENSIONS wav mp3 aiff           # Specify file extensions that should be associated with this app
    COMPANY_NAME "TEAMuP"                  # Specify the name of the app's author
    PRODUCT_NAME "HARP")     # The name of the final executable, which can differ from the target name

# `juce_generate_juce_header` will create a JuceHeader.h for a given target, which will be generated
# into your build tree. This should be included with `#include <JuceHeader.h>`. The include path for
# this header will be automatically added to the target. The main function of the JuceHeader is to
# include all your JUCE module headers; if you're happy to include module headers directly, you
# probably don't need to call this.

juce_generate_juce_header(${PROJECT_NAME})

# `target_sources` adds source files to a target. We pass the target that needs the sources as the
# first argument, then a visibility parameter for the sources which should normally be PRIVATE.
# Finally, we supply a list of source files that will be built into the target. This is a standard
# CMake command.

# add_subdirectory(src/external/fontaudio)

target_sources(${PROJECT_NAME}
    PRIVATE
        src/Main.cpp
        src/MainComponent.h
        src/ControlAreaWidget.h
        src/TrackAreaWidget.h
        src/Model.h 
        src/WebModel.h
        src/HarpLogger.h
        src/HarpLogger.cpp
        src/errors.h
        src/utils.h

        src/gradio/GradioClient.cpp
        src/external/magic_enum.hpp

        src/external/fontaudio/src/FontAudio.h
        src/external/fontaudio/src/FontAudio.cpp
        src/external/fontaudio/data/FontAudioData.h
        src/external/fontaudio/data/FontAudioData.cpp
        src/external/fontaudio/data/FontAudioIcons.h

        src/external/fontawesome/src/FontAwesome.h
        src/external/fontawesome/src/FontAwesome.cpp
        src/external/fontawesome/data/FontAwesomeData.h
        src/external/fontawesome/data/FontAwesomeData.cpp
        src/external/fontawesome/data/FontAwesomeIcons.h
        # src/external/fonts/src/IconHelper.h
        # src/external/fonts/src/IconHelper.cpp
        # src/external/fonts/data/FontAudioIcons.h
        # src/external/fonts/data/FontAwesomeIcons.h
        # src/external/fonts/data/FontAudioData.h 
        # src/external/fonts/data/FontAwesomeData.h
        # src/external/fonts/data/FontAudioData.cpp 
        # src/external/fonts/data/FontAwesomeData.cpp
        
        src/gui/MultiButton.cpp
        src/gui/StatusComponent.cpp
        src/gui/HoverHandler.cpp
        src/gui/TitledTextBox.h
        src/gui/SliderWithLabel.h
        src/gui/CustomPathDialog.h
        src/gui/ModelAuthorLabel.h

        src/media/MediaDisplayComponent.cpp
        src/media/AudioDisplayComponent.cpp
        src/media/MidiDisplayComponent.cpp
        src/media/OutputLabelComponent.cpp

        src/pianoroll/KeyboardComponent.cpp
        src/pianoroll/NoteGridComponent.cpp
        src/pianoroll/PianoRollComponent.cpp
)

# `target_compile_definitions` adds some preprocessor definitions to our target. In a Projucer
# project, these might be passed in the 'Preprocessor Definitions' field. JUCE modules also make use
# of compile definitions to switch certain features on/off, so if there's a particular feature you
# need that's not on by default, check the module header for the correct flag to set here. These
# definitions will be visible both to your code, and also the JUCE module code, so for new
# definitions, pick unique names that are unlikely to collide! This is a standard CMake command.

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_gui_app` call
        JUCE_USE_CURL=1     # only needed in Linux
        JUCE_LOAD_CURL_SYMBOLS_LAZILY=1
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${PROJECT_NAME},JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${PROJECT_NAME},JUCE_VERSION>"
        JUCE_USE_FLAC=1
        JUCE_USE_OGGVORBIS=1
        JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_USE_WINDOWS_MEDIA_FORMAT=1
        PUBLIC
        APP_VERSION="${CURRENT_VERSION}"
        APP_COMPANY="TEAMuP"
        APP_NAME="HARP"
        APP_COPYRIGHT="Copyright 2024 TEAMuP. All rights reserved."    
)

# If your target needs extra binary assets, you can add them here. The first argument is the name of
# a new static library target that will include all the binary resources. There is an optional
# `NAMESPACE` argument that can specify the namespace of the generated binary data class. Finally,
# the SOURCES argument should be followed by a list of source files that should be built into the
# static library. These source files can be of any kind (wav data, images, fonts, icons etc.).
# Conversion to binary-data will happen when your target is built.

# juce_add_binary_data(GuiAppData SOURCES ...)

# `target_link_libraries` links libraries and JUCE modules to other libraries or executables. Here,
# we're linking our executable target to the `juce::juce_gui_extra` module. Inter-module
# dependencies are resolved automatically, so `juce_core`, `juce_events` and so on will also be
# linked automatically. If we'd generated a binary data target above, we would need to link to it
# here too. This is a standard CMake command.


target_link_libraries(${PROJECT_NAME}
    PRIVATE
        # GuiAppData            # If we'd created a binary data target, we'd link to it here
        juce::juce_gui_extra
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
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.36.32532\x64\Microsoft.VC143.CRT\msvcp140.dll
# C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.36.32532\x64\Microsoft.VC143.CRT\vcruntime140_1.dll
# C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.36.32532\x64\Microsoft.VC143.CRT\vcruntime140.dll
if (WIN32)
    # Function to find the specific runtime DLLs
    function(find_vc_runtime_dlls out_var)
        message(STATUS "Searching for specific runtime DLLs...")

        # Define the target DLLs
        set(target_dlls
            "msvcp140.dll"
            "vcruntime140_1.dll"
            "vcruntime140.dll"
        )

        # Set the base path for Visual Studio 2022 redistributable DLLs
        set(VS_CRT_BASE_PATH "C:/Program Files/Microsoft Visual Studio/2022/*/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT")

        # Search for the DLLs in the specified base path
        file(GLOB_RECURSE all_runtime_dlls
            LIST_DIRECTORIES false
            "${VS_CRT_BASE_PATH}/*.dll"
        )

        set(runtime_dlls)

        foreach(dll ${all_runtime_dlls})
            get_filename_component(dll_name ${dll} NAME)
            if(dll_name IN_LIST target_dlls)
                list(APPEND runtime_dlls ${dll})
            endif()
        endforeach()

        if(runtime_dlls)
            message(STATUS "Found specific runtime DLLs:")
            foreach(dll ${runtime_dlls})
                message(STATUS "${dll}")
            endforeach()
            set(${out_var} ${runtime_dlls} PARENT_SCOPE)
        else()
            message(FATAL_ERROR "Required specific runtime DLLs not found")
        endif()
    endfunction()

    # Call the function to find the specific DLLs
    find_vc_runtime_dlls(RUNTIME_DLLS)

    # Add a post-build step to copy the specific DLLs to the output directory
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${RUNTIME_DLLS}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif()
# copy the pyinstaller tools to the  bundle
# if (APPLE)
#     add_custom_command(TARGET ${PROJECT_NAME}
#                     POST_BUILD
#                     COMMAND ${CMAKE_COMMAND} -E copy_directory
#                     ${CMAKE_SOURCE_DIR}/py/client/dist/
#                     "$<TARGET_FILE_DIR:${PROJECT_NAME}>/../Resources")
# else()
#     add_custom_command(TARGET ${PROJECT_NAME}
#                     POST_BUILD
#                     COMMAND ${CMAKE_COMMAND} -E copy_directory
#                     ${CMAKE_SOURCE_DIR}/py/client/dist/
#                     "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources")
# endif()

# this fixes the RPATH to be relative to the executable
# in MacOS. Now, all we need to do is copy the 
# dynamic libraries to the executable directories
if (APPLE)
    set_property(TARGET ${PROJECT_NAME} PROPERTY BUILD_RPATH "@loader_path/../Frameworks" )

    set(CMAKE_SKIP_RPATH "NO" CACHE INTERNAL "")
endif(APPLE)
