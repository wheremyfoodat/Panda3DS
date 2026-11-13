# OpenEmu core plugin.
#
# Builds a loadable bundle (.oecoreplugin) that OpenEmu loads at runtime. It lives in this tree
# because it links the AlberCore static library built here, but it is a frontend of its own:
# nothing in the emulator depends on it, and it is only built when the OpenEmu frameworks are
# available.

if (NOT APPLE OR NOT ENABLE_METAL)
    return()
endif()

set(OPENEMU_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../OpenEmu" CACHE PATH
    "OpenEmu source tree, needed for the system plugin's responder protocol header")
set(OPENEMU_FRAMEWORK_DIR "/tmp/oe_arm64/Build/Products/Debug" CACHE PATH
    "Directory holding OpenEmuBase.framework and the other frameworks OpenEmu is built from")

find_library(OPENEMU_BASE_FRAMEWORK OpenEmuBase PATHS "${OPENEMU_FRAMEWORK_DIR}" NO_DEFAULT_PATH)
if (NOT OPENEMU_BASE_FRAMEWORK)
    message(STATUS "OpenEmuBase.framework not found in ${OPENEMU_FRAMEWORK_DIR}; skipping the OpenEmu core plugin")
    return()
endif()

add_library(Panda3DS MODULE OEPanda3DSCore.mm)

set_target_properties(Panda3DS PROPERTIES
    BUNDLE TRUE
    BUNDLE_EXTENSION "oecoreplugin"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    OUTPUT_NAME "Panda3DS"
)

# OpenEmu itself is ARC, and the game core is called from its threads, so match it
target_compile_options(Panda3DS PRIVATE -fobjc-arc "-F${OPENEMU_FRAMEWORK_DIR}")

target_include_directories(Panda3DS PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${OPENEMU_SOURCE_DIR}/SystemPlugins/3DS"
)

target_link_libraries(Panda3DS PRIVATE
    AlberCore
    "${OPENEMU_BASE_FRAMEWORK}"
    "-framework Metal"
    "-framework QuartzCore"
    "-framework OpenGL"
    "-framework Foundation"
)

# OpenEmu refuses plugins whose signature does not match the app's, and an ad hoc signature is
# enough for a locally built app. Set OPENEMU_CODE_SIGN_IDENTITY to use a real identity instead.
set(OPENEMU_CODE_SIGN_IDENTITY "-" CACHE STRING "Code signing identity for the OpenEmu plugins")
add_custom_command(TARGET Panda3DS POST_BUILD
    COMMAND codesign -f -s "${OPENEMU_CODE_SIGN_IDENTITY}" "$<TARGET_BUNDLE_DIR:Panda3DS>"
    COMMENT "Signing Panda3DS.oecoreplugin"
    VERBATIM
)
