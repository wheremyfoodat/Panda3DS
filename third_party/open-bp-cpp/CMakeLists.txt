# CMakeList.txt : CMake project for buttplugCpp, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project(buttplugCpp)

# Add source to this project's executable.

file(GLOB SRC_FILES    
    "source/*.cpp"
    "include/*.h"
    "include/*.hpp"
)

add_library(buttplugCpp STATIC ${SRC_FILES})

set(USE_ZLIB FALSE)
target_include_directories(buttplugCpp PUBLIC third_party/IXWebSocket/ixwebsocket)
target_include_directories(buttplugCpp PUBLIC include)
add_subdirectory(third_party/IXWebSocket)

find_package(Threads REQUIRED)
target_link_libraries(buttplugCpp PUBLIC ixwebsocket::ixwebsocket Threads::Threads)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET buttplugCpp PROPERTY CXX_STANDARD 11)
endif()
# TODO: Add tests and install targets if needed.
