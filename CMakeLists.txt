cmake_minimum_required(VERSION 3.21)
set(PROJECT_NAME "HARP")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(JUCE_COPY_PLUGIN_AFTER_BUILD TRUE)

set (TENSORJUCE_FORMATS "VST3" "Standalone")

# defines SourceFiles
include(cmake/SourceFiles.cmake)

# Reads in our VERSION file and sticks in it CURRENT_VERSION variable
# Be sure the file has no newlines!
file(STRINGS VERSION CURRENT_VERSION)

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})
add_subdirectory(JUCE)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # windows library location
    message("Building for Windows...") 
    
    # ara settings
    # juce_set_ara_sdk_path("C:\\SDKs\\ARA_SDK")
    juce_set_ara_sdk_path("${CMAKE_SOURCE_DIR}/ARA_SDK")

elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # mac and linux library location
    message("Building for UNIX...")

    # ara settings
    juce_set_ara_sdk_path("${CMAKE_SOURCE_DIR}/ARA_SDK")

else()
    message(FATAL_ERROR "Unsupported platform.")

endif()

# TODO:
# make sure we have a variable for the python executable we'll be using for pyinstaller
# cd into the py/client directory and run `pip install -r requirements.txt`
# then run `pyinstaller gradiojuce_client.py --collect-all gradio_client`
# the dist directory will be copied to the bundle later

# New: Function to download and install Miniconda
function(install_miniconda)
    set(MINICONDA_DIR "${CMAKE_SOURCE_DIR}/Miniconda3")

    if (EXISTS ${MINICONDA_DIR})
        message(STATUS "Miniconda already installed at ${MINICONDA_DIR}")
        return()
    endif()

    set(MINICONDA_VERSION "py39_23.5.2-0")

    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(MINICONDA_INSTALLER "Miniconda3-${MINICONDA_VERSION}-Windows-x86_64.exe")
        set(MINICONDA_INSTALL_SCRIPT "start /wait ${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER} /S /D=C:\\Miniconda3")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(MINICONDA_INSTALLER "Miniconda3-${MINICONDA_VERSION}-MacOSX-arm64.sh")
        set(MINICONDA_INSTALL_SCRIPT "bash ${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER} -b -p ${CMAKE_SOURCE_DIR}/Miniconda3")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(MINICONDA_INSTALLER "Miniconda3-${MINICONDA_VERSION}-MacOSX-x86_64.sh")
        set(MINICONDA_INSTALL_SCRIPT "bash ${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER} -b -p ${CMAKE_SOURCE_DIR}/Miniconda3")
    else()
        set(MINICONDA_INSTALLER "Miniconda3-${MINICONDA_VERSION}-Linux-x86_64.sh")
        set(MINICONDA_INSTALL_SCRIPT "bash ${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER} -b -p ${CMAKE_SOURCE_DIR}/Miniconda3")
    endif()

    message(STATUS "Downloading Miniconda installer...")
    file(DOWNLOAD
        "https://repo.anaconda.com/miniconda/${MINICONDA_INSTALLER}"
        "${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER}"
    )

    if (NOT EXISTS "${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER}")
        message(FATAL_ERROR "Miniconda installer not found at ${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER}")
    endif()

    message(STATUS "Installing Miniconda...")
    execute_process(
        COMMAND bash -c "${MINICONDA_INSTALL_SCRIPT}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(result)
        message(FATAL_ERROR "Miniconda installation failed with result: ${result}. Output: ${output}. Error: ${error}")
    else()
        message(STATUS "Miniconda installed successfully.")
    endif()
endfunction()


# Call the function to install Miniconda
install_miniconda()

# Update the paths to point to the Python executable and PyInstaller in the Miniconda environment
set(MINICONDA_DIR "${CMAKE_SOURCE_DIR}/Miniconda3")  # Update this path if Miniconda is installed in a different location
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PYTHON_EXECUTABLE "${MINICONDA_DIR}/python.exe")
    set(PYINSTALLER_EXECUTABLE "${MINICONDA_DIR}/Scripts/pyinstaller.exe")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PYTHON_EXECUTABLE "${MINICONDA_DIR}/bin/python")
    set(PYINSTALLER_EXECUTABLE "${MINICONDA_DIR}/bin/pyinstaller")
endif()


# Define the command sequence as a list
set(PYINSTALLER_COMMANDS 
    "${PYTHON_EXECUTABLE} -m ensurepip --upgrade"
    "${PYTHON_EXECUTABLE} -m pip install -r ${CMAKE_SOURCE_DIR}/py/client/requirements.txt"
    "${PYINSTALLER_EXECUTABLE} -y gradiojuce_client.py --collect-all gradio_client"
)

# Execute each command individually, and check the result of each
foreach(cmd IN LISTS PYINSTALLER_COMMANDS)
    execute_process(
        COMMAND bash -c "${cmd}"  # Using bash -c to ensure command is executed in a shell
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/py/client  # Ensure the working directory is set correctly
    )
    if(result)
        message(FATAL_ERROR "Command '${cmd}' failed with result: ${result}. Output: ${output}. Error: ${error}")
    else()
        message(STATUS "Command '${cmd}' succeeded. Output:\n${output}")
    endif()
endforeach()




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
    PRODUCT_NAME HARP                # The name of the final executable, which can differ from the target name
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

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags

)


# copy the pyinstaller tools to the  bundle
if (UNIX)
    foreach(format ${TENSORJUCE_FORMATS})
        # make a resource directory
        add_custom_command(TARGET ${PROJECT_NAME}_${format}
                        POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E make_directory
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}_${format}>/../Resources/")
        # Use the native cp command with -R to properly handle symlinks
        add_custom_command(TARGET ${PROJECT_NAME}_${format}
                        POST_BUILD
                        COMMAND cp -R
                        ${CMAKE_SOURCE_DIR}/py/client/dist/
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}_${format}>/../Resources/")

    endforeach()
elif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    foreach(format ${TENSORJUCE_FORMATS})
        # make a resource directory
        add_custom_command(TARGET ${PROJECT_NAME}_${format}
                        POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E make_directory
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}_${format}>/../Resources")
        # Use the native copy command with /Y to suppress prompt
        add_custom_command(TARGET ${PROJECT_NAME}_${format}
                        POST_BUILD
                        COMMAND copy /Y
                        ${CMAKE_SOURCE_DIR}/py/client/dist/*
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}_${format}>/../Resources")
    endforeach()
endif()


# this fixes the RPATH to be relative to the executable
# in MacOS. Now, all we need to do is copy the 
# dynamic libraries to the executable directories
if (APPLE)
    foreach(format ${TENSORJUCE_FORMATS})
        set_property(TARGET ${PROJECT_NAME}_${format} PROPERTY BUILD_RPATH "@loader_path/../Frameworks" )
    endforeach()

    set(CMAKE_SKIP_RPATH "NO" CACHE INTERNAL "")
endif(APPLE)


