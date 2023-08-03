# Suppress warnings from external libraries
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W0)
else()
    add_compile_options(-w)
endif()

target_include_directories(Alber PRIVATE boost/)
target_include_directories(Alber PRIVATE elfio/)
target_include_directories(Alber PRIVATE imgui/)
target_include_directories(Alber PRIVATE cryptopp/)
target_include_directories(Alber PRIVATE cityhash/include)
target_include_directories(Alber PRIVATE result/include)
target_include_directories(Alber PRIVATE xxhash/include)
target_include_directories(Alber PRIVATE httplib)
target_include_directories(Alber PRIVATE stb)

set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL2)

add_subdirectory(toml11)
target_include_directories(Alber PRIVATE toml11)
target_include_directories(Alber PRIVATE glm)

set(BOOST_ROOT "${CMAKE_SOURCE_DIR}/third_party/boost")
set(Boost_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/boost")
set(Boost_NO_SYSTEM_PATHS ON)
add_compile_definitions(BOOST_NO_CXX98_FUNCTION_BASE) # Forbid Boost from using std::unary_function (Fixes MacOS build)

add_library(boost INTERFACE)
target_include_directories(boost SYSTEM INTERFACE ${Boost_INCLUDE_DIR})

set(CRYPTOPP_BUILD_TESTING OFF)
add_subdirectory(cryptopp)
add_subdirectory(glad)

# Check for x64
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86-64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(HOST_X64 TRUE)
    add_subdirectory(xbyak) # Add xbyak submodule for x86 JITs
    target_link_libraries(Alber PRIVATE xbyak)
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
    add_subdirectory(dynarmic)
    target_compile_definitions(Alber PRIVATE CPU_DYNARMIC)
else()
    message(FATAL_ERROR "Currently unsupported CPU architecture")
endif()
