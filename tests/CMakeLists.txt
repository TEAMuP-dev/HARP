include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(googletest)

file(GLOB_RECURSE HARP_TEST_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*_tests.cpp"
)

add_executable(harp_tests
    test_main.cpp
    test_harp_logger.cpp
    ${HARP_TEST_SOURCES}
)

target_link_libraries(harp_tests PRIVATE
    GTest::gtest
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_gui_basics
)

target_compile_definitions(harp_tests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=1
    JUCE_LOAD_CURL_SYMBOLS_LAZILY=1
)

add_test(NAME harp_tests COMMAND harp_tests)
