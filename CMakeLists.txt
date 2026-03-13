cmake_minimum_required(VERSION 3.16)

project(M3 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_VERSION_MAJOR 6)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Quick QuickWidgets LinguistTools Gui QuickControls2 Quick3D)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Quick QuickWidgets LinguistTools Gui QuickControls2 Quick3D)
find_package(PkgConfig REQUIRED)

set(TS_FILES M3_ru_RU.ts)
set(QT_CREATOR_SKIP_MAINTENANCE_TOOL_PROVIDER ON)

set(CMAKE_AUTOUIC_SEARCH_PATHS
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/ui
    ${CMAKE_SOURCE_DIR}/shaders
)

# --- Сбор исходных файлов ---
# Рекурсивно ищем все исходники в текущей директории и подпапках
file(GLOB_RECURSE PROJECT_SOURCES
    "*.cpp"
    "*.h"
    "*.ui"
    "*.qml"
    "*.qrc"
    "*.qmodel"
    "*.sc"
)

# нужны ли функции для частичной сборки (только ui, только весь интерфейс, т.д.)

# Исключаем файлы из папок сборки, если они вдруг попали в дерево проекта
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "build/.*")


# --- Настройка исполняемого файла ---
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(M3
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(M3 SHARED ${PROJECT_SOURCES})
    else()
        add_executable(M3 ${PROJECT_SOURCES})
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# --- Библиотеки и зависимости ---
add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE ${CMAKE_SOURCE_DIR}/../miniaudio-0.11.24)

find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio highgui)
pkg_check_modules(HIDAPI REQUIRED IMPORTED_TARGET hidapi)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
pkg_check_modules(SERIALPORT REQUIRED libserialport)

target_include_directories(M3 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/shaders")

set(BGFX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../bgfx/install-mingw64-d")
set(BIN_DIRS "${BGFX_ROOT}/bin")

target_include_directories(M3 PRIVATE ${BGFX_ROOT}/include)
target_link_directories(M3 PRIVATE ${BGFX_ROOT}/lib)
set(UIOHOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libuiohook")
add_library(uiohook SHARED IMPORTED)
set_target_properties(uiohook PROPERTIES IMPORTED_IMPLIB
    "${UIOHOOK_DIR}/Debug/libuiohook.dll.a" IMPORTED_LOCATION
    "${UIOHOOK_DIR}/Debug/libuiohook.dll"
    INTERFACE_INCLUDE_DIRECTORIES
    "${UIOHOOK_DIR}/libuiohook/include" )
target_link_libraries(M3 PRIVATE uiohook)

set(SPOUT_DIR "${CMAKE_SOURCE_DIR}/../spout2")
set(SPOUT_LIB_DIR "${SPOUT_DIR}/INSTALL/lib")
set(SPOUT_BIN_DIR "${SPOUT_DIR}/INSTALL/bin")

target_include_directories(M3 PRIVATE
    "${SPOUT_DIR}/INSTALL/include"
)

set(SPOUT_LIBS
    "${SPOUT_LIB_DIR}/libSpoutDX.dll.a"
    "${SPOUT_LIB_DIR}/libSpoutDX12.dll.a"
    "${SPOUT_LIB_DIR}/libSpoutDX9.dll.a"
    "${SPOUT_LIB_DIR}/libSpout.dll.a"
    "${SPOUT_LIB_DIR}/libSpoutLibrary.dll.a"
)
target_compile_definitions(M3 PRIVATE
    USE_SPOUT_DX
    USE_SPOUT_DX12
    USE_SPOUT_DX9
    USE_SPOUT_GL
    USE_SPOUT_LIBRARY
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${SPOUT_LIBS})

add_compile_definitions(BX_CONFIG_DEBUG=1)

message(STATUS "OpenCV libs: ${OpenCV_LIBS}")

target_link_libraries(
    M3 PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickWidgets
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::QuickControls2
    bgfx
    bx
    bimg
    miniaudio
    serialport
    PkgConfig::HIDAPI
    PkgConfig::LIBUSB
    ${OpenCV_LIBS}
)

target_link_libraries(M3 PRIVATE ws2_32)

if(WIN32)
    target_link_libraries(M3 PRIVATE winmm setupapi)
endif()

file(GLOB_RECURSE VERTEX_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.vs.sc")
file(GLOB_RECURSE FRAGMENT_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.fs.sc")

message(STATUS "Vertex shaders found: ${VERTEX_SHADERS}")
message(STATUS "Fragment shaders found: ${FRAGMENT_SHADERS}")

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

set(COMPILED_SHADERS "")

if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(SHADER_RENDERER "glsl")
    set(SHADER_PROFILE "440")
elseif(WIN32)
    set(SHADER_RENDERER "dx12")
    set(SHADER_PROFILE "s_6_9")
elseif(APPLE)
    set(SHADER_RENDERER "metal")
    set(SHADER_PROFILE "metal31-14")
else()
    set(SHADER_RENDERER "glsl")
    set(SHADER_PROFILE "440")
endif()

function(compile_shader TYPE INPUT OUTPUT_DIR RENDERER PROFILE)
    get_filename_component(FILE_NAME ${INPUT} NAME_WE)
    set(OUTPUT_BIN "${OUTPUT_DIR}/${FILE_NAME}_${TYPE}.bin")

    add_custom_command(
        OUTPUT ${OUTPUT_BIN}
        COMMAND "${BIN_DIRS}/shaderc"
            -f ${INPUT}
            -o ${OUTPUT_BIN}
            --type ${TYPE}
            --platform ${RENDERER}
            --profile ${PROFILE}
        DEPENDS ${INPUT}
        COMMENT "Compiling ${TYPE} shader: ${INPUT} -> ${OUTPUT_BIN}"
        VERBATIM
    )

    set(COMPILED_SHADERS ${COMPILED_SHADERS} ${OUTPUT_BIN} PARENT_SCOPE)
endfunction()

foreach(SHADER ${VERTEX_SHADERS})
    compile_shader("vertex" ${SHADER} ${SHADER_OUTPUT_DIR} ${SHADER_RENDERER} ${SHADER_PROFILE})
endforeach()

foreach(SHADER ${FRAGMENT_SHADERS})
    compile_shader("fragment" ${SHADER} ${SHADER_OUTPUT_DIR} ${SHADER_RENDERER} ${SHADER_PROFILE})
endforeach()

add_custom_target(shaders ALL DEPENDS ${COMPILED_SHADERS})
add_dependencies(M3 shaders)

target_include_directories(M3 PRIVATE ${SHADER_OUTPUT_DIR})

# --- Свойства цели и установка ---
if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.M3)
endif()

set_target_properties(M3 PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS M3
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

# copy_opencv_dependencies(M3 ${OUTPUT_DIR})

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(M3)
endif()
