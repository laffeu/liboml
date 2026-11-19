cmake_minimum_required(VERSION 3.20)
project(liboml LANGUAGES C)
include(GNUInstallDirs)

option(BUILD_SHARED_LIBS "Build liboml as a shared library" ON)
add_library(oml src/liboml.c)

target_compile_features(oml PRIVATE c_std_17)
target_include_directories(oml PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(oml PRIVATE OML_QUIET)

if(WIN32)
    if(BUILD_SHARED_LIBS)
        set_target_properties(oml PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()
    target_link_libraries(oml PRIVATE kernel32)
    if(MSVC)
        target_compile_options(oml PRIVATE /W4)
        target_compile_definitions(oml PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(BUILD_SHARED_LIBS)
        set(uring_name liburing.so)
        set(uring_var URING_SHARED)
    else()
        set(uring_name liburing.a)
        set(uring_var URING_STATIC)
    endif()

    find_path(URING_INCLUDE liburing.h)
    find_library(${uring_var} NAMES "${uring_name}")
    if(NOT URING_INCLUDE OR NOT ${uring_var})
        include(FetchContent)
        find_program(GNU_MAKE NAMES gmake make REQUIRED)

        FetchContent_Declare(liburing
                GIT_REPOSITORY https://github.com/axboe/liburing.git
                GIT_TAG liburing-2.15
                GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(liburing)
        execute_process(
                COMMAND ./configure "--cc=${CMAKE_C_COMPILER}" "--prefix=${CMAKE_INSTALL_PREFIX}"
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
        find_library(${uring_var} NAMES "${uring_name}" REQUIRED)
    endif()

    target_include_directories(oml PRIVATE "${URING_INCLUDE}")
    target_link_libraries(oml PRIVATE "${${uring_var}}")
    target_compile_options(oml PRIVATE -Wall -Wextra)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_options(oml PRIVATE -Wall -Wextra -fblocks)
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

install(TARGETS oml)
install(FILES src/liboml.h src/loml.hpp DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
