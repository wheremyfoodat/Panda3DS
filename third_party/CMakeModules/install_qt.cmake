# Adapted from https://github.com/citra-emu/citra/blob/master/CMakeModules/DownloadExternals.cmake

# This function downloads Qt using aqt. The path of the downloaded content will be added to the CMAKE_PREFIX_PATH.
# Params:
#   target: Qt dependency to install. Specify a version number to download Qt, or "tools_(name)" for a specific build tool.
function(download_qt target)
    # Detect architecture to find which Qt version to fetch
    if (CMAKE_OSX_ARCHITECTURES)
        set(ARCHITECTURE "${CMAKE_OSX_ARCHITECTURES}")
    elseif (MSVC)
        detect_architecture("_M_AMD64" x86_64)
        detect_architecture("_M_IX86" x86)
        detect_architecture("_M_ARM" arm)
        detect_architecture("_M_ARM64" arm64)
    else()
        detect_architecture("__x86_64__" x86_64)
        detect_architecture("__i386__" x86)
        detect_architecture("__arm__" arm)
        detect_architecture("__aarch64__" arm64)
    endif()
    message(STATUS "Qt Downloader: Detected target architecture: ${ARCHITECTURE}")

    if (target MATCHES "tools_.*")
        set(DOWNLOAD_QT_TOOL ON)
    else()
        set(DOWNLOAD_QT_TOOL OFF)
    endif()

    # Determine installation parameters for OS, architecture, and compiler
    if (WIN32)
        set(host "windows")
        set(type "desktop")
        if (NOT DOWNLOAD_QT_TOOL)
            if (MINGW)
                set(arch "win64_mingw")
                set(arch_path "mingw_64")
            elseif (MSVC)
                if ("arm64" IN_LIST ARCHITECTURE)
                    set(arch_path "msvc2019_arm64")
                elseif ("x86_64" IN_LIST ARCHITECTURE)
                    set(arch_path "msvc2019_64")
                else()
                    message(FATAL_ERROR "Unsupported bundled Qt architecture. Enable USE_SYSTEM_QT and provide your own.")
                endif()
                set(arch "win64_${arch_path}")
            else()
                message(FATAL_ERROR "Unsupported bundled Qt toolchain. Enable USE_SYSTEM_QT and provide your own.")
            endif()
        endif()
    elseif (APPLE)
        set(host "mac")
        if (IOS AND NOT DOWNLOAD_QT_TOOL)
            set(type "ios")
            set(arch "ios")
            set(arch_path "ios")
            set(host_arch_path "macos")
        else()
            set(type "desktop")
            set(arch "clang_64")
            set(arch_path "macos")
        endif()
    else()
        set(host "linux")
        set(type "desktop")
        set(arch "gcc_64")
        set(arch_path "linux")
    endif()

    get_external_prefix(qt base_path)
    file(MAKE_DIRECTORY "${base_path}")

    if (DOWNLOAD_QT_TOOL)
        set(prefix "${base_path}/Tools")
        set(install_args install-tool --outputdir ${base_path} ${host} desktop ${target})
    else()
        set(prefix "${base_path}/${target}/${arch_path}")
        if (host_arch_path)
            set(host_flag "--autodesktop")
            set(host_prefix "${base_path}/${target}/${host_arch_path}")
        endif()
        set(install_args install-qt --outputdir ${base_path} ${host} ${type} ${target} ${arch} ${host_flag}
                                    -m qtmultimedia --archives qttranslations qttools qtsvg qtbase)
    endif()

    if (NOT EXISTS "${prefix}")
        message(STATUS "Downloading binaries for Qt...")
        if (WIN32)
            set(aqt_path "${base_path}/aqt.exe")
            file(DOWNLOAD
                https://github.com/miurahr/aqtinstall/releases/download/v3.1.7/aqt.exe
                ${aqt_path} SHOW_PROGRESS)
            execute_process(COMMAND ${aqt_path} ${install_args}
                    WORKING_DIRECTORY ${base_path})
        else()
            # aqt does not offer binary releases for other platforms, so download and run from pip.
            set(aqt_install_path "${base_path}/aqt")
            file(MAKE_DIRECTORY "${aqt_install_path}")

            execute_process(COMMAND python3 -m pip install --target=${aqt_install_path} aqtinstall
                    WORKING_DIRECTORY ${base_path})
            execute_process(COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${aqt_install_path} python3 -m aqt ${install_args}
                    WORKING_DIRECTORY ${base_path})
        endif()
    endif()

    message(STATUS "Using downloaded Qt binaries at ${prefix}")

    # Add the Qt prefix path so CMake can locate it.
    list(APPEND CMAKE_PREFIX_PATH "${prefix}")
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} PARENT_SCOPE)

    if (DEFINED host_prefix)
        message(STATUS "Using downloaded host Qt binaries at ${host_prefix}")
        set(QT_HOST_PATH "${host_prefix}" CACHE STRING "")
    endif()
endfunction()

function(get_external_prefix lib_name prefix_var)
    set(${prefix_var} "${CMAKE_BINARY_DIR}/externals/${lib_name}" PARENT_SCOPE)
endfunction()

function(detect_architecture symbol arch)
    include(CheckSymbolExists)
    if (NOT DEFINED ARCHITECTURE)
        set(CMAKE_REQUIRED_QUIET 1)
        check_symbol_exists("${symbol}" "" ARCHITECTURE_${arch})
        unset(CMAKE_REQUIRED_QUIET)

        # The output variable needs to be unique across invocations otherwise
        # CMake's crazy scope rules will keep it defined
        if (ARCHITECTURE_${arch})
            set(ARCHITECTURE "${arch}" PARENT_SCOPE)
        endif()
    endif()
endfunction()