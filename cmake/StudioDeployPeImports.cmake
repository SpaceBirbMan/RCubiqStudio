# Рекурсивное копирование DLL рядом с exe: обход импортов PE (objdump -p).
# Поиск файлов — только в каталогах из STUDIO_PATHS_FILE + каталог exe (уже скопированное).
# Системные DLL Windows не копируются (см. список и префиксы api-ms-win- / ext-ms-win-).
cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED STUDIO_OBJDUMP)
    message(FATAL_ERROR "StudioDeployPeImports: задайте STUDIO_OBJDUMP")
endif()
if(NOT DEFINED STUDIO_DEST OR NOT DEFINED STUDIO_PATHS_FILE)
    message(FATAL_ERROR "StudioDeployPeImports: задайте STUDIO_DEST и STUDIO_PATHS_FILE")
endif()
if(NOT DEFINED STUDIO_SEED_EXE)
    message(FATAL_ERROR "StudioDeployPeImports: задайте STUDIO_SEED_EXE")
endif()

string(STRIP "${STUDIO_DEST}" STUDIO_DEST)
string(STRIP "${STUDIO_PATHS_FILE}" STUDIO_PATHS_FILE)
string(STRIP "${STUDIO_SEED_EXE}" STUDIO_SEED_EXE)

if(NOT EXISTS "${STUDIO_PATHS_FILE}")
    message(FATAL_ERROR "StudioDeployPeImports: не найден STUDIO_PATHS_FILE: ${STUDIO_PATHS_FILE}")
endif()
if(NOT EXISTS "${STUDIO_SEED_EXE}")
    message(FATAL_ERROR "StudioDeployPeImports: не найден exe: ${STUDIO_SEED_EXE}")
endif()

file(STRINGS "${STUDIO_PATHS_FILE}" _plines)
set(_search "")
foreach(line IN LISTS _plines)
    string(STRIP "${line}" line)
    if(NOT line STREQUAL "" AND NOT line MATCHES "^#")
        list(APPEND _search "${line}")
    endif()
endforeach()
list(APPEND _search "${STUDIO_DEST}")
list(REMOVE_DUPLICATES _search)

# Базовые системные имена (нижний регистр); api-ms-win / ext-ms-win отдельным правилом
set(_system_names
    kernel32 kernelbase ntdll user32 gdi32 gdiplus advapi32 shell32 shlwapi
    ole32 oleaut32 oleacc comdlg32 comctl32 combase ws2_32 winmm winspool
    imm32 odbc32 odbccp32 rpcrt4 dbghelp version dbgcore normaliz bcrypt secur32
    ncrypt nsi profapi crypt32 wldap32
    d3d9 d3d10 d3d11 d3d12 dxgi dxguid dwmapi hid setupapi cfgmgr32 msctf uxtheme
    dwmapi gdi32full win32u windows.storage
    msvcrt ucrtbase
    vcruntime140 vcruntime140_1 vcruntime140d msvcp140 msvcp140d concrt140
)

function(_studio_is_system_dll base_lower out_var)
    if(base_lower MATCHES "^api-ms-win-" OR base_lower MATCHES "^ext-ms-win-")
        set(${out_var} 1 PARENT_SCOPE)
        return()
    endif()
    foreach(s IN LISTS _system_names)
        if(base_lower STREQUAL "${s}")
            set(${out_var} 1 PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out_var} 0 PARENT_SCOPE)
endfunction()

function(_studio_find_dll_in_search_dirs name out_path)
    set(found "")
    foreach(dir IN LISTS _search)
        if(NOT IS_DIRECTORY "${dir}")
            continue()
        endif()
        if(EXISTS "${dir}/${name}")
            set(found "${dir}/${name}")
            break()
        endif()
    endforeach()
    set(${out_path} "${found}" PARENT_SCOPE)
endfunction()

function(_studio_imported_dlls_from_pe pe_file out_list)
    execute_process(
        COMMAND "${STUDIO_OBJDUMP}" -p "${pe_file}"
        OUTPUT_VARIABLE _out
        RESULT_VARIABLE _rc
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _rc EQUAL 0 OR _out STREQUAL "")
        set(${out_list} "" PARENT_SCOPE)
        return()
    endif()
    string(REGEX REPLACE "\r\n" "\n" _out "${_out}")
    string(REGEX REPLACE "\r" "\n" _out "${_out}")
    string(REPLACE "\n" ";" _lines "${_out}")
    set(_names "")
    foreach(_line IN LISTS _lines)
        string(STRIP "${_line}" _line)
        if(NOT _line MATCHES "^DLL Name:[ \t]*(.+)$")
            continue()
        endif()
        set(_raw "${CMAKE_MATCH_1}")
        string(STRIP "${_raw}" _raw)
        if(NOT _raw)
            continue()
        endif()
        get_filename_component(_base "${_raw}" NAME)
        string(TOLOWER "${_base}" _low)
        _studio_is_system_dll("${_low}" _sys)
        if(_sys)
            continue()
        endif()
        list(APPEND _names "${_base}")
    endforeach()
    list(REMOVE_DUPLICATES _names)
    set(${out_list} ${_names} PARENT_SCOPE)
endfunction()

set(_queue "")
set(_done_pe "")

macro(_studio_enqueue_pe path)
    if(EXISTS "${path}")
        list(APPEND _queue "${path}")
    endif()
endmacro()

_studio_enqueue_pe("${STUDIO_SEED_EXE}")

if(DEFINED STUDIO_SEED_DLL)
    string(STRIP "${STUDIO_SEED_DLL}" STUDIO_SEED_DLL)
endif()
if(STUDIO_SEED_DLL AND EXISTS "${STUDIO_SEED_DLL}")
    get_filename_component(_sdname "${STUDIO_SEED_DLL}" NAME)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${STUDIO_SEED_DLL}" "${STUDIO_DEST}/${_sdname}")
    if(EXISTS "${STUDIO_DEST}/${_sdname}")
        _studio_enqueue_pe("${STUDIO_DEST}/${_sdname}")
    endif()
endif()

set(_max_pass 400)
set(_pass 0)
while(_queue AND _pass LESS _max_pass)
    math(EXPR _pass "${_pass}+1")
    list(POP_FRONT _queue _pe)
    if(NOT EXISTS "${_pe}")
        continue()
    endif()

    get_filename_component(_can "${_pe}" REALPATH)
    if(_can IN_LIST _done_pe)
        continue()
    endif()
    list(APPEND _done_pe "${_can}")

    _studio_imported_dlls_from_pe("${_pe}" _imports)
    foreach(dllname IN LISTS _imports)
        _studio_find_dll_in_search_dirs("${dllname}" _src)
        if(_src STREQUAL "")
            continue()
        endif()
        set(_dst "${STUDIO_DEST}/${dllname}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_src}" "${_dst}")
        if(EXISTS "${_dst}")
            _studio_enqueue_pe("${_dst}")
        endif()
    endforeach()
endwhile()

if(_pass GREATER_EQUAL _max_pass)
    message(WARNING "StudioDeployPeImports: достигнут лимит итераций (${_max_pass})")
endif()
