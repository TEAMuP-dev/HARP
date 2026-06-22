## put all cpp files from this direc. and sub-direcs. into HARP_SOURCES list
file(GLOB_RECURSE HARP_SOURCES "*.cpp") 

## add HARP_SOURCES list to a library called harp
add_library(harp STATIC ${HARP_SOURCES}) 

## files linked to this directory can see its header files
target_include_directories(harp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}) 
