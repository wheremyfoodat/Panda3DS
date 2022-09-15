cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fbracket-depth=4096")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

project(Alber)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(SFML_STATIC_LIBRARIES TRUE)
find_package(SFML COMPONENTS system window graphics CONFIG REQUIRED)

if(NOT SFML_FOUND)
  message(FATAL_ERROR "SFML couldn't be located!")
endif()

include_directories(${PROJECT_SOURCE_DIR}/include/)
include_directories (${SFML_INCLUDE_DIR})
include_directories (${FMT_INCLUDE_DIR})
include_directories(third_party/imgui/)
include_directories(third_party/gl3w/)
include_directories(third_party/dynarmic/src)

set(BOOST_ROOT "${CMAKE_SOURCE_DIR}/third_party/boost")
set(Boost_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/boost")
set(Boost_NO_SYSTEM_PATHS ON)
add_library(boost INTERFACE)
target_include_directories(boost SYSTEM INTERFACE ${Boost_INCLUDE_DIR})

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86-64")
    set(DYNARMIC_TESTS OFF)
    #set(DYNARMIC_NO_BUNDLED_FMT ON)
    set(DYNARMIC_FRONTENDS "A32" CACHE STRING "")
    add_subdirectory(third_party/dynarmic)
else()
    message(FATAL_ERROR "THIS IS NOT x64 WAIT FOR THE KVM IMPLEMENTATION")
endif()

set(SOURCE_FILES src/main.cpp src/emulator.cpp)
set(INCLUDE_FILES include/emulator.hpp include/helpers.hpp include/termcolor.hpp include/opengl.hpp)

set(THIRD_PARTY_SOURCE_FILES third_party/imgui/imgui.cpp
                             third_party/imgui/imgui_draw.cpp
                             third_party/imgui/imgui_tables.cpp
                             third_party/imgui/imgui_widgets.cpp
                             third_party/imgui/imgui_demo.cpp
                             third_party/gl3w/gl3w.cpp
)

source_group("Source Files\\Core" FILES ${SOURCE_FILES})
source_group("Include Files\\Core" FILES ${INCLUDE_FILES})
source_group("Source Files\\Third_Party" FILES ${THIRD_PARTY_SOURCE_FILES})

add_executable(Alber ${SOURCE_FILES} ${THIRD_PARTY_SOURCE_FILES})
target_link_libraries(Alber PRIVATE sfml-system sfml-network sfml-graphics sfml-window dynarmic)