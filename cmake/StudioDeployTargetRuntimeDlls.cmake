# Копирует $<TARGET_RUNTIME_DLLS> рядом с exe. Пустой список — не ошибка.
cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED DEST)
    message(FATAL_ERROR "StudioDeployTargetRuntimeDlls: задайте DEST")
endif()
string(STRIP "${DEST}" DEST)

if(NOT DLLS)
    return()
endif()

foreach(dll IN LISTS DLLS)
    if(dll AND EXISTS "${dll}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${dll}" "${DEST}")
    endif()
endforeach()
