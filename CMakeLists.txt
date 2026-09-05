cmake_minimum_required(VERSION 3.18)

project(AppGameToolbox LANGUAGES CXX)

add_library(AppGameToolbox STATIC
    src/UIElement2D.cxx
    src/Object3D.cxx
    src/Toolbox.cxx
    src/AudioSyncSystem.cxx
    src/AudioFX.cxx
    src/Render2D.cxx
    src/HtmlCss.cxx
    src/UITemplate.cxx
)

target_compile_features(AppGameToolbox PUBLIC cxx_std_17)
target_include_directories(AppGameToolbox PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    if(BUILD_TESTING)
        add_executable(AppGameToolboxTests tests/Render2DTests.cxx tests/HtmlCssTests.cxx tests/UITemplateTests.cxx)
        target_link_libraries(AppGameToolboxTests PRIVATE AppGameToolbox)
        add_test(NAME AppGameToolboxTests COMMAND AppGameToolboxTests)
    endif()
endif()
