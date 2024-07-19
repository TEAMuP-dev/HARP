include(cmake/miniconda.cmake)

# Create a flag that indicates we are using Miniconda
set(USING_MINICONDA TRUE)
# Call the function to install Miniconda
if(DEFINED ENV{CI} AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # If running in a CI env on windows don't install miniconda
    # we'll use the python that's in the github actions environment
    message(STATUS "Skipping Miniconda installation on Windows CI")
    set(USING_MINICONDA FALSE)
endif()

if(USING_MINICONDA)
    install_miniconda()
    set(MINICONDA_DIR "${CMAKE_SOURCE_DIR}/Miniconda3")  # Update this path if Miniconda is installed in a different location
endif()
# Update the paths to point to the Python executable and PyInstaller in the Miniconda environment
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (USING_MINICONDA)
        set(PYTHON_EXECUTABLE "${MINICONDA_DIR}/python.exe")
        set(PYINSTALLER_EXECUTABLE "${MINICONDA_DIR}/Scripts/pyinstaller.exe")
    else()
        set(PYTHON_EXECUTABLE "python")
        set(PYINSTALLER_EXECUTABLE "pyinstaller")
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PYTHON_EXECUTABLE "${MINICONDA_DIR}/bin/python")
    set(PYINSTALLER_EXECUTABLE "${MINICONDA_DIR}/bin/pyinstaller")
endif()


# Define the command sequence as a list
set(PYINSTALLER_COMMANDS 
    "${PYTHON_EXECUTABLE} -m ensurepip --upgrade"
    "${PYTHON_EXECUTABLE} -m pip install -r ${CMAKE_SOURCE_DIR}/py/client/requirements.txt"
    "${PYINSTALLER_EXECUTABLE} --hide-console hide-early -y gradiojuce_client.py --collect-all gradio_client"
)

# Execute each command individually, and check the result of each
foreach(cmd IN LISTS PYINSTALLER_COMMANDS)
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        execute_process(
            COMMAND cmd /c "${cmd}"  # Use cmd /c on Windows
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/py/client  # Ensure the working directory is set correctly
        )
    else()
        execute_process(
            COMMAND bash -c "${cmd}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/py/client
        )
    endif()
    
    if(result)
        message(FATAL_ERROR "Command '${cmd}' failed with result: ${result}. Output: ${output}. Error: ${error}")
    else()
        message(STATUS "Command '${cmd}' succeeded. Output:\n${output}")
    endif()
endforeach()
