# CMake Module: SetupPythonAndPybind.cmake



# Function to set up Python and pybind
function(setup_relocatable_python)

    message(STATUS "Setting up Relocatable Python for MacOS")

    set (RELOCATABLE_PY_DIR "${CMAKE_SOURCE_DIR}/py")

    # Variables
    set(RELOCATABLE_PY_REPO "https://github.com/hugofloresgarcia/relocatable-python")
    set(RELOCATABLE_PY_REPO_DIR "${RELOCATABLE_PY_DIR}/relocatable-python")
    set(PYTHON_VERSION "3.11.5")
    set(PYTHON_VERSION_DIR "3.11")
    set(REQUIREMENTS_FILE "${RELOCATABLE_PY_DIR}/requirements.txt")
    set(PY_FRAMEWORK_DIR "${RELOCATABLE_PY_REPO_DIR}/Python.framework")

    # Clone the relocatable-python repo
    if(NOT EXISTS "${RELOCATABLE_PY_REPO_DIR}")
        execute_process(
            COMMAND git clone ${RELOCATABLE_PY_REPO}
            WORKING_DIRECTORY ${RELOCATABLE_PY_DIR}
            RESULT_VARIABLE GIT_CLONE_RESULT
            OUTPUT_VARIABLE GIT_CLONE_OUTPUT
            ERROR_VARIABLE GIT_CLONE_ERROR
        )
        if(NOT GIT_CLONE_RESULT EQUAL "0")
            message(FATAL_ERROR "Failed to clone the relocatable-python repository. Output:\n${GIT_CLONE_OUTPUT}\nError:\n${GIT_CLONE_ERROR}")
        else()
            message(STATUS "Git clone output:\n${GIT_CLONE_OUTPUT}")
        endif()
    endif()

    # Run the make_relocatable_python_framework.py script to create a relocatable Python.framework
    # first, erase the Python.framework if it exists
    execute_process(
        COMMAND rm -rf ${PY_FRAMEWORK_DIR}
        WORKING_DIRECTORY ${RELOCATABLE_PY_REPO_DIR}
        RESULT_VARIABLE RM_RESULT
        OUTPUT_VARIABLE RM_OUTPUT
        ERROR_VARIABLE RM_ERROR
    )

    execute_process(
        COMMAND ./make_relocatable_python_framework.py --python-version ${PYTHON_VERSION} --pip-requirements ${REQUIREMENTS_FILE} --destination . --upgrade-pip --os-version 11
        WORKING_DIRECTORY ${RELOCATABLE_PY_REPO_DIR}
        RESULT_VARIABLE SCRIPT_RUN_RESULT
        OUTPUT_VARIABLE SCRIPT_RUN_OUTPUT
        ERROR_VARIABLE SCRIPT_RUN_ERROR
    )
    if(NOT SCRIPT_RUN_RESULT EQUAL "0")
        message(FATAL_ERROR "Failed to execute the make_relocatable_python_framework.py script. Output:\n${SCRIPT_RUN_OUTPUT}\nError:\n${SCRIPT_RUN_ERROR}")
    else()
        message(STATUS "Script run output:\n${SCRIPT_RUN_OUTPUT}")
    endif()

    # Check if Python.framework exists
    if(EXISTS "${PY_FRAMEWORK_DIR}")
        message(STATUS "Python.framework exists!")
    else()
        message(FATAL_ERROR "Python.framework does not exist!")
    endif()

    
    message(STATUS "relocatable python setup complete!")
    
    # set the output variables: 
    set(PY_FRAMEWORK_DIR "${PY_FRAMEWORK_DIR}" PARENT_SCOPE)
    set(Python_ROOT_DIR "${PY_FRAMEWORK_DIR}/Versions/${PYTHON_VERSION_DIR}/" PARENT_SCOPE)
    set(PYTHON_EXECUTABLE "${PY_FRAMEWORK_DIR}/Versions/${PYTHON_VERSION_DIR}/bin/python3" PARENT_SCOPE)

endfunction()

# Usage
# setup_relocatable_python()
