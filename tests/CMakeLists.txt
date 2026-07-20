include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

FetchContent_MakeAvailable(googletest)

add_executable(harp_tests
    test_main.cpp
    test_harp_logger.cpp
    utils/controls_tests.cpp
    utils/settings_tests.cpp
    clients/client_tests.cpp
    clients/gradio_client_tests.cpp
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

if(HARP_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(harp_tests PRIVATE
            -O0
            -g
            --coverage
        )

        target_link_options(harp_tests PRIVATE
            --coverage
        )
    else()
        message(WARNING
            "HARP_ENABLE_COVERAGE is enabled, but coverage is only configured for GCC and Clang."
        )
    endif()
endif()

add_test(NAME harp_tests COMMAND harp_tests)
