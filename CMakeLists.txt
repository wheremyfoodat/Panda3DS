cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fbracket-depth=4096")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

project(Alber)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include_directories(${PROJECT_SOURCE_DIR}/include/)
include_directories(${PROJECT_SOURCE_DIR}/include/kernel)
include_directories (${FMT_INCLUDE_DIR})
include_directories(third_party/boost/)
include_directories(third_party/elfio/)
include_directories(third_party/gl3w/)
include_directories(third_party/imgui/)
include_directories(third_party/dynarmic/src)
include_directories(third_party/cryptopp/)

add_compile_definitions(NOMINMAX)
add_compile_definitions(SDL_MAIN_HANDLED)

set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/SDL2)
include_directories(${SDL2_INCLUDE_DIR})

set(BOOST_ROOT "${CMAKE_SOURCE_DIR}/third_party/boost")
set(Boost_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/boost")
set(Boost_NO_SYSTEM_PATHS ON)
add_library(boost INTERFACE)
target_include_directories(boost SYSTEM INTERFACE ${Boost_INCLUDE_DIR})

add_subdirectory(third_party/cryptopp)

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

set(SOURCE_FILES src/main.cpp src/emulator.cpp src/core/CPU/cpu_dynarmic.cpp src/core/memory.cpp)
set(KERNEL_SOURCE_FILES src/core/kernel/kernel.cpp src/core/kernel/resource_limits.cpp
                        src/core/kernel/memory_management.cpp src/core/kernel/ports.cpp
                        src/core/kernel/events.cpp src/core/kernel/threads.cpp
                        src/core/kernel/address_arbiter.cpp src/core/kernel/error.cpp
                        src/core/kernel/file_operations.cpp
)
set(SERVICE_SOURCE_FILES src/core/services/service_manager.cpp src/core/services/apt.cpp src/core/services/hid.cpp
                         src/core/services/fs.cpp src/core/services/gsp_gpu.cpp src/core/services/gsp_lcd.cpp
                         src/core/services/ndm.cpp src/core/services/dsp.cpp src/core/services/cfg.cpp
                         src/core/services/ptm.cpp src/core/services/mic.cpp src/core/services/cecd.cpp
                         src/core/services/ac.cpp src/core/services/am.cpp src/core/services/boss.cpp
                         src/core/services/frd.cpp src/core/services/nim.cpp
)
set(PICA_SOURCE_FILES src/core/PICA/gpu.cpp src/core/PICA/regs.cpp src/core/PICA/shader_unit.cpp
                      src/core/PICA/shader_interpreter.cpp
)
set(RENDERER_GL_SOURCE_FILES src/core/renderer_gl/renderer_gl.cpp)

set(LOADER_SOURCE_FILES src/core/loader/elf.cpp src/core/loader/ncsd.cpp src/core/loader/ncch.cpp)
set(FS_SOURCE_FILES src/core/fs/archive_ncch.cpp src/core/fs/archive_save_data.cpp src/core/fs/archive_sdmc.cpp)

set(HEADER_FILES include/emulator.hpp include/helpers.hpp include/opengl.hpp include/termcolor.hpp
                 include/cpu.hpp include/cpu_dynarmic.hpp include/memory.hpp include/kernel/kernel.hpp
                 include/dynarmic_cp15.hpp include/kernel/resource_limits.hpp include/kernel/kernel_types.hpp
                 include/kernel/config_mem.hpp include/services/service_manager.hpp include/services/apt.hpp
                 include/kernel/handles.hpp include/services/hid.hpp include/services/fs.hpp
                 include/services/gsp_gpu.hpp include/services/gsp_lcd.hpp include/arm_defs.hpp
                 include/PICA/gpu.hpp include/PICA/regs.hpp include/services/ndm.hpp
                 include/PICA/shader.hpp include/PICA/shader_unit.hpp include/PICA/float_types.hpp
                 include/logger.hpp include/loader/ncch.hpp include/loader/ncsd.hpp include/io_file.hpp
                 include/loader/lz77.hpp include/fs/archive_base.hpp include/fs/archive_ncch.hpp
                 include/services/dsp.hpp include/services/cfg.hpp include/services/region_codes.hpp
                 include/fs/archive_save_data.hpp include/fs/archive_sdmc.hpp include/services/ptm.hpp
                 include/services/mic.hpp include/services/cecd.hpp include/renderer_gl/renderer_gl.hpp
                 include/renderer_gl/surfaces.hpp include/renderer_gl/surface_cache.hpp include/services/ac.hpp
                 include/services/am.hpp include/services/boss.hpp include/services/frd.hpp include/services/nim.hpp
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
source_group("Source Files\\Core\\Filesystem" FILES ${FS_SOURCE_FILES})
source_group("Source Files\\Core\\Kernel" FILES ${KERNEL_SOURCE_FILES})
source_group("Source Files\\Core\\Loader" FILES ${LOADER_SOURCE_FILES})
source_group("Source Files\\Core\\Services" FILES ${SERVICE_SOURCE_FILES})
source_group("Source Files\\Core\\PICA" FILES ${PICA_SOURCE_FILES})
source_group("Source Files\\Core\\OpenGL Renderer" FILES ${RENDERER_GL_SOURCE_FILES})
source_group("Source Files\\Third Party" FILES ${THIRD_PARTY_SOURCE_FILES})

add_executable(Alber ${SOURCE_FILES} ${FS_SOURCE_FILES} ${KERNEL_SOURCE_FILES} ${LOADER_SOURCE_FILES} ${SERVICE_SOURCE_FILES}
${PICA_SOURCE_FILES} ${RENDERER_GL_SOURCE_FILES} ${THIRD_PARTY_SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(Alber PRIVATE dynarmic SDL2-static)