# Копирование runtime DLL рядом с exe (MinGW + Qt + каталоги из CMake / STUDIO_EXTRA_RUNTIME_SEARCH_DIRS).
# Подключать после find_package(OpenCV), pkg_check_modules, задания BGFX_ROOT и uiohook.
cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED STUDIO_DEPLOY_RUNTIME_POSTBUILD)
    option(STUDIO_DEPLOY_RUNTIME_POSTBUILD
        "После сборки: windeployqt + копирование MinGW/OpenCV/hidapi/usb/bgfx из сгенерированного списка каталогов"
        ON)
endif()

if(NOT DEFINED STUDIO_EXTRA_RUNTIME_SEARCH_DIRS)
    set(STUDIO_EXTRA_RUNTIME_SEARCH_DIRS "" CACHE STRING
        "Дополнительные каталоги с .dll (точка с запятой), например C:/opencv/build/x64/mingw/bin")
endif()

function(_studio_imported_dll_parent_dir out_var tgt)
    set(_dir "")
    if(NOT TARGET "${tgt}")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()
    foreach(suffix "" "_DEBUG" "_RELEASE" "_MINSIZEREL" "_RELWITHDEBINFO" "_NOCONFIG")
        if(suffix STREQUAL "")
            set(prop "IMPORTED_LOCATION")
        else()
            set(prop "IMPORTED_LOCATION${suffix}")
        endif()
        get_target_property(_loc ${tgt} ${prop})
        if(_loc AND NOT _loc MATCHES "-NOTFOUND$" AND EXISTS "${_loc}")
            if(_loc MATCHES "\\.[Dd][Ll][Ll]$")
                get_filename_component(_dir "${_loc}" DIRECTORY)
                break()
            endif()
        endif()
    endforeach()
    set(${out_var} "${_dir}" PARENT_SCOPE)
endfunction()

function(studio_write_runtime_search_paths out_txt)
    set(cands "")

    if(CMAKE_CXX_COMPILER)
        get_filename_component(_mb "${CMAKE_CXX_COMPILER}" DIRECTORY)
        list(APPEND cands "${_mb}")
    endif()

    if(DEFINED OpenCV_DIR)
        get_filename_component(_ocv_cfg "${OpenCV_DIR}" ABSOLUTE)
        get_filename_component(_ocv_root "${_ocv_cfg}" DIRECTORY)
        list(APPEND cands "${_ocv_root}/bin")
        list(APPEND cands "${_ocv_root}/x64/mingw/bin")
        list(APPEND cands "${_ocv_cfg}/bin")
        list(APPEND cands "${OpenCV_DIR}/../bin")
        list(APPEND cands "${OpenCV_DIR}/bin")
    endif()

    if(DEFINED BGFX_ROOT)
        list(APPEND cands "${BGFX_ROOT}/bin")
    endif()

    if(DEFINED UIOHOOK_DIR)
        list(APPEND cands "${UIOHOOK_DIR}")
        list(APPEND cands "${UIOHOOK_DIR}/Debug")
        list(APPEND cands "${UIOHOOK_DIR}/Release")
    endif()

    foreach(pref IN LISTS CMAKE_PREFIX_PATH)
        list(APPEND cands "${pref}/bin")
    endforeach()

    if(DEFINED HIDAPI_LIBRARY_DIRS)
        foreach(d IN LISTS HIDAPI_LIBRARY_DIRS)
            list(APPEND cands "${d}")
        endforeach()
    endif()
    if(DEFINED LIBUSB_LIBRARY_DIRS)
        foreach(d IN LISTS LIBUSB_LIBRARY_DIRS)
            list(APPEND cands "${d}")
        endforeach()
    endif()

    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        list(APPEND cands "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
    endif()
    if(DEFINED ENV{VCPKG_ROOT} AND DEFINED VCPKG_TARGET_TRIPLET)
        list(APPEND cands "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/bin")
    endif()

    _studio_imported_dll_parent_dir(_d uiohook)
    if(_d)
        list(APPEND cands "${_d}")
    endif()
    _studio_imported_dll_parent_dir(_d PkgConfig::HIDAPI)
    if(_d)
        list(APPEND cands "${_d}")
    endif()
    _studio_imported_dll_parent_dir(_d PkgConfig::LIBUSB)
    if(_d)
        list(APPEND cands "${_d}")
    endif()

    foreach(d IN LISTS STUDIO_EXTRA_RUNTIME_SEARCH_DIRS)
        list(APPEND cands "${d}")
    endforeach()

    set(dirs "")
    foreach(p IN LISTS cands)
        if(NOT p)
            continue()
        endif()
        if(IS_DIRECTORY "${p}")
            list(APPEND dirs "${p}")
        endif()
        # pkg-config отдаёт .../lib, сами DLL лежат в соседнем bin.
        if(p MATCHES "[/\\\\]lib$")
            get_filename_component(_parent "${p}" DIRECTORY)
            if(IS_DIRECTORY "${_parent}/bin")
                list(APPEND dirs "${_parent}/bin")
            endif()
        endif()
    endforeach()
    list(REMOVE_DUPLICATES dirs)

    file(WRITE "${out_txt}" "")
    foreach(d IN LISTS dirs)
        file(APPEND "${out_txt}" "${d}\n")
    endforeach()

    message(STATUS "Studio deploy search dirs (${out_txt}):")
    foreach(d IN LISTS dirs)
        message(STATUS "  ${d}")
    endforeach()
endfunction()

function(studio_add_windows_runtime_postbuild tgt paths_txt fall_script)
    if(NOT WIN32)
        return()
    endif()
    if(NOT STUDIO_DEPLOY_RUNTIME_POSTBUILD)
        message(STATUS "STUDIO_DEPLOY_RUNTIME_POSTBUILD=OFF: windeployqt / копирование DLL отключены")
        return()
    endif()

    find_program(STUDIO_WINDEPLOYQT
        NAMES windeployqt6 windeployqt
        HINTS
            "$ENV{QT_DIR}/bin"
            "${Qt6_DIR}/../../../bin"
            "${Qt5_DIR}/../../../bin"
        DOC "Qt windeployqt for bundling Qt plugins and DLLs"
    )
    if(STUDIO_WINDEPLOYQT)
        add_custom_command(TARGET ${tgt} POST_BUILD
            COMMAND "${STUDIO_WINDEPLOYQT}"
                "$<TARGET_FILE:${tgt}>"
                --no-translations
                --no-compiler-runtime
                $<IF:$<CONFIG:Debug>,--debug,--release>
                --quick
                --quickcontrols2
                --quick3d
            COMMENT "windeployqt: Qt DLLs/plugins next to $<TARGET_FILE_NAME:${tgt}>"
            VERBATIM
        )
        message(STATUS "windeployqt: ${STUDIO_WINDEPLOYQT}")
    else()
        message(WARNING "windeployqt not found — add Qt bin to PATH or CMAKE_PREFIX_PATH for a portable bundle")
    endif()

    add_custom_command(TARGET ${tgt} POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            "-DSTUDIO_DEST=$<TARGET_FILE_DIR:${tgt}>"
            "-DSTUDIO_PATHS_FILE=${paths_txt}"
            -P "${fall_script}"
        COMMENT "Copy MinGW/OpenCV/pkgconfig DLLs (see studio_deploy_search_dirs.txt)"
        VERBATIM
    )

    # То, что CMake знает как разделяемые зависимости цели (не .a и не системные .lib).
    get_filename_component(_deploy_cmake_dir_rt "${fall_script}" DIRECTORY)
    set(_rt_script "${_deploy_cmake_dir_rt}/StudioDeployTargetRuntimeDlls.cmake")
    if(EXISTS "${_rt_script}")
        add_custom_command(TARGET ${tgt} POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DDLLS=$<TARGET_RUNTIME_DLLS:${tgt}>"
                "-DDEST=$<TARGET_FILE_DIR:${tgt}>"
                -P "${_rt_script}"
            COMMENT "Copy linked runtime DLLs next to $<TARGET_FILE_NAME:${tgt}>"
            VERBATIM
        )
    endif()

    # Рекурсивно: objdump по импортам exe и уже скопированных DLL → докинуть gstreamer и др. из mingw64/bin
    get_filename_component(_deploy_cmake_dir "${fall_script}" DIRECTORY)
    set(_pe_script "${_deploy_cmake_dir}/StudioDeployPeImports.cmake")
    if(NOT EXISTS "${_pe_script}")
        message(WARNING "StudioDeployPeImports.cmake not found next to fallback: ${_pe_script}")
        return()
    endif()

    get_filename_component(_cxx_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_program(STUDIO_DEPLOY_OBJDUMP
        NAMES objdump
        HINTS "${_cxx_bin}" "${_cxx_bin}/.." "${_cxx_bin}/../bin"
        DOC "MinGW objdump (-p) for PE import bundle"
    )
    if(NOT STUDIO_DEPLOY_OBJDUMP)
        message(WARNING "objdump not found (HINT ${_cxx_bin}) — skipping recursive PE-import DLL deploy")
        return()
    endif()

    # Явная копия uiohook (плюс то же подтянет граф импортов exe); путь на Release/Debug — genex при генерации билда
    set(_pe_seed_dll_arg "")
    if(DEFINED UIOHOOK_DIR AND NOT UIOHOOK_DIR STREQUAL "")
        set(_pe_seed_dll_arg "-DSTUDIO_SEED_DLL=$<IF:$<CONFIG:Debug>,${UIOHOOK_DIR}/Debug/libuiohook.dll,${UIOHOOK_DIR}/Release/libuiohook.dll>")
    endif()

    add_custom_command(TARGET ${tgt} POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            "-DSTUDIO_OBJDUMP=${STUDIO_DEPLOY_OBJDUMP}"
            "-DSTUDIO_DEST=$<TARGET_FILE_DIR:${tgt}>"
            "-DSTUDIO_PATHS_FILE=${paths_txt}"
            "-DSTUDIO_SEED_EXE=$<TARGET_FILE:${tgt}>"
            ${_pe_seed_dll_arg}
            -P "${_pe_script}"
        COMMENT "Recursive deploy: objdump PE imports → search dirs from studio_deploy_search_dirs.txt"
        VERBATIM
    )
    message(STATUS "PE-import deploy: ${_pe_script} (objdump ${STUDIO_DEPLOY_OBJDUMP})")
endfunction()
