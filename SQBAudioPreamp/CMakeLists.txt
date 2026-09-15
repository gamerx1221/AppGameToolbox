cmake_minimum_required(VERSION 3.20)

add_library(SQBAudioPreamp STATIC
    src/AudioBuffer.cpp
    src/PreampEngine.cpp
)
add_library(SQB::AudioPreamp ALIAS SQBAudioPreamp)

target_compile_features(SQBAudioPreamp PUBLIC cxx_std_20)
target_include_directories(SQBAudioPreamp
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/speexdsp")
    target_compile_definitions(SQBAudioPreamp PUBLIC SQB_AUDIO_PREAMP_HAS_SPEEXDSP=1)
else()
    target_compile_definitions(SQBAudioPreamp PUBLIC SQB_AUDIO_PREAMP_HAS_SPEEXDSP=0)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/kissfft")
    target_compile_definitions(SQBAudioPreamp PUBLIC SQB_AUDIO_PREAMP_HAS_KISSFFT=1)
else()
    target_compile_definitions(SQBAudioPreamp PUBLIC SQB_AUDIO_PREAMP_HAS_KISSFFT=0)
endif()

if(MSVC)
    target_compile_options(SQBAudioPreamp PRIVATE /W4 /permissive-)
else()
    target_compile_options(SQBAudioPreamp PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING AND SQB_AUDIO_PREAMP_BUILD_TESTS)
    add_executable(SQBAudioPreampTests tests/PreampTests.cpp)
    target_link_libraries(SQBAudioPreampTests PRIVATE SQBAudioPreamp)
    add_test(NAME SQBAudioPreampTests COMMAND SQBAudioPreampTests)
endif()
