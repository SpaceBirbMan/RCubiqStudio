cmake_minimum_required(VERSION 3.16)

project(M3 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_VERSION_MAJOR 6)

# Qt6 является основным, но если найти ему замену, можно будет выбрать и Qt5 + будет проще заменить графический фреймворк

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Quick QuickWidgets LinguistTools Gui QuickControls2 Quick3D)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Quick QuickWidgets LinguistTools Gui QuickControls2 Quick3D)
find_package(PkgConfig REQUIRED)

set(TS_FILES M3_ru_RU.ts)
set(QT_CREATOR_SKIP_MAINTENANCE_TOOL_PROVIDER ON)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(M3
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}

        eventmanager.h eventmanager.cpp
        eventqueue.h
        appmessage.h
        appcore.h appcore.cpp
        datamanager.h datamanager.cpp
        model.qmodel
        modelmanager.h modelmanager.cpp
        cachemanager.h cachemanager.cpp
        crashreportmanager.h crashreportmanager.cpp
        fileloader.h fileloader.cpp
        filesaver.h filesaver.cpp
        libs.h
        messageprocessor.h messageprocessor.cpp
        fileinstance.h fileinstance.cpp
        filetypes.h
        enginemanager.h enginemanager.cpp

        shorts.h
        DevGuide.md

        dynamiclibrary.h dynamiclibrary.cpp
        devicemanager.h devicemanager.cpp
        deviceregistrator.h deviceregistrator.cpp
        inputdevice.h inputdevice.cpp
        testEng/core.h testEng/core.cpp
        testEng/puppetmodel.h testEng/puppetmodel.cpp
        testEng/inputhandler.h testEng/inputhandler.cpp
        testEng/rendercontroller.h testEng/rendercontroller.cpp
        testEng/modelfactory.h testEng/modelfactory.cpp
        misc.h
        icacheable.h
        resources.qrc
        consts.h

        abstractuinodes.h
        uirenderer.h uirenderer.cpp

        outputdevice.cpp
        outputdevice.h

        framequeue.h framequeue.cpp
        frameview.h frameview.cpp


        commandlistimpl.h commandlistimpl.cpp

        mainwindow2.ui
        main.qml
        viewportwidget.h
        rendermanager.h rendermanager.cpp
        renderapi.h
        viewportwidget.cpp
        cube.vs.sc
        cube.fs.sc
        varying.def.sc
        trackermanager.h trackermanager.cpp
        devices.h devices.cpp

    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET M3 APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(M3 SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(M3
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# if(MINGW)
#     set(CMAKE_EXE_LINKER_FLAGS "-static")
# endif()

add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE ${CMAKE_SOURCE_DIR}/../miniaudio-0.11.24)

find_package(PkgConfig REQUIRED)

find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio highgui)
pkg_check_modules(HIDAPI REQUIRED IMPORTED_TARGET hidapi)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
pkg_check_modules(SERIALPORT REQUIRED libserialport)

target_include_directories(M3 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(BGFX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../bgfx/install-mingw64-d") # Буква d означает debug-сборку библиотеки
set(BIN_DIRS "${BGFX_ROOT}/bin")
target_include_directories(M3 PRIVATE ${BGFX_ROOT}/include)
target_link_directories(M3 PRIVATE ${BGFX_ROOT}/lib)
add_compile_definitions(BX_CONFIG_DEBUG=1)
message(${OpenCV_LIBS})
target_link_libraries(
    M3 PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickWidgets
    Qt6::Gui
    Qt6::QuickControls2
    Qt6::GuiPrivate
    bgfx
    bx
    bimg
    miniaudio
    serialport
    PkgConfig::HIDAPI
    PkgConfig::LIBUSB
    ${OpenCV_LIBS}
)

if(WIN32)
    target_link_libraries(M3 PRIVATE winmm setupapi)
endif()

# set(BX_DIR       "${CMAKE_CURRENT_SOURCE_DIR}/../bgfx/bx")
# set(BIMG_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/../bgfx/bimg")
# set(BGFX_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/../bgfx/bgfx")
# add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../bgfx/bgfx.cmake" bgfx-build)

file(GLOB VERTEX_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.vs.sc")
file(GLOB FRAGMENT_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.fs.sc")

message(STATUS "Vertex shaders: ${VERTEX_SHADERS}")
message(STATUS "Fragment shaders: ${FRAGMENT_SHADERS}")

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

function(copy_opencv_dependencies target_name output_path)
    get_target_property(LINK_LIBS ${target_name} LINK_LIBRARIES)
    if(NOT LINK_LIBS)
        return()
    endif()

    # Используем рекурсивный обход всех связанных библиотек
    list(APPEND libs_to_check ${LINK_LIBS})
    set(processed_libs "")

    while(libs_to_check)
        list(POP_FRONT libs_to_check lib)

        # Пропускаем уже обработанные и служебные записи
        if(lib IN_LIST processed_libs OR lib MATCHES "^\\$" OR lib MATCHES "debug|optimized|general")
            continue()
        endif()
        list(APPEND processed_libs lib)

        if(TARGET ${lib})
            # Получаем расположение библиотеки
            get_target_property(lib_location ${lib} LOCATION)

            # Если это DLL, копируем её
            if(lib_location AND lib_location MATCHES "\\.dll$")
                file(COPY ${lib_location} DESTINATION ${output_path})
                message(STATUS "Copied DLL: ${lib_location}")
            endif()

            # Добавляем зависимости этой библиотеки в очередь проверки
            get_target_property(lib_deps ${lib} INTERFACE_LINK_LIBRARIES)
            if(lib_deps)
                list(APPEND libs_to_check ${lib_deps})
            endif()

            # Для импортированных целей иногда зависимости лежат в LINK_LIBRARIES
            get_target_property(lib_direct_deps ${lib} LINK_LIBRARIES)
            if(lib_direct_deps)
                list(APPEND libs_to_check ${lib_direct_deps})
            endif()
        endif()
    endwhile()
endfunction()

message(STATUS "VERTEX_SHADERS = ${VERTEX_SHADERS}")
message(STATUS "FRAGMENT_SHADERS = ${FRAGMENT_SHADERS}")
message(STATUS "SHADER_OUTPUT_DIR = ${SHADER_OUTPUT_DIR}")
message(STATUS "SHADER_RENDERER = ${SHADER_RENDERER}")

foreach(SHADER ${VERTEX_SHADERS})
    compile_shader("vertex" ${SHADER} ${SHADER_OUTPUT_DIR} ${SHADER_RENDERER} ${SHADER_PROFILE})
endforeach()
foreach(SHADER ${FRAGMENT_SHADERS})
    compile_shader("fragment" ${SHADER} ${SHADER_OUTPUT_DIR} ${SHADER_RENDERER} ${SHADER_PROFILE})
endforeach()

add_custom_target(shaders ALL DEPENDS ${COMPILED_SHADERS})
add_dependencies(M3 shaders)

target_include_directories(M3 PRIVATE ${SHADER_OUTPUT_DIR})

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
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


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/Debug")
else()
    set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/Release")
endif()

file(MAKE_DIRECTORY ${OUTPUT_DIR})

copy_opencv_dependencies(M3 ${OUTPUT_DIR})

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(M3)
endif()
