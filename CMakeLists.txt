cmake_minimum_required(VERSION 3.18)

project(AppGameToolbox LANGUAGES CXX OBJCXX)

if(NOT APPLE)
    message(FATAL_ERROR "AppGameToolbox currently provides Apple AVFoundation audio backends only")
endif()

add_library(AppGameToolbox STATIC
    src/UIElement2D.cxx
    src/Object3D.cxx
    src/Toolbox.cxx
    src/AudioPlayer.mm
    src/SpatialAudioPlayer.mm
    src/AudioTransport.mm
    src/AudioSyncSystem.cxx
    src/AudioFX.mm
)

target_compile_features(AppGameToolbox PUBLIC cxx_std_17)
target_include_directories(AppGameToolbox PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

find_library(AVFOUNDATION_LIBRARY AVFoundation REQUIRED)
target_link_libraries(AppGameToolbox PUBLIC ${AVFOUNDATION_LIBRARY})
set_source_files_properties(src/AudioPlayer.mm src/SpatialAudioPlayer.mm src/AudioTransport.mm src/AudioFX.mm
    PROPERTIES COMPILE_FLAGS "-fobjc-arc")
