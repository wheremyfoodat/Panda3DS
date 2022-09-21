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
include_directories(${PROJECT_SOURCE_DIR}/include/kernel)
include_directories (${SFML_INCLUDE_DIR})
include_directories (${FMT_INCLUDE_DIR})
include_directories(third_party/elfio/)
include_directories(third_party/gl3w/)
include_directories(third_party/imgui/)
include_directories(third_party/dynarmic/src)

set(BOOST_ROOT "${CMAKE_SOURCE_DIR}/third_party/boost")
set(Boost_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/boost")
set(Boost_NO_SYSTEM_PATHS ON)
add_library(boost INTERFACE)
target_include_directories(boost SYSTEM INTERFACE ${Boost_INCLUDE_DIR})
add_compile_definitions(NOMINMAX)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86-64")
    set(DYNARMIC_TESTS OFF)
    #set(DYNARMIC_NO_BUNDLED_FMT ON)
    set(DYNARMIC_FRONTENDS "A32" CACHE STRING "")
    add_subdirectory(third_party/dynarmic)
    add_compile_definitions(CPU_DYNARMIC)
else()
    add_compile_definitions(CPU_KVM)
    message(FATAL_ERROR "THIS IS NOT x64 WAIT FOR THE KVM IMPLEMENTATION")
endif()

set(SOURCE_FILES src/main.cpp src/emulator.cpp src/core/CPU/cpu_dynarmic.cpp src/core/memory.cpp src/core/elf.cpp)
set(KERNEL_SOURCE_FILES src/core/kernel/kernel.cpp src/core/kernel/resource_limits.cpp
                        src/core/kernel/memory_management.cpp src/core/kernel/ports.cpp
                        src/core/kernel/events.cpp src/core/kernel/threads.cpp
                        src/core/kernel/address_arbiter.cpp
)
set(SERVICE_SOURCE_FILES src/core/services/service_manager.cpp src/core/services/apt.cpp src/core/services/hid.cpp
                         src/core/services/fs.cpp src/core/services/gsp_gpu.cpp src/core/services/gsp_lcd.cpp
)
set(PICA_SOURCE_FILES src/core/PICA/gpu.cpp)

set(HEADER_FILES include/emulator.hpp include/helpers.hpp include/opengl.hpp include/termcolor.hpp
                 include/cpu.hpp include/cpu_dynarmic.hpp include/memory.hpp include/kernel/kernel.hpp
                 include/dynarmic_cp15.hpp include/kernel/resource_limits.hpp include/kernel/kernel_types.hpp
                 include/kernel/config_mem.hpp include/services/service_manager.hpp include/services/apt.hpp
                 include/kernel/handles.hpp include/services/hid.hpp include/services/fs.hpp
                 include/services/gsp_gpu.hpp include/services/gsp_lcd.hpp include/arm_defs.hpp
                 include/gpu.hpp
)

set(THIRD_PARTY_SOURCE_FILES third_party/imgui/imgui.cpp
                             third_party/imgui/imgui_draw.cpp
                             third_party/imgui/imgui_tables.cpp
                             third_party/imgui/imgui_widgets.cpp
                             third_party/imgui/imgui_demo.cpp
                             third_party/gl3w/gl3w.cpp
)

#add_library(Alber ${HEADER_FILES})
source_group("Header Files\\Core" FILES ${HEADER_FILES})
source_group("Source Files\\Core" FILES ${SOURCE_FILES})
source_group("Source Files\\Core\\Kernel" FILES ${KERNEL_SOURCE_FILES})
source_group("Source Files\\Core\\Services" FILES ${SERVICE_SOURCE_FILES})
source_group("Source Files\\Core\\PICA" FILES ${PICA_SOURCE_FILES})
source_group("Source Files\\Third Party" FILES ${THIRD_PARTY_SOURCE_FILES})

add_executable(Alber ${SOURCE_FILES} ${KERNEL_SOURCE_FILES} ${SERVICE_SOURCE_FILES} ${PICA_SOURCE_FILES}
${THIRD_PARTY_SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(Alber PRIVATE sfml-system sfml-network sfml-graphics sfml-window dynarmic)