include(cmake/miniconda.cmake)

# Call the function to install Miniconda
install_miniconda()

# Update the paths to point to the Python executable and PyInstaller in the Miniconda environment
set(MINICONDA_DIR "${CMAKE_SOURCE_DIR}/Miniconda3")  # Update this path if Miniconda is installed in a different location
set(PYTHON_EXECUTABLE "${MINICONDA_DIR}/bin/python")  # On Windows, change /bin/ to /Scripts/ and python to python.exe
set(PYINSTALLER_EXECUTABLE "${MINICONDA_DIR}/bin/pyinstaller")  # On Windows, change /bin/ to /Scripts/ and pyinstaller to pyinstaller.exe


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