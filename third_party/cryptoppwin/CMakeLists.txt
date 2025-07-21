add_library(cryptoppwin SHARED IMPORTED GLOBAL)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_target_properties(cryptoppwin PROPERTIES IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/cryptlibd.lib")
else()
    set_target_properties(cryptoppwin PROPERTIES IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/cryptlib.lib")
endif()