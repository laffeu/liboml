cmake_minimum_required(VERSION 3.20)
project(liboml LANGUAGES C)
include(GNUInstallDirs)

add_library(oml SHARED src/liboml.c)

target_include_directories(oml PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(oml PRIVATE OML_QUIET)

if(WIN32)
    set_target_properties(oml PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    target_link_libraries(oml PRIVATE kernel32)
    if(MSVC)
        target_compile_options(oml PRIVATE /W4)
        target_compile_definitions(oml PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_path(URING_INCLUDE liburing.h)
    find_library(URING_SHARED NAMES liburing.so)
    if(NOT URING_INCLUDE OR NOT URING_SHARED)
        include(FetchContent)
        find_program(GNU_MAKE NAMES gmake make REQUIRED)

        FetchContent_Declare(liburing
                GIT_REPOSITORY https://github.com/axboe/liburing.git
                GIT_TAG liburing-2.15
                GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(liburing)
        execute_process(
                COMMAND ./configure
                "--cc=${CMAKE_C_COMPILER}"
                "--prefix=${CMAKE_INSTALL_PREFIX}"
                WORKING_DIRECTORY "${liburing_SOURCE_DIR}"
                COMMAND_ERROR_IS_FATAL ANY
        )
        foreach(target IN ITEMS all liburing.pc install)
            execute_process(
                    COMMAND "${GNU_MAKE}" "${target}"
                    WORKING_DIRECTORY "${liburing_SOURCE_DIR}"
                    COMMAND_ERROR_IS_FATAL ANY
            )
        endforeach()
        find_path(URING_INCLUDE liburing.h REQUIRED)
        find_library(URING_SHARED NAMES liburing.so REQUIRED)
    endif()

    target_include_directories(oml PRIVATE "${URING_INCLUDE}")
    target_link_libraries(oml PRIVATE "${URING_SHARED}")
    target_compile_options(oml PRIVATE -Wall -Wextra)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_options(oml PRIVATE -Wall -Wextra -fblocks)
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

install(TARGETS oml)
install(FILES src/liboml.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
