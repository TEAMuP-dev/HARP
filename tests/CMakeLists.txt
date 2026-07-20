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
    gtest
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_gui_basics
)

add_test(NAME harp_tests COMMAND harp_tests)
