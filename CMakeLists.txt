cmake_minimum_required(VERSION 3.16)

project(Studio VERSION 0.1 LANGUAGES CXX)

# cache.json, session_cache.json, plugins_cache и settings.ini рядом с exe (как в Debug portable).
option(STUDIO_FORCE_ROOT_CACHE "Force portable cache/settings roots next to executable (Release-friendly)." ON)

set(CPPTRACE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
include(FetchContent)
FetchContent_Declare(
    cpptrace
    GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
    GIT_TAG v1.0.4
)
FetchContent_MakeAvailable(cpptrace)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_VERSION_MAJOR 6)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Quick QuickWidgets LinguistTools Gui QuickControls2 Quick3D)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Quick QuickWidgets LinguistTools Gui QuickControls2 Quick3D)
find_package(PkgConfig REQUIRED)

set(STUDIO_TS_RU ${CMAKE_SOURCE_DIR}/i18n/Studio_ru_RU.ts)
set(QT_CREATOR_SKIP_MAINTENANCE_TOOL_PROVIDER ON)

set(CMAKE_AUTOUIC_SEARCH_PATHS
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/ui
    ${CMAKE_SOURCE_DIR}/shaders
)

# --- Сбор исходных файлов ---
# CONFIGURE_DEPENDS: при появлении новых .cpp CMake перезапускается (иначе GLOB «застывает» и линковка без .obj).
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "*.cpp"
    "*.h"
    "*.ui"
    "*.qml"
    "*.qrc"
    "*.qmodel"
    "*.sc"
)

# Явно подключаемые исходники (на случай устаревшего кэша GLOB); убираем дубликаты из списка GLOB.
set(STUDIO_EXPLICIT_SOURCES
    "${CMAKE_SOURCE_DIR}/src/appsettings.cpp"
    "${CMAKE_SOURCE_DIR}/src/settingsdialog.cpp"
)
set(_prj_filtered "")
foreach(s IN LISTS PROJECT_SOURCES)
    get_filename_component(_leaf_name "${s}" NAME)
    if(_leaf_name STREQUAL "appsettings.cpp" OR _leaf_name STREQUAL "settingsdialog.cpp")
        continue()
    endif()
    list(APPEND _prj_filtered "${s}")
endforeach()
set(PROJECT_SOURCES "${_prj_filtered}")
list(APPEND PROJECT_SOURCES ${STUDIO_EXPLICIT_SOURCES})

# нужны ли функции для частичной сборки (только ui, только весь интерфейс, т.д.)

# Исключаем файлы из папок сборки, если они вдруг попали в дерево проекта
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "build/.*")


# --- Настройка исполняемого файла ---
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Studio
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )

else()
    if(ANDROID)
        add_library(Studio SHARED ${PROJECT_SOURCES})
    else()
        add_executable(Studio ${PROJECT_SOURCES})
    endif()
endif()

# --- i18n: lrelease в CMake, явная зависимость от .ts (правки в Linguist сразу попадают в .qm) ---
# qt_create_translation часто не пересобирал .qm при одном лишь изменении .ts.
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    find_program(STUDIO_LRELEASE_TOOL NAMES lrelease-qt6 lrelease
        HINTS ${Qt6_DIR}/../../../bin ${Qt6_DIR}/../../bin
        DOC "Qt6 lrelease")
    find_program(STUDIO_LUPDATE_TOOL NAMES lupdate-qt6 lupdate
        HINTS ${Qt6_DIR}/../../../bin ${Qt6_DIR}/../../bin
        DOC "Qt6 lupdate")
else()
    find_program(STUDIO_LRELEASE_TOOL NAMES lrelease-qt5 lrelease
        HINTS ${Qt5_DIR}/../../../bin ${Qt5_DIR}/../../bin
        DOC "Qt5 lrelease")
    find_program(STUDIO_LUPDATE_TOOL NAMES lupdate-qt5 lupdate
        HINTS ${Qt5_DIR}/../../../bin
        DOC "Qt5 lupdate")
endif()
if(NOT STUDIO_LRELEASE_TOOL)
    message(FATAL_ERROR "lrelease not found (install Qt Linguist / Qt tools, or add to PATH).")
endif()

set(STUDIO_RU_QM ${CMAKE_CURRENT_BINARY_DIR}/Studio_ru_RU.qm)
add_custom_command(
    OUTPUT ${STUDIO_RU_QM}
    COMMAND ${STUDIO_LRELEASE_TOOL} ${STUDIO_TS_RU} -qm ${STUDIO_RU_QM}
    DEPENDS ${STUDIO_TS_RU}
    COMMENT "lrelease: i18n/Studio_ru_RU.ts -> Studio_ru_RU.qm (depends on .ts; edit strings in Qt Linguist)"
    VERBATIM
)
add_custom_target(Studio_lrelease_qm ALL DEPENDS ${STUDIO_RU_QM})
add_dependencies(Studio Studio_lrelease_qm)
set(QM_FILES ${STUDIO_RU_QM})

# Обновить .ts из исходников (сканируются каталоги, без перечисления каждого .cpp)
#   cmake --build <dir> --target Studio_lupdate
if(STUDIO_LUPDATE_TOOL)
    add_custom_target(Studio_lupdate
        COMMAND ${STUDIO_LUPDATE_TOOL} -locations relative
            "${CMAKE_SOURCE_DIR}/src"
            "${CMAKE_SOURCE_DIR}/ui"
            -ts ${STUDIO_TS_RU}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "lupdate: перенос tr()/ui в Studio_ru_RU.ts (после смены строк в коде)"
        VERBATIM
    )
endif()

# .ts в проекте Qt Creator: только исходник, не линкуется
target_sources(Studio PRIVATE ${STUDIO_TS_RU})
set_source_files_properties(${STUDIO_TS_RU} PROPERTIES HEADER_FILE_ONLY ON)

# --- Библиотеки и зависимости ---
add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE ${CMAKE_SOURCE_DIR}/../miniaudio-0.11.24)

find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio highgui)
pkg_check_modules(HIDAPI REQUIRED IMPORTED_TARGET hidapi)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

target_include_directories(Studio PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/shaders")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BGFX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../bgfx/install-mingw64-d")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(BGFX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../bgfx/install-mingw64-r")
endif()

set(BIN_DIRS "${BGFX_ROOT}/bin")

target_include_directories(Studio PRIVATE ${BGFX_ROOT}/include)
target_link_directories(Studio PRIVATE ${BGFX_ROOT}/lib)
set(UIOHOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libuiohook")
add_library(uiohook SHARED IMPORTED)
set_target_properties(uiohook PROPERTIES IMPORTED_IMPLIB
    "${UIOHOOK_DIR}/Debug/libuiohook.dll.a" IMPORTED_LOCATION
    "${UIOHOOK_DIR}/Debug/libuiohook.dll"
    INTERFACE_INCLUDE_DIRECTORIES
    "${UIOHOOK_DIR}/libuiohook/include" )
target_link_libraries(Studio PRIVATE uiohook)

add_compile_definitions(BX_CONFIG_DEBUG=1)

message(STATUS "OpenCV libs: ${OpenCV_LIBS}")

target_link_libraries(
    Studio PRIVATE
    cpptrace::cpptrace
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickWidgets
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::QuickControls2
    bgfx
    bx
    bimg
    miniaudio
    PkgConfig::HIDAPI
    PkgConfig::LIBUSB
    ${OpenCV_LIBS}
)

target_link_libraries(Studio PRIVATE ws2_32)

if(STUDIO_FORCE_ROOT_CACHE)
    message(STATUS "STUDIO_FORCE_ROOT_CACHE=ON: cache/settings/plugins_cache у каталога exe (включая Release)")
    target_compile_definitions(Studio PRIVATE STUDIO_FORCE_ROOT_CACHE=1)
endif()

if(WIN32)
    target_link_libraries(
        Studio PRIVATE
        winmm
        setupapi
        crypt32
        strmiids
        ole32
        oleaut32
        uuid
        shell32
        psapi
        dxgi
    )
    target_compile_definitions(Studio PRIVATE
        $<$<CONFIG:Debug>:STUDIO_PORTABLE_USER_DATA=1>
        $<$<NOT:$<CONFIG:Debug>>:STUDIO_PORTABLE_USER_DATA=0>
    )
endif()

# file(GLOB_RECURSE VERTEX_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.vs.sc")
# file(GLOB_RECURSE FRAGMENT_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.fs.sc")

# message(STATUS "Vertex shaders found: ${VERTEX_SHADERS}")
# message(STATUS "Fragment shaders found: ${FRAGMENT_SHADERS}")

# set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
# file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

# set(COMPILED_SHADERS "")

# function(compile_shader TYPE INPUT OUTPUT_DIR RENDERER PROFILE)
#     get_filename_component(FILE_NAME ${INPUT} NAME_WE)
#     set(OUTPUT_BIN "${OUTPUT_DIR}/${FILE_NAME}_${TYPE}.bin")

#     add_custom_command(
#         OUTPUT ${OUTPUT_BIN}
#         COMMAND "${BIN_DIRS}/shaderc"
#             -f ${INPUT}
#             -o ${OUTPUT_BIN}
#             --type ${TYPE}
#             --platform ${RENDERER}
#             --profile ${PROFILE}
#         DEPENDS ${INPUT}
#         COMMENT "Compiling ${TYPE} shader: ${INPUT} -> ${OUTPUT_BIN}"
#         VERBATIM
#     )

#     set(COMPILED_SHADERS ${COMPILED_SHADERS} ${OUTPUT_BIN} PARENT_SCOPE)
# endfunction()

# foreach(SHADER ${VERTEX_SHADERS})
#     compile_shader("vertex" ${SHADER} ${SHADER_OUTPUT_DIR} ${SHADER_RENDERER} ${SHADER_PROFILE})
# endforeach()

# foreach(SHADER ${FRAGMENT_SHADERS})
#     compile_shader("fragment" ${SHADER} ${SHADER_OUTPUT_DIR} ${SHADER_RENDERER} ${SHADER_PROFILE})
# endforeach()

# add_custom_target(shaders ALL DEPENDS ${COMPILED_SHADERS})
# add_dependencies(Studio shaders)

# target_include_directories(Studio PRIVATE ${SHADER_OUTPUT_DIR})

# --- Свойства цели и установка ---
if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.rcubiq.studio)
endif()

set_target_properties(Studio PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Studio
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# # --- Копирование зависимостей ---
# if(CMAKE_BUILD_TYPE STREQUAL "Debug")
#     set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/Debug")
# else()
#     set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/Release")
# endif()

# file(MAKE_DIRECTORY ${OUTPUT_DIR})

# function(copy_opencv_dependencies target_name output_path)
#     get_target_property(LINK_LIBS ${target_name} LINK_LIBRARIES)
#     if(NOT LINK_LIBS)
#         return()
#     endif()

#     list(APPEND libs_to_check ${LINK_LIBS})
#     set(processed_libs "")

#     while(libs_to_check)
#         list(POP_FRONT libs_to_check lib)

#         if(lib IN_LIST processed_libs OR lib MATCHES "^\\$" OR lib MATCHES "debug|optimized|general")
#             continue()
#         endif()
#         list(APPEND processed_libs lib)

#         if(TARGET ${lib})
#             get_target_property(lib_location ${lib} LOCATION)

#             if(lib_location AND lib_location MATCHES "\\.dll$")
#                 file(COPY ${lib_location} DESTINATION ${output_path})
#                 message(STATUS "Copied DLL: ${lib_location}")
#             endif()

#             get_target_property(lib_deps ${lib} INTERFACE_LINK_LIBRARIES)
#             if(lib_deps)
#                 list(APPEND libs_to_check ${lib_deps})
#             endif()

#             get_target_property(lib_direct_deps ${lib} LINK_LIBRARIES)
#             if(lib_direct_deps)
#                 list(APPEND libs_to_check ${lib_direct_deps})
#             endif()
#         endif()
#     endwhile()
# endfunction()

# copy_opencv_dependencies(Studio ${OUTPUT_DIR})

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Studio)
endif()

foreach(_qm IN LISTS QM_FILES)
    add_custom_command(
        TARGET Studio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Studio>/i18n"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_qm}" "$<TARGET_FILE_DIR:Studio>/i18n/"
        VERBATIM
    )
endforeach()

# Optional: copy akvirtualcamera C API next to the executable (same folder the app loads vcam from).
set(STUDIO_AKVCAM_CAPI_DLL "${CMAKE_SOURCE_DIR}/../akvirtualcamera/build-mingw64/build/x64/libvcam_capi.dll")
if(WIN32 AND EXISTS "${STUDIO_AKVCAM_CAPI_DLL}")
    add_custom_command(
        TARGET Studio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${STUDIO_AKVCAM_CAPI_DLL}" "$<TARGET_FILE_DIR:Studio>"
        COMMENT "Copying libvcam_capi.dll next to Studio executable"
    )
endif()
