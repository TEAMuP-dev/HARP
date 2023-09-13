cmake_minimum_required(VERSION 3.18) # Ensure compatibility with file(ARCHIVE_EXTRACT)

# Set variables for the library's URL and the destination directory
set(LIBTORCH_URL "https://github.com/mlverse/libtorch-mac-m1/releases/download/LibTorchOpenMP/libtorch-v2.0.0.zip")
set(LIBTORCH_DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/_downloads")
set(LIBTORCH_UNZIP_DIR "${CMAKE_SOURCE_DIR}")
set(LIBTORCH_ZIP_PATH "${LIBTORCH_DOWNLOAD_DIR}/libtorch.zip")

# Check if the library has already been downloaded
if(NOT EXISTS "${LIBTORCH_ZIP_PATH}")
    message(STATUS "Downloading library from ${LIBTORCH_URL} ...")
    file(DOWNLOAD ${LIBTORCH_URL} "${LIBTORCH_ZIP_PATH}"
         TLS_VERIFY ON
         SHOW_PROGRESS) # Ensure secure downloading
else()
    message(STATUS "libtorch already downloaded")
endif()

# Check if the library has already been extracted
if (NOT EXISTS "${LIBTORCH_UNZIP_DIR}/libtorch")
    message(STATUS "Extracting libtorch ...")
    # extract using unzip 
    execute_process(COMMAND unzip -q -o "${LIBTORCH_ZIP_PATH}" -d "${LIBTORCH_UNZIP_DIR}")
else()
    message(STATUS "libtorch already extracted")
endif()

# Make CMake aware of the library's CMake files, if they exist (this assumes the library has CMake integration)
# the following should make find_package(Torch) work
list(APPEND CMAKE_PREFIX_PATH "${LIB_UNZIP_DIR}")
set(Torch_DIR "${LIBTORCH_UNZIP_DIR}/libtorch/share/cmake/Torch")
