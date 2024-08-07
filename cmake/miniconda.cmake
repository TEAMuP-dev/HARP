# TODO:
# make sure we have a variable for the python executable we'll be using for pyinstaller
# cd into the py/client directory and run `pip install -r requirements.txt`
# then run `pyinstaller gradiojuce_client.py --collect-all gradio_client`
# the dist directory will be copied to the bundle later

# New: Function to download and install Miniconda
function(install_miniconda)
    message(STATUS "Host System Processor: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
    message(STATUS "System Name: ${CMAKE_SYSTEM_NAME}")
    set(MINICONDA_DIR "${CMAKE_SOURCE_DIR}/Miniconda3")

    if (EXISTS ${MINICONDA_DIR})
        message(STATUS "Miniconda already installed at ${MINICONDA_DIR}")
        return()
    endif()

    set(MINICONDA_VERSION "py312_24.5.0-0")

    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(MINICONDA_INSTALLER "Miniconda3-${MINICONDA_VERSION}-Windows-x86_64.exe")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
        message(STATUS "Detected Apple Silicon (arm64) processor")
        set(MINICONDA_INSTALLER "Miniconda3-${MINICONDA_VERSION}-MacOSX-arm64.sh")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(MINICONDA_INSTALLER "Miniconda3-${MINICONDA_VERSION}-MacOSX-x86_64.sh")
    else()
        set(MINICONDA_INSTALLER "Miniconda3-${MINICONDA_VERSION}-Linux-x86_64.sh")
    endif()

    # If the Miniconda installer is not found, download it
    if (EXISTS "${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER}")
        message(STATUS "Miniconda installer already exists at ${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER}")
    else()
        message(STATUS "Downloading Miniconda installer...")
        file(DOWNLOAD
            "https://repo.anaconda.com/miniconda/${MINICONDA_INSTALLER}"
            "${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER}"
            TIMEOUT 60  # Adds a timeout for the download
        )

        if (NOT EXISTS "${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER}")
            message(FATAL_ERROR "Failed to download Miniconda installer from ${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER}")
            return()
        endif()
    endif()

    # Determine the install script based on the system
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(MINICONDA_INSTALL_SCRIPT "${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER} /D=${MINICONDA_DIR}")
        # set(MINICONDA_INSTALL_SCRIPT "${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER}" /InstallationType=JustMe /RegisterPython=0 /S /D="${MINICONDA_DIR}")
    else()
        set(MINICONDA_INSTALL_SCRIPT "bash ${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER} -b -p ${MINICONDA_DIR}")
    endif()

    message(STATUS "Installing Miniconda...")
    
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        message(STATUS "Running: ${MINICONDA_INSTALL_SCRIPT}")
        execute_process(
            COMMAND cmd.exe /C start /wait "" "${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER}" /D="${MINICONDA_DIR}"
            # COMMAND cmd.exe /C start /wait "" "${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER}" /InstallationType=JustMe /RegisterPython=0 /S /D="${MINICONDA_DIR}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
    else()
        message(STATUS "Running: ${MINICONDA_INSTALL_SCRIPT}")
        # Set the correct permissions for the file using chmod +x
        execute_process(
            COMMAND chmod +x "${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error
        )
        execute_process(
            COMMAND bash -c "${MINICONDA_INSTALL_SCRIPT}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error
        )
    endif()

    if(result)
        message(FATAL_ERROR "Miniconda installation failed with result: ${result}. Output: ${output}. Error: ${error}")
    else()
        message(STATUS "Miniconda installed successfully.")
    endif()
endfunction()
