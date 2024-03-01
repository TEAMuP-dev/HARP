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
