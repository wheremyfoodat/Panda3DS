# We need to be able to use enable_language(OBJC) on Mac, so we need CMake 3.16 vs the 3.10 we use otherwise. Blame Apple.
if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version")
    cmake_minimum_required(VERSION 3.16)
else()
    cmake_minimum_required(VERSION 3.10)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 12)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fbracket-depth=4096")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

project(Alber)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(APPLE)
    enable_language(OBJC)
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-nonliteral -Wno-format-security")
endif() 

option(DISABLE_PANIC_DEV "Make a build with fewer and less intrusive asserts" OFF)
option(GPU_DEBUG_INFO "Enable additional GPU debugging info" OFF)
option(ENABLE_OPENGL "Enable OpenGL rendering backend" ON)
option(ENABLE_VULKAN "Enable Vulkan rendering backend" ON)
option(ENABLE_LTO "Enable link-time optimization" OFF)
option(ENABLE_USER_BUILD "Make a user-facing build. These builds have various assertions disabled, LTO, and more" OFF)
option(ENABLE_HTTP_SERVER "Enable HTTP server. Used for Discord bot support" OFF)
option(ENABLE_DISCORD_RPC "Compile with Discord RPC support (disabled by default)" ON)

include_directories(${PROJECT_SOURCE_DIR}/include/)
include_directories(${PROJECT_SOURCE_DIR}/include/kernel)
include_directories (${FMT_INCLUDE_DIR})
include_directories(third_party/boost/)
include_directories(third_party/elfio/)
include_directories(third_party/imgui/)
include_directories(third_party/dynarmic/src)
include_directories(third_party/cryptopp/)
include_directories(third_party/cityhash/include)
include_directories(third_party/result/include)
include_directories(third_party/xxhash/include)
include_directories(third_party/httplib)
include_directories(third_party/stb)
include_directories(third_party/opengl)

add_compile_definitions(NOMINMAX)             # Make windows.h not define min/max macros because third-party deps don't like it
add_compile_definitions(WIN32_LEAN_AND_MEAN)  # Make windows.h not include literally everything
add_compile_definitions(SDL_MAIN_HANDLED)

if(ENABLE_DISCORD_RPC AND NOT ANDROID)
    add_subdirectory(third_party/discord-rpc)
    include_directories(third_party/discord-rpc/include)
endif()

set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/SDL2)

add_subdirectory(third_party/toml11)
include_directories(${SDL2_INCLUDE_DIR})
include_directories(third_party/toml11)
include_directories(third_party/glm)

add_subdirectory(third_party/cmrc)

set(BOOST_ROOT "${CMAKE_SOURCE_DIR}/third_party/boost")
set(Boost_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/boost")
set(Boost_NO_SYSTEM_PATHS ON)
add_compile_definitions(BOOST_NO_CXX98_FUNCTION_BASE) # Forbid Boost from using std::unary_function (Fixes MacOS build)

add_library(boost INTERFACE)
target_include_directories(boost SYSTEM INTERFACE ${Boost_INCLUDE_DIR})

set(CRYPTOPP_BUILD_TESTING OFF)
add_subdirectory(third_party/cryptopp)
add_subdirectory(third_party/glad)

# Check for x64
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86-64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(HOST_X64 TRUE)
    add_subdirectory(third_party/xbyak) # Add xbyak submodule for x86 JITs
    include_directories(third_party/xbyak)
    add_compile_definitions(PANDA3DS_DYNAPICA_SUPPORTED)
    add_compile_definitions(PANDA3DS_X64_HOST)
else()
    set(HOST_X64 FALSE)
endif()

# Check for arm64
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(HOST_ARM64 TRUE)
    add_compile_definitions(PANDA3DS_ARM64_HOST)
else()
    set(HOST_ARM64 FALSE)
endif()

if(HOST_X64 OR HOST_ARM64)
    set(DYNARMIC_TESTS OFF)
    #set(DYNARMIC_NO_BUNDLED_FMT ON)
    set(DYNARMIC_FRONTENDS "A32" CACHE STRING "")
    add_subdirectory(third_party/dynarmic)
    add_compile_definitions(CPU_DYNARMIC)
else()
    message(FATAL_ERROR "Currently unsupported CPU architecture")
endif()

set(SOURCE_FILES src/main.cpp src/emulator.cpp src/io_file.cpp src/config.cpp
				 src/core/CPU/cpu_dynarmic.cpp src/core/CPU/dynarmic_cycles.cpp
				 src/core/memory.cpp src/renderer.cpp src/core/renderer_null/renderer_null.cpp
				 src/http_server.cpp src/stb_image_write.c src/core/cheats.cpp src/core/action_replay.cpp
                 src/discord_rpc.cpp
)
set(CRYPTO_SOURCE_FILES src/core/crypto/aes_engine.cpp)
set(KERNEL_SOURCE_FILES src/core/kernel/kernel.cpp src/core/kernel/resource_limits.cpp
                        src/core/kernel/memory_management.cpp src/core/kernel/ports.cpp
                        src/core/kernel/events.cpp src/core/kernel/threads.cpp
                        src/core/kernel/address_arbiter.cpp src/core/kernel/error.cpp
                        src/core/kernel/file_operations.cpp src/core/kernel/directory_operations.cpp
                        src/core/kernel/idle_thread.cpp src/core/kernel/timers.cpp
)
set(SERVICE_SOURCE_FILES src/core/services/service_manager.cpp src/core/services/apt.cpp src/core/services/hid.cpp
                         src/core/services/fs.cpp src/core/services/gsp_gpu.cpp src/core/services/gsp_lcd.cpp
                         src/core/services/ndm.cpp src/core/services/dsp.cpp src/core/services/cfg.cpp
                         src/core/services/ptm.cpp src/core/services/mic.cpp src/core/services/cecd.cpp
                         src/core/services/ac.cpp src/core/services/am.cpp src/core/services/boss.cpp
                         src/core/services/frd.cpp src/core/services/nim.cpp src/core/services/mcu/mcu_hwc.cpp
                         src/core/services/y2r.cpp src/core/services/cam.cpp src/core/services/ldr_ro.cpp
                         src/core/services/act.cpp src/core/services/nfc.cpp src/core/services/dlp_srvr.cpp
                         src/core/services/ir_user.cpp src/core/services/http.cpp src/core/services/soc.cpp
                         src/core/services/ssl.cpp src/core/services/news_u.cpp
)
set(PICA_SOURCE_FILES src/core/PICA/gpu.cpp src/core/PICA/regs.cpp src/core/PICA/shader_unit.cpp
                      src/core/PICA/shader_interpreter.cpp src/core/PICA/dynapica/shader_rec.cpp
                      src/core/PICA/dynapica/shader_rec_emitter_x64.cpp src/core/PICA/pica_hash.cpp
)

set(LOADER_SOURCE_FILES src/core/loader/elf.cpp src/core/loader/ncsd.cpp src/core/loader/ncch.cpp src/core/loader/lz77.cpp)
set(FS_SOURCE_FILES src/core/fs/archive_self_ncch.cpp src/core/fs/archive_save_data.cpp src/core/fs/archive_sdmc.cpp
                    src/core/fs/archive_ext_save_data.cpp src/core/fs/archive_ncch.cpp src/core/fs/romfs.cpp
                    src/core/fs/ivfc.cpp
)

set(RENDERER_SW_SOURCE_FILES src/core/renderer_sw/renderer_sw.cpp)

set(HEADER_FILES include/emulator.hpp include/helpers.hpp include/termcolor.hpp
                 include/cpu.hpp include/cpu_dynarmic.hpp include/memory.hpp include/renderer.hpp include/kernel/kernel.hpp
                 include/dynarmic_cp15.hpp include/kernel/resource_limits.hpp include/kernel/kernel_types.hpp
                 include/kernel/config_mem.hpp include/services/service_manager.hpp include/services/apt.hpp
                 include/kernel/handles.hpp include/services/hid.hpp include/services/fs.hpp
                 include/services/gsp_gpu.hpp include/services/gsp_lcd.hpp include/arm_defs.hpp include/renderer_null/renderer_null.hpp
                 include/PICA/gpu.hpp include/PICA/regs.hpp include/services/ndm.hpp
                 include/PICA/shader.hpp include/PICA/shader_unit.hpp include/PICA/float_types.hpp
                 include/logger.hpp include/loader/ncch.hpp include/loader/ncsd.hpp include/io_file.hpp
                 include/loader/lz77.hpp include/fs/archive_base.hpp include/fs/archive_self_ncch.hpp
                 include/services/dsp.hpp include/services/cfg.hpp include/services/region_codes.hpp
                 include/fs/archive_save_data.hpp include/fs/archive_sdmc.hpp include/services/ptm.hpp
                 include/services/mic.hpp include/services/cecd.hpp include/services/ac.hpp
                 include/services/am.hpp include/services/boss.hpp include/services/frd.hpp include/services/nim.hpp
                 include/fs/archive_ext_save_data.hpp include/fs/archive_ncch.hpp include/services/mcu/mcu_hwc.hpp
                 include/colour.hpp include/services/y2r.hpp include/services/cam.hpp include/services/ssl.hpp 
                 include/services/ldr_ro.hpp include/ipc.hpp include/services/act.hpp include/services/nfc.hpp
                 include/system_models.hpp include/services/dlp_srvr.hpp include/PICA/dynapica/pica_recs.hpp
                 include/PICA/dynapica/x64_regs.hpp include/PICA/dynapica/vertex_loader_rec.hpp include/PICA/dynapica/shader_rec.hpp
                 include/PICA/dynapica/shader_rec_emitter_x64.hpp include/PICA/pica_hash.hpp include/result/result.hpp
                 include/result/result_common.hpp include/result/result_fs.hpp include/result/result_fnd.hpp
                 include/result/result_gsp.hpp include/result/result_kernel.hpp include/result/result_os.hpp
                 include/crypto/aes_engine.hpp include/metaprogramming.hpp include/PICA/pica_vertex.hpp
                 include/config.hpp include/services/ir_user.hpp include/http_server.hpp include/cheats.hpp
                 include/action_replay.hpp include/renderer_sw/renderer_sw.hpp include/compiler_builtins.hpp
                 include/fs/romfs.hpp include/fs/ivfc.hpp include/discord_rpc.hpp include/services/http.hpp include/result/result_cfg.hpp
                 include/math_util.hpp include/services/soc.hpp include/services/news_u.hpp
)

cmrc_add_resource_library(
	resources_console_fonts
	NAMESPACE ConsoleFonts
	WHENCE "src/core/services/fonts/"
	"src/core/services/fonts/CitraSharedFontUSRelocated.bin"
)

set(THIRD_PARTY_SOURCE_FILES third_party/imgui/imgui.cpp
                             third_party/imgui/imgui_draw.cpp
                             third_party/imgui/imgui_tables.cpp
                             third_party/imgui/imgui_widgets.cpp
                             third_party/imgui/imgui_demo.cpp

                             third_party/cityhash/cityhash.cpp
                             third_party/xxhash/xxhash.c
)
source_group("Source Files\\Core" FILES ${SOURCE_FILES})
source_group("Source Files\\Core\\Crypto" FILES ${CRYPTO_SOURCE_FILES})
source_group("Source Files\\Core\\Filesystem" FILES ${FS_SOURCE_FILES})
source_group("Source Files\\Core\\Kernel" FILES ${KERNEL_SOURCE_FILES})
source_group("Source Files\\Core\\Loader" FILES ${LOADER_SOURCE_FILES})
source_group("Source Files\\Core\\Services" FILES ${SERVICE_SOURCE_FILES})
source_group("Source Files\\Core\\PICA" FILES ${PICA_SOURCE_FILES})
source_group("Source Files\\Core\\Software Renderer" FILES ${RENDERER_SW_SOURCE_FILES})
source_group("Source Files\\Third Party" FILES ${THIRD_PARTY_SOURCE_FILES})

set(RENDERER_GL_SOURCE_FILES "") # Empty by default unless we are compiling with the GL renderer
set(RENDERER_VK_SOURCE_FILES "") # Empty by default unless we are compiling with the VK renderer

if(ENABLE_OPENGL)
    # This may look weird but opengl.hpp is our header even if it's in the third_party folder
	set(RENDERER_GL_INCLUDE_FILES third_party/opengl/opengl.hpp
		include/renderer_gl/renderer_gl.hpp include/renderer_gl/textures.hpp
		include/renderer_gl/surfaces.hpp include/renderer_gl/surface_cache.hpp
		include/renderer_gl/gl_state.hpp
	)

	set(RENDERER_GL_SOURCE_FILES src/core/renderer_gl/renderer_gl.cpp
        src/core/renderer_gl/textures.cpp src/core/renderer_gl/etc1.cpp
        src/core/renderer_gl/gl_state.cpp src/host_shaders/opengl_display.frag
        src/host_shaders/opengl_display.vert src/host_shaders/opengl_vertex_shader.vert
        src/host_shaders/opengl_fragment_shader.frag
	)

    set(HEADER_FILES ${HEADER_FILES} ${RENDERER_GL_INCLUDE_FILES})
    source_group("Source Files\\Core\\OpenGL Renderer" FILES ${RENDERER_GL_SOURCE_FILES})

	cmrc_add_resource_library(
		resources_renderer_gl
		NAMESPACE RendererGL
		WHENCE "src/host_shaders/"
		"src/host_shaders/opengl_display.frag"
		"src/host_shaders/opengl_display.vert"
		"src/host_shaders/opengl_vertex_shader.vert"
		"src/host_shaders/opengl_fragment_shader.frag"
	)
endif()

if(ENABLE_VULKAN)
	find_package(
		Vulkan 1.3.206 REQUIRED
		COMPONENTS glslangValidator
	)

	set(RENDERER_VK_INCLUDE_FILES include/renderer_vk/renderer_vk.hpp
		include/renderer_vk/vulkan_api.hpp include/renderer_vk/vk_debug.hpp
	)

	set(RENDERER_VK_SOURCE_FILES src/core/renderer_vk/renderer_vk.cpp
		src/core/renderer_vk/vulkan_api.cpp src/core/renderer_vk/vk_debug.cpp
	)

    set(HEADER_FILES ${HEADER_FILES} ${RENDERER_VK_INCLUDE_FILES})
    source_group("Source Files\\Core\\Vulkan Renderer" FILES ${RENDERER_VK_SOURCE_FILES})

	cmrc_add_resource_library(
		resources_renderer_vk
		NAMESPACE RendererVK
		WHENCE "src/host_shaders/"
	)
endif()

source_group("Header Files\\Core" FILES ${HEADER_FILES})
set(ALL_SOURCES ${SOURCE_FILES} ${FS_SOURCE_FILES} ${CRYPTO_SOURCE_FILES} ${KERNEL_SOURCE_FILES} ${LOADER_SOURCE_FILES} ${SERVICE_SOURCE_FILES}
	${RENDERER_SW_SOURCE_FILES} ${PICA_SOURCE_FILES} ${THIRD_PARTY_SOURCE_FILES} ${HEADER_FILES})

if(ENABLE_OPENGL)
    # Add the OpenGL source files to ALL_SOURCES
    set(ALL_SOURCES ${ALL_SOURCES} ${RENDERER_GL_SOURCE_FILES})
endif()

if(ENABLE_VULKAN)
    # Add the Vulkan source files to ALL_SOURCES
    set(ALL_SOURCES ${ALL_SOURCES} ${RENDERER_VK_SOURCE_FILES})
endif()

add_executable(Alber ${ALL_SOURCES})

if(ENABLE_LTO OR ENABLE_USER_BUILD)
    set_target_properties(Alber PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

target_link_libraries(Alber PRIVATE dynarmic SDL2-static cryptopp glad resources_console_fonts)

if(ENABLE_DISCORD_RPC AND NOT ANDROID)
    target_compile_definitions(Alber PUBLIC "PANDA3DS_ENABLE_DISCORD_RPC=1")
    target_link_libraries(Alber PRIVATE discord-rpc)
endif()

if(ENABLE_OPENGL)
    target_compile_definitions(Alber PUBLIC "PANDA3DS_ENABLE_OPENGL=1")
    target_link_libraries(Alber PRIVATE resources_renderer_gl)
endif()

if(ENABLE_VULKAN)
    target_compile_definitions(Alber PUBLIC "PANDA3DS_ENABLE_VULKAN=1")
    target_link_libraries(Alber PRIVATE Vulkan::Vulkan resources_renderer_vk)
endif()

if(GPU_DEBUG_INFO)
    target_compile_definitions(Alber PRIVATE GPU_DEBUG_INFO=1)
endif()

if(ENABLE_USER_BUILD)
    target_compile_definitions(Alber PRIVATE PANDA3DS_USER_BUILD=1)
endif()

if(ENABLE_USER_BUILD OR DISABLE_PANIC_DEV)
    target_compile_definitions(Alber PRIVATE PANDA3DS_LIMITED_PANICS=1)
endif()

if(ENABLE_HTTP_SERVER)
    target_compile_definitions(Alber PRIVATE PANDA3DS_ENABLE_HTTP_SERVER=1)
endif()
