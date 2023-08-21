cmake_minimum_required(VERSION 3.21)
set(PROJECT_NAME "tensor-juce-ARA")
project(${PROJECT_NAME} VERSION 0.0.1)
add_subdirectory(JUCE)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JUCE_COPY_PLUGIN_AFTER_BUILD TRUE)

set (TENSORJUCE_FORMATS "VST3")

# defines SourceFiles
include(cmake/SourceFiles.cmake)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # windows library location
    message("Building for Windows...") 
    # libtorch settings
    find_package(Torch REQUIRED PATHS "C:\\libtorch" NO_DEFAULT_PATH)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")
    # ara settings
    juce_set_ara_sdk_path("C:\\SDKs\\ARA_SDK")

elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # mac and linux library location
    message("Building for UNIX...")
    # libtorch settings
    find_package(Torch REQUIRED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")
    # ara settings
    juce_set_ara_sdk_path("/usr/local/lib/ARA_SDK")

else()
    message(FATAL_ERROR "Unsupported platform.")

endif()

# add_subdirectory(pybind11)



# add_subdirectory(tracktion_engine/modules)

juce_add_plugin(${PROJECT_NAME}
    # VERSION ...                               # Set this if the plugin version is different to the project version
    # ICON_BIG ...                              # ICON_* arguments specify a path to an image file to use as an icon for the Standalone
    # ICON_SMALL ...
    COMPANY_NAME "TEAMuP"             # Specify the name of the plugin's author
    # IS_SYNTH FALSE                              # Is this a synth or an effect?
    # NEEDS_MIDI_INPUT FALSE                      # Does the plugin need midi input?
    # NEEDS_MIDI_OUTPUT FALSE                     # Does the plugin need midi output?
    # IS_MIDI_EFFECT FALSE                        # Is this plugin a MIDI effect?
    # EDITOR_WANTS_KEYBOARD_FOCUS FALSE           # Does the editor need keyboard focus?
    # COPY_PLUGIN_AFTER_BUILD False               # Should the plugin be installed to a default location after building?
    PLUGIN_MANUFACTURER_CODE ADeC               # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE AraS                            # A unique four-character plugin id with exactly one upper-case character
                                                # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
                                                # However this ID has been assigned before that behaviour was discovered, and remains as-is for backwards
                                                # compatibility - GarageBand 10.3 does not support ARA anyways.
    PRODUCT_NAME tensor-juce                # The name of the final executable, which can differ from the target name
    FORMATS ${TENSORJUCE_FORMATS}
    # VST3_CATEGORIES "Tools"
    # AU_SANDBOX_SAFE TRUE
    # APP_SANDBOX_ENABLED TRUE
    # HARDENED_RUNTIME_ENABLED TRUE
    IS_ARA_EFFECT TRUE                          # Are we using ARA?
    # ARA_FACTORY_ID "..."                      # Optional custom ARA factory ID (otherwise generated from bundle ID and version)
    # ARA_DOCUMENT_ARCHIVE_ID "..."             # Optional custom ARA document archive ID (otherwise generated from bundle ID)
                                                # This must be manually updated whenever changing the archive in non-backwards compatible ways!
                                                # Note that if changing the document archive ID, you'll also have to add the previous ID to JucePlugin_ARACompatibleArchiveIDs!
    # ARA_COMPATIBLE_ARCHIVE_IDS "...", "..."   # Initially empty, but will indicate upwards compatibility when changing ARA_DOCUMENT_ARCHIVE_ID.
    # ARA_ANALYSIS_TYPES                        # If providing analyzable ARA content types to the host, define them as OR'd values here - defaults to 0.
    # ARA_TRANSFORMATION_FLAGS                  # If supporting time-stretching or other ARA playback transformations, define them as OR'd values here - otherwise 0.
)


juce_generate_juce_header(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        ${SourceFiles}
)

# `target_compile_definitions` adds some preprocessor definitions to our target. In a Projucer
# project, these might be passed in the 'Preprocessor Definitions' field. JUCE modules also make use
# of compile definitions to switch certain features on/off, so if there's a particular feature you
# need that's not on by default, check the module header for the correct flag to set here. These
# definitions will be visible both to your code, and also the JUCE module code, so for new
# definitions, pick unique names that are unlikely to collide! This is a standard CMake command.

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
        JUCE_USE_CURL=1    # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
        JUCE_VST3_CAN_REPLACE_VST2=0)



target_link_libraries(${PROJECT_NAME}
    PRIVATE
        # AudioPluginData
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        # tracktion::tracktion_engine

        # tractkion::tracktion_core
        # tracktion::3rd_party
        # tracktion::tracktion_graph

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
        ${TORCH_LIBRARIES}

        # pybind11::embed
)

# set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

# The following code block is suggested to be used on Windows.
# According to https://github.com/pytorch/pytorch/issues/25457,
# the DLLs need to be copied to avoid memory errors.
if (MSVC)
  file(GLOB TORCH_DLLS "${TORCH_INSTALL_PREFIX}/lib/*.dll")
  add_custom_command(TARGET ${PROJECT_NAME}
                     POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different
                     ${TORCH_DLLS}
                     $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif (MSVC)

if (UNIX AND NOT APPLE) # Linux
    # For Linux, libtorch usually provides .so files. Modify this as needed.
    file(GLOB TORCH_SHARED_LIBRARIES "${TORCH_INSTALL_PREFIX}/lib/*.so")
elseif(APPLE)
    # For macOS, libtorch provides .dylib files. Modify this as needed.
    file(GLOB TORCH_SHARED_LIBRARIES "${TORCH_INSTALL_PREFIX}/lib/*.dylib")
endif()

if (UNIX)
    foreach(format ${TENSORJUCE_FORMATS})
        # make a resource directory
        add_custom_command(TARGET ${PROJECT_NAME}_${format}
                        POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E make_directory
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}_${format}>/../Resources/")
        add_custom_command(TARGET ${PROJECT_NAME}_${format}
                        POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${TORCH_SHARED_LIBRARIES}
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}_${format}>/../Resources/")
    endforeach()
endif()



# this fixes the RPATH to be relative to the executable
# in MacOS. Now, all we need to do is copy the libtorch
# dynamic libraries to the executable directories
if (APPLE)
    foreach(format ${TENSORJUCE_FORMATS})
        set_property(TARGET ${PROJECT_NAME}_${format} PROPERTY BUILD_RPATH "@loader_path/../Resources" )

    endforeach()

    set(CMAKE_SKIP_RPATH "NO" CACHE INTERNAL "")
endif(APPLE)


