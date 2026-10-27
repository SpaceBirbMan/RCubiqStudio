# POST_BUILD без GET_RUNTIME_DEPENDENCIES (CMake < 3.21): узкие glob по каталогам из STUDIO_PATHS_FILE.
# STUDIO_DEST задаётся через cmake -C studio_deploy_paths_<CONFIG>.cmake (см. CMakeLists M3).
cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED STUDIO_DEST OR NOT DEFINED STUDIO_PATHS_FILE)
    message(FATAL_ERROR "StudioDeployRuntimeFallback.cmake: задайте STUDIO_DEST и STUDIO_PATHS_FILE")
endif()
string(STRIP "${STUDIO_DEST}" STUDIO_DEST)
string(STRIP "${STUDIO_PATHS_FILE}" STUDIO_PATHS_FILE)
if(NOT EXISTS "${STUDIO_PATHS_FILE}")
    message(FATAL_ERROR "StudioDeployRuntimeFallback: не найден файл каталогов: ${STUDIO_PATHS_FILE}")
endif()

file(STRINGS "${STUDIO_PATHS_FILE}" _lines)
set(_search "")
foreach(line IN LISTS _lines)
    string(STRIP "${line}" line)
    if(NOT line STREQUAL "" AND NOT line MATCHES "^#")
        list(APPEND _search "${line}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _search)

set(_patterns
    "zlib*.dll"
    "zlib1.dll"
    "libz*.dll"
    "libbz2*.dll"
    "liblzma*.dll"
    "libpng*.dll"
    "jpeg*.dll"
    "libjpeg*.dll"
    "tiff*.dll"
    "libtiff*.dll"
    "webp*.dll"
    "libwebp*.dll"
    "libopencv*.dll"
    "opencv_world*.dll"
    "opencv_videoio_ffmpeg*.dll"
    "libstdc++*.dll"
    "libwinpthread*.dll"
    "libgcc_s*.dll"
    "libhidapi*.dll"
    "hidapi*.dll"
    "libusb*.dll"
    "usb*.dll"
    "bgfx*.dll"
    "bimg*.dll"
    "bx*.dll"
    "libuiohook*.dll"
    "uiohook*.dll"
)

foreach(dir IN LISTS _search)
    if(NOT IS_DIRECTORY "${dir}")
        continue()
    endif()
    foreach(pat IN LISTS _patterns)
        file(GLOB _hits "${dir}/${pat}")
        foreach(h IN LISTS _hits)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${h}" "${STUDIO_DEST}")
        endforeach()
    endforeach()
endforeach()
