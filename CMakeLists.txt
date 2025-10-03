# ELFBSP CMake Script
cmake_minimum_required(VERSION 3.15)
project(elfbsp
        VERSION "1.7"
        LANGUAGES C CXX)

include(CheckIncludeFile)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Set a default build type if none was specified
set(default_build_type "RelWithDebInfo")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS
        "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE
        "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(PROJECT_COMPANY "Guilherme Miranda, et al")
set(PROJECT_COPYRIGHT "Copyright (C) 1994-2025")
set(PROJECT_LICENSE "GNU General Public License, version 2")
set(PROJECT_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")

configure_file(config.h.in config.h)

add_compile_options(
    -Wall
    -Wextra
    -Wshadow
    -Wno-unused-parameter
    -O3
    -fno-exceptions
    -fno-rtti
    -fno-strict-aliasing
    -fwrapv
)

add_executable(elfbsp
    src/elfbsp.cpp
    src/level.cpp
    src/node.cpp
    src/misc.cpp
    src/parse.cpp
    src/utility.cpp
    src/wad.cpp
)

target_include_directories(elfbsp PRIVATE ${CMAKE_BINARY_DIR})

if(WIN32)
    set(CPACK_GENERATOR ZIP)
    install(FILES LICENSE.txt DESTINATION .)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.21 AND NOT VCPKG_TOOLCHAIN)
        install(
            TARGETS elfbsp
            RUNTIME_DEPENDENCIES 
                PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
                POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
                DIRECTORIES $<TARGET_FILE_DIR:elfbsp> $ENV{PATH}
            RUNTIME DESTINATION .
        )
    else()
        install(TARGETS elfbsp RUNTIME DESTINATION .)
    endif()
else()
    install(FILES LICENSE.txt DESTINATION "share/doc/${PROJECT_NAME}")
    install(TARGETS elfbsp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
include(CPack)
