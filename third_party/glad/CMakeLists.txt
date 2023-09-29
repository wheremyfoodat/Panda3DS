set(GLAD_FILES src/gl.c include/KHR/khrplatform.h)

if(WIN32)
    set(GLAD_FILES ${GLAD_FILES} src/glad_wgl.c)
elseif(NOT APPLE)
    set(GLAD_FILES ${GLAD_FILES} src/glad_egl.c src/glad_glx.c)
endif()

add_library(glad STATIC ${GLAD_FILES})

target_include_directories(glad PUBLIC "include")
