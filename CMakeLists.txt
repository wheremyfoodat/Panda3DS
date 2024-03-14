# We need to be able to use enable_language(OBJC) on Mac, so we need CMake 3.16 vs the 3.11 we use otherwise. Blame Apple.
if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
    cmake_minimum_required(VERSION 3.16)
else()
    cmake_minimum_required(VERSION 3.11)
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

option(DISABLE_PANIC_DEV "Make a build with fewer and less intrusive asserts" ON)
option(GPU_DEBUG_INFO "Enable additional GPU debugging info" OFF)
option(ENABLE_OPENGL "Enable OpenGL rendering backend" ON)
option(ENABLE_VULKAN "Enable Vulkan rendering backend" ON)
option(ENABLE_LTO "Enable link-time optimization" OFF)
option(ENABLE_TESTS "Compile unit-tests" OFF)
option(ENABLE_USER_BUILD "Make a user-facing build. These builds have various assertions disabled, LTO, and more" OFF)
option(ENABLE_HTTP_SERVER "Enable HTTP server. Used for Discord bot support" OFF)
option(ENABLE_DISCORD_RPC "Compile with Discord RPC support (disabled by default)" ON)
option(ENABLE_LUAJIT "Enable scripting with the Lua programming language" ON)
option(ENABLE_QT_GUI "Enable the Qt GUI. If not selected then the emulator uses a minimal SDL-based UI instead" OFF)
option(BUILD_HYDRA_CORE "Build a Hydra core" OFF)

if(BUILD_HYDRA_CORE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

add_library(AlberCore STATIC)

include_directories(${PROJECT_SOURCE_DIR}/include/)
include_directories(${PROJECT_SOURCE_DIR}/include/kernel)
include_directories(${FMT_INCLUDE_DIR})
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
include_directories(third_party/miniaudio)
include_directories(third_party/mio/single_include)

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

if (NOT ANDROID)
    add_subdirectory(third_party/SDL2)
    target_link_libraries(AlberCore PUBLIC SDL2-static)
endif()

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

if(ANDROID)
    set(CRYPTOPP_OPT_DISABLE_ASM ON CACHE BOOL "" FORCE)
    target_sources(AlberCore PRIVATE src/jni_driver.cpp)
    target_link_libraries(AlberCore PRIVATE EGL log)
endif()

set(CRYPTOPP_BUILD_TESTING OFF)
add_subdirectory(third_party/cryptopp)
add_subdirectory(third_party/glad)

if(ENABLE_LUAJIT)
    add_subdirectory(third_party/LuaJIT luajit)
    include_directories(third_party/LuaJIT/src ${CMAKE_BINARY_DIR}/luajit)
    set_target_properties(luajit PROPERTIES EXCLUDE_FROM_ALL 1)

    if(MSVC)
      target_compile_definitions(libluajit PRIVATE _CRT_SECURE_NO_WARNINGS)
      target_compile_definitions(minilua PRIVATE _CRT_SECURE_NO_WARNINGS)
      target_compile_definitions(buildvm PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    target_compile_definitions(AlberCore PUBLIC "PANDA3DS_ENABLE_LUA=1")
    target_link_libraries(AlberCore PRIVATE libluajit)
endif()

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
    add_subdirectory(third_party/oaknut) # Add Oaknut submodule for arm64 JITs
    include_directories(third_party/oaknut/include)
    add_compile_definitions(PANDA3DS_DYNAPICA_SUPPORTED)
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
add_subdirectory(third_party/teakra EXCLUDE_FROM_ALL)

set(SOURCE_FILES src/emulator.cpp src/io_file.cpp src/config.cpp
                 src/core/CPU/cpu_dynarmic.cpp src/core/CPU/dynarmic_cycles.cpp
                 src/core/memory.cpp src/renderer.cpp src/core/renderer_null/renderer_null.cpp
                 src/http_server.cpp src/stb_image_write.c src/core/cheats.cpp src/core/action_replay.cpp
                 src/discord_rpc.cpp src/lua.cpp src/memory_mapped_file.cpp src/miniaudio.cpp
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
                         src/core/services/ssl.cpp src/core/services/news_u.cpp src/core/services/amiibo_device.cpp
                         src/core/services/csnd.cpp src/core/services/nwm_uds.cpp
)
set(PICA_SOURCE_FILES src/core/PICA/gpu.cpp src/core/PICA/regs.cpp src/core/PICA/shader_unit.cpp
                      src/core/PICA/shader_interpreter.cpp src/core/PICA/dynapica/shader_rec.cpp
                      src/core/PICA/dynapica/shader_rec_emitter_x64.cpp src/core/PICA/pica_hash.cpp
                      src/core/PICA/dynapica/shader_rec_emitter_arm64.cpp
)

set(LOADER_SOURCE_FILES src/core/loader/elf.cpp src/core/loader/ncsd.cpp src/core/loader/ncch.cpp src/core/loader/3dsx.cpp src/core/loader/lz77.cpp)
set(FS_SOURCE_FILES src/core/fs/archive_self_ncch.cpp src/core/fs/archive_save_data.cpp src/core/fs/archive_sdmc.cpp
                    src/core/fs/archive_ext_save_data.cpp src/core/fs/archive_ncch.cpp src/core/fs/romfs.cpp
                    src/core/fs/ivfc.cpp src/core/fs/archive_user_save_data.cpp src/core/fs/archive_system_save_data.cpp
)

set(APPLET_SOURCE_FILES src/core/applets/applet.cpp src/core/applets/mii_selector.cpp src/core/applets/software_keyboard.cpp src/core/applets/applet_manager.cpp
                        src/core/applets/error_applet.cpp
)
set(AUDIO_SOURCE_FILES src/core/audio/dsp_core.cpp src/core/audio/null_core.cpp src/core/audio/teakra_core.cpp
                       src/core/audio/miniaudio_device.cpp
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
                 include/logger.hpp include/loader/ncch.hpp include/loader/ncsd.hpp include/loader/3dsx.hpp include/io_file.hpp
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
                 include/applets/applet.hpp include/applets/mii_selector.hpp include/math_util.hpp include/services/soc.hpp 
                 include/services/news_u.hpp include/applets/software_keyboard.hpp include/applets/applet_manager.hpp include/fs/archive_user_save_data.hpp
                 include/services/amiibo_device.hpp include/services/nfc_types.hpp include/swap.hpp include/services/csnd.hpp include/services/nwm_uds.hpp
                 include/fs/archive_system_save_data.hpp include/lua_manager.hpp include/memory_mapped_file.hpp include/hydra_icon.hpp
                 include/PICA/dynapica/shader_rec_emitter_arm64.hpp include/scheduler.hpp include/applets/error_applet.hpp
                 include/audio/dsp_core.hpp include/audio/null_core.hpp include/audio/teakra_core.hpp
                 include/audio/miniaudio_device.hpp include/ring_buffer.hpp
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

if(ENABLE_LUAJIT AND NOT ANDROID)
    # Build luv and libuv for Lua TCP server usage if we're not on Android
    include_directories(third_party/luv/src)
    include_directories(third_party/luv/deps/lua-compat-5.3/c-api)
    include_directories(third_party/libuv/include)
    set(THIRD_PARTY_SOURCE_FILES ${THIRD_PARTY_SOURCE_FILES} third_party/luv/src/luv.c)
    set(LIBUV_BUILD_SHARED OFF)

    add_subdirectory(third_party/libuv)
    target_link_libraries(AlberCore PRIVATE uv_a)
endif()

if(ENABLE_QT_GUI)
    include_directories(third_party/duckstation)
    set(THIRD_PARTY_SOURCE_FILES ${THIRD_PARTY_SOURCE_FILES} third_party/duckstation/window_info.cpp third_party/duckstation/gl/context.cpp)

    if(APPLE)
        set(THIRD_PARTY_SOURCE_FILES ${THIRD_PARTY_SOURCE_FILES} third_party/duckstation/gl/context_agl.mm)
    elseif(WIN32)
        set(THIRD_PARTY_SOURCE_FILES ${THIRD_PARTY_SOURCE_FILES} third_party/duckstation/gl/context_wgl.cpp)
    else()
        set(THIRD_PARTY_SOURCE_FILES ${THIRD_PARTY_SOURCE_FILES} third_party/duckstation/gl/context_egl.cpp third_party/duckstation/gl/context_egl_wayland.cpp
        third_party/duckstation/gl/context_egl_x11.cpp third_party/duckstation/gl/context_glx.cpp third_party/duckstation/gl/x11_window.cpp)
    endif()
endif()

source_group("Source Files\\Core" FILES ${SOURCE_FILES})
source_group("Source Files\\Core\\Crypto" FILES ${CRYPTO_SOURCE_FILES})
source_group("Source Files\\Core\\Filesystem" FILES ${FS_SOURCE_FILES})
source_group("Source Files\\Core\\Kernel" FILES ${KERNEL_SOURCE_FILES})
source_group("Source Files\\Core\\Loader" FILES ${LOADER_SOURCE_FILES})
source_group("Source Files\\Core\\Services" FILES ${SERVICE_SOURCE_FILES})
source_group("Source Files\\Core\\Applets" FILES ${APPLET_SOURCE_FILES})
source_group("Source Files\\Core\\PICA" FILES ${PICA_SOURCE_FILES})
source_group("Source Files\\Core\\Audio" FILES ${AUDIO_SOURCE_FILES})
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

    target_sources(AlberCore PRIVATE ${RENDERER_GL_SOURCE_FILES})
    target_compile_definitions(AlberCore PUBLIC "PANDA3DS_ENABLE_OPENGL=1")
    target_link_libraries(AlberCore PRIVATE resources_renderer_gl)
endif()

if(ENABLE_VULKAN)
    find_package(
        Vulkan 1.3.206 REQUIRED
        COMPONENTS glslangValidator
    )

    set(RENDERER_VK_INCLUDE_FILES include/renderer_vk/renderer_vk.hpp
        include/renderer_vk/vk_api.hpp include/renderer_vk/vk_debug.hpp
        include/renderer_vk/vk_descriptor_heap.hpp
        include/renderer_vk/vk_descriptor_update_batch.hpp
        include/renderer_vk/vk_sampler_cache.hpp
        include/renderer_vk/vk_memory.hpp include/renderer_vk/vk_pica.hpp
    )

    set(RENDERER_VK_SOURCE_FILES src/core/renderer_vk/renderer_vk.cpp
        src/core/renderer_vk/vk_api.cpp src/core/renderer_vk/vk_debug.cpp
        src/core/renderer_vk/vk_descriptor_heap.cpp
        src/core/renderer_vk/vk_descriptor_update_batch.cpp
        src/core/renderer_vk/vk_sampler_cache.cpp
        src/core/renderer_vk/vk_memory.cpp src/core/renderer_vk/vk_pica.cpp
    )

    set(HEADER_FILES ${HEADER_FILES} ${RENDERER_VK_INCLUDE_FILES})
    source_group("Source Files\\Core\\Vulkan Renderer" FILES ${RENDERER_VK_SOURCE_FILES})

    set(RENDERER_VK_HOST_SHADERS_SOURCE
        "src/host_shaders/vulkan_display.frag"
        "src/host_shaders/vulkan_display.vert"
    )

    set(RENDERER_VK_HOST_SHADERS_FLAGS -e main --target-env vulkan1.1)

    if(GPU_DEBUG_INFO)
        # generate nonsemantic shader debug information with source
        set(RENDERER_VK_HOST_SHADERS_FLAGS ${RENDERER_VK_HOST_SHADERS_FLAGS} -gVS)
    else()
        set(RENDERER_VK_HOST_SHADERS_FLAGS ${RENDERER_VK_HOST_SHADERS_FLAGS} -g0)
    endif()

    # Compile each vulkan shader into an .spv file
    foreach( HOST_SHADER_SOURCE ${RENDERER_VK_HOST_SHADERS_SOURCE} )
        get_filename_component( FILE_NAME ${HOST_SHADER_SOURCE} NAME )
        set( HOST_SHADER_SPIRV "${PROJECT_BINARY_DIR}/host_shaders/${FILE_NAME}.spv" )
        add_custom_command(
            OUTPUT ${HOST_SHADER_SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/host_shaders/"
            COMMAND Vulkan::glslangValidator ${RENDERER_VK_HOST_SHADERS_FLAGS} -V "${PROJECT_SOURCE_DIR}/${HOST_SHADER_SOURCE}" -o ${HOST_SHADER_SPIRV}
            DEPENDS ${HOST_SHADER_SOURCE}
        )
        list( APPEND RENDERER_VK_HOST_SHADERS_SPIRV ${HOST_SHADER_SPIRV} )
    endforeach()

    cmrc_add_resource_library(
        resources_renderer_vk
        NAMESPACE RendererVK
        WHENCE "${PROJECT_BINARY_DIR}/host_shaders/"
        ${RENDERER_VK_HOST_SHADERS_SPIRV}
    )

    target_sources(AlberCore PRIVATE ${RENDERER_VK_SOURCE_FILES})
    target_compile_definitions(AlberCore PUBLIC "PANDA3DS_ENABLE_VULKAN=1")
    target_link_libraries(AlberCore PRIVATE Vulkan::Vulkan resources_renderer_vk)
endif()

source_group("Header Files\\Core" FILES ${HEADER_FILES})
set(ALL_SOURCES ${SOURCE_FILES} ${FS_SOURCE_FILES} ${CRYPTO_SOURCE_FILES} ${KERNEL_SOURCE_FILES} 
    ${LOADER_SOURCE_FILES} ${SERVICE_SOURCE_FILES} ${APPLET_SOURCE_FILES} ${RENDERER_SW_SOURCE_FILES} ${PICA_SOURCE_FILES} ${THIRD_PARTY_SOURCE_FILES}
    ${AUDIO_SOURCE_FILES} ${HEADER_FILES} ${FRONTEND_HEADER_FILES})
target_sources(AlberCore PRIVATE ${ALL_SOURCES})

target_link_libraries(AlberCore PRIVATE dynarmic cryptopp glad resources_console_fonts teakra)
target_link_libraries(AlberCore PUBLIC glad)

if(ENABLE_DISCORD_RPC AND NOT ANDROID)
    target_compile_definitions(AlberCore PUBLIC "PANDA3DS_ENABLE_DISCORD_RPC=1")
    target_link_libraries(AlberCore PRIVATE discord-rpc)
endif()

if(GPU_DEBUG_INFO)
    target_compile_definitions(AlberCore PRIVATE GPU_DEBUG_INFO=1)
endif()

if(ENABLE_USER_BUILD)
    target_compile_definitions(AlberCore PRIVATE PANDA3DS_USER_BUILD=1)
endif()

if(ENABLE_USER_BUILD OR DISABLE_PANIC_DEV)
    target_compile_definitions(AlberCore PRIVATE PANDA3DS_LIMITED_PANICS=1)
endif()

if(ENABLE_HTTP_SERVER)
    target_compile_definitions(AlberCore PRIVATE PANDA3DS_ENABLE_HTTP_SERVER=1)
endif()

# Configure frontend

if(ENABLE_QT_GUI)
    target_compile_definitions(AlberCore PUBLIC "PANDA3DS_FRONTEND_QT=1")
else()
    target_compile_definitions(AlberCore PUBLIC "PANDA3DS_FRONTEND_SDL=1")
endif()

if(NOT BUILD_HYDRA_CORE)
    add_executable(Alber)

    if(ENABLE_QT_GUI)
        find_package(Qt6 REQUIRED COMPONENTS Widgets)
        if(NOT ENABLE_OPENGL)
            message(FATAL_ERROR "Qt frontend requires OpenGL")
        endif()

        set(FRONTEND_SOURCE_FILES src/panda_qt/main.cpp src/panda_qt/screen.cpp src/panda_qt/main_window.cpp src/panda_qt/about_window.cpp
            src/panda_qt/config_window.cpp src/panda_qt/zep.cpp src/panda_qt/text_editor.cpp src/panda_qt/cheats_window.cpp
        )   
        set(FRONTEND_HEADER_FILES include/panda_qt/screen.hpp include/panda_qt/main_window.hpp include/panda_qt/about_window.hpp
            include/panda_qt/config_window.hpp include/panda_qt/text_editor.hpp include/panda_qt/cheats_window.hpp
        )

        source_group("Source Files\\Qt" FILES ${FRONTEND_SOURCE_FILES})
        source_group("Header Files\\Qt" FILES ${FRONTEND_HEADER_FILES})
        include_directories(${Qt6Gui_PRIVATE_INCLUDE_DIRS})

        include_directories(third_party/zep/include) # Include zep for text editor usage
        configure_file(third_party/zep/cmake/config_app.h.cmake ${CMAKE_BINARY_DIR}/zep_config/config_app.h)
        include_directories(${CMAKE_BINARY_DIR}/zep_config)

        target_compile_definitions(Alber PUBLIC "ZEP_QT=1")
        target_compile_definitions(Alber PUBLIC "ZEP_FEATURE_CPP_FILE_SYSTEM=1")
        target_link_libraries(Alber PRIVATE Qt6::Widgets)

        # We can't use qt_standard_project_setup since it's Qt 6.3+ and we don't need to set the minimum that high
        set_target_properties(Alber PROPERTIES AUTOMOC ON)
        set_target_properties(Alber PROPERTIES AUTORCC ON)
        set_target_properties(Alber PROPERTIES AUTOUIC ON)

        if(LINUX OR FREEBSD)
            find_package(X11 REQUIRED)
            target_link_libraries(Alber PRIVATE ${X11_LIBRARIES})

            if(ENABLE_OPENGL)
                find_package(OpenGL REQUIRED COMPONENTS OpenGL EGL GLX)
                target_link_libraries(Alber PRIVATE OpenGL::OpenGL OpenGL::EGL OpenGL::GLX)
            endif()
        endif()

        qt_add_resources(AlberCore "app_images"
            PREFIX "/"
            FILES
                docs/img/rsob_icon.png docs/img/rstarstruck_icon.png
        )
    else()
        set(FRONTEND_SOURCE_FILES src/panda_sdl/main.cpp src/panda_sdl/frontend_sdl.cpp)
        set(FRONTEND_HEADER_FILES "")
    endif()

    target_link_libraries(Alber PRIVATE AlberCore)
    target_sources(Alber PRIVATE ${FRONTEND_SOURCE_FILES} ${FRONTEND_HEADER_FILES})
elseif(BUILD_HYDRA_CORE)
    target_compile_definitions(AlberCore PRIVATE PANDA3DS_HYDRA_CORE=1)
    include_directories(third_party/hydra_core/include)
    add_library(Alber SHARED src/hydra_core.cpp)
    target_link_libraries(Alber PUBLIC AlberCore)
endif()

if(ENABLE_LTO OR ENABLE_USER_BUILD)
    set_target_properties(Alber PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(ENABLE_TESTS)
    enable_testing()

    find_package(Catch2 3)
    if(NOT Catch2_FOUND)
        add_subdirectory(third_party/Catch2)
    endif()

    add_library(nihstro-headers INTERFACE)
    target_include_directories(nihstro-headers SYSTEM INTERFACE ./third_party/nihstro/include)

    add_executable(AlberTests
        tests/shader.cpp
    )
    target_link_libraries(
        AlberTests
        PRIVATE
        Catch2::Catch2WithMain
        AlberCore
        nihstro-headers
    )

    add_test(AlberTests AlberTests)
endif()
