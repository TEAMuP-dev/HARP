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
        # set(MINICONDA_INSTALL_SCRIPT "start /wait ${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER} /S /D=C:\\Miniconda3")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(MINICONDA_INSTALLER "Miniconda3-${MINICONDA_VERSION}-MacOSX-arm64.sh")
        # set(MINICONDA_INSTALL_SCRIPT "bash ${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER} -b -p ${CMAKE_SOURCE_DIR}/Miniconda3")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(MINICONDA_INSTALLER "Miniconda3-${MINICONDA_VERSION}-MacOSX-x86_64.sh")
        # set(MINICONDA_INSTALL_SCRIPT "bash ${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER} -b -p ${CMAKE_SOURCE_DIR}/Miniconda3")
    else()
        set(MINICONDA_INSTALLER "Miniconda3-${MINICONDA_VERSION}-Linux-x86_64.sh")
        # set(MINICONDA_INSTALL_SCRIPT "bash ${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER} -b -p ${CMAKE_SOURCE_DIR}/Miniconda3")
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
    else()
        set(MINICONDA_INSTALL_SCRIPT "bash ${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER} -b -p ${MINICONDA_DIR}")
    endif()

    message(STATUS "Installing Miniconda...")
    
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        message(STATUS "Running: ${MINICONDA_INSTALL_SCRIPT}")
        execute_process(
            COMMAND cmd.exe /C start /wait "" "${CMAKE_SOURCE_DIR}/${MINICONDA_INSTALLER}" /D="${MINICONDA_DIR}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
    else()
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
