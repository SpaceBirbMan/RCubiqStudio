cmake_minimum_required(VERSION 3.16)

project(M3 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_VERSION_MAJOR 6)

# Qt6 является основным, но если найти ему замену, можно будет выбрать и Qt5 + будет проще заменить графический фреймворк

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Quick QuickWidgets LinguistTools Gui QuickControls2)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Quick QuickWidgets LinguistTools Gui QuickControls2)

set(TS_FILES M3_ru_RU.ts)

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


        renderbridgeqt.h renderbridgeqt.cpp
        qrenderitem.h qrenderitem.cpp
        framequeue.h framequeue.cpp
        frameview.h frameview.cpp

        qttextureimpl.h qttextureimpl.cpp
        commandlistimpl.h commandlistimpl.cpp
        qtcontextimpl.h qtcontextimpl.cpp
        mainwindow2.ui
        main.qml
        ViewportWidget.h



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

target_link_libraries(
    M3 PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickWidgets
    # Qt 6 из-за зависимости от RHI модуля
    Qt6::Gui
    Qt6::QuickControls2
    Qt6::GuiPrivate
)

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

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(M3)
endif()
