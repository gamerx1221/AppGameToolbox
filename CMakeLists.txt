cmake_minimum_required(VERSION 3.18)

project(AppGameToolbox LANGUAGES CXX)

add_library(AppGameToolbox STATIC
    src/UIElement2D.cxx
    src/Object3D.cxx
    src/Toolbox.cxx
    src/AudioSyncSystem.cxx
    src/AudioFX.cxx
)

target_compile_features(AppGameToolbox PUBLIC cxx_std_17)
target_include_directories(AppGameToolbox PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
