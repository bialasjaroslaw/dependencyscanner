cmake_minimum_required(VERSION 3.12.0)
cmake_policy(SET CMP0048 NEW)
project(dependencyscanner VERSION 1.0.0)

include(GenerateExportHeader)

include(FetchContent)
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11
  GIT_TAG v2.2.0)

FetchContent_MakeAvailable(cli11)

set(CMAKE_CXX_STANDARD 20)
set(LIB_SOURCES
    include/androiddependencyextractor.h
    include/dependencyextractor.h
    include/dependency.h
    include/textutils.h
    src/androiddependencyextractor.cpp
    src/dependencyextractor.cpp
    src/textutils.cpp)

set(ANDROID_TOOL_SRC src/check.cpp)
set(ANDROID_PLUGIN_TOOL_SRC src/qtpluginresolver.cpp)

find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

add_library(dependency_extractor ${LIB_SOURCES})
target_include_directories(dependency_extractor
                           PUBLIC include ${CMAKE_CURRENT_BINARY_DIR})
generate_export_header(dependency_extractor)

add_executable(qtandroiddependencyscanner ${ANDROID_TOOL_SRC})
add_executable(qtpluginresolver ${ANDROID_PLUGIN_TOOL_SRC})
target_link_libraries(dependency_extractor PRIVATE fmt::fmt spdlog::spdlog)
target_link_libraries(
  qtandroiddependencyscanner
  PRIVATE dependency_extractor fmt::fmt spdlog::spdlog CLI11::CLI11
          nlohmann_json::nlohmann_json)
target_link_libraries(
  qtpluginresolver
  PRIVATE dependency_extractor fmt::fmt spdlog::spdlog CLI11::CLI11
          nlohmann_json::nlohmann_json)
