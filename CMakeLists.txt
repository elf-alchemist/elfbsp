# ELFBSP CMake Script
cmake_minimum_required(VERSION 3.15)
project(elfbsp
        VERSION "1.1"
        LANGUAGES C CXX)

include(CheckIncludeFile)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_INSTALL_DO_STRIP TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")

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

option(ENABLE_WERROR "Treat warnings as errors" OFF)
option(ENABLE_ASAN "Enable ASan" OFF)
option(ENABLE_UBSAN "Enable UBSan" OFF)
option(ENABLE_TSAN "Enable TSan" OFF)
option(ENABLE_HARDENING "Enable hardening flags" OFF)
option(ENABLE_LTO "Enable link time Optimization" OFF)

set(SAN_COUNT 0)
foreach(san ENABLE_ASAN ENABLE_UBSAN ENABLE_TSAN)
    if(${san})
        math(EXPR SAN_COUNT "${SAN_COUNT} + 1")
    endif()
endforeach()

if(ENABLE_LTO AND SAN_COUNT GREATER 0)
    message(FATAL_ERROR "LTO cannot be enabled together with sanitizers.")
endif()

if(SAN_COUNT GREATER 0)
    if(NOT CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
        message(FATAL_ERROR
                "Sanitizers require Debug or RelWithDebInfo build type "
                "(current: ${CMAKE_BUILD_TYPE}).")
    endif()
endif()

add_executable(elfbsp
    src/bsp.cpp
    src/level.cpp
    src/main.cpp
    src/misc.cpp
    src/node.cpp
    src/parse.cpp
    src/polyobj.cpp
    src/wad.cpp
)

target_compile_options(elfbsp PRIVATE
    -Wall
    -Wextra
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wno-unused-parameter
)

target_compile_options(elfbsp PRIVATE
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:RelWithDebInfo>:-O3>
    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:MinSizeRel>:-Os>
)

target_compile_options(elfbsp PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
)

if(ENABLE_WERROR)
    target_compile_options(elfbsp PRIVATE -Werror)
endif()

if(ENABLE_ASAN)
    target_compile_options(elfbsp PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(elfbsp PRIVATE -fsanitize=address)
endif()

if(ENABLE_UBSAN)
    target_compile_options(elfbsp PRIVATE -fsanitize=undefined)
    target_link_options(elfbsp PRIVATE -fsanitize=undefined)
endif()

if(ENABLE_TSAN)
    target_compile_options(elfbsp PRIVATE -fsanitize=thread)
    target_link_options(elfbsp PRIVATE -fsanitize=thread)
endif()

if(ENABLE_HARDENING)
    target_compile_options(elfbsp PRIVATE -fstack-protector-strong)
    target_link_options(elfbsp PRIVATE -Wl,-z,relro -Wl,-z,now)
endif()

if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set_property(TARGET elfbsp PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "LTO not supported: ${ipo_error}")
    endif()
endif()

target_include_directories(elfbsp PRIVATE ${CMAKE_BINARY_DIR})

set(CPACK_GENERATOR ZIP)
if(WIN32)
    install(FILES LICENSE.txt DESTINATION .)
    install(FILES elfbsp.cfg DESTINATION .)
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
    include(GNUInstallDirs)
    install(FILES LICENSE.txt DESTINATION "share/doc/${PROJECT_NAME}")
    install(FILES elfbsp.cfg DESTINATION "share/doc/${PROJECT_NAME}")
    install(TARGETS elfbsp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

set(CPACK_PACKAGE_NAME "elfbsp")
set(CPACK_PACKAGE_VENDOR "elfbsp")

set(CPACK_PACKAGE_VERSION
    "${CPACK_PACKAGE_VERSION}"
    CACHE STRING "CPack version override")

if(NOT CPACK_PACKAGE_VERSION)
  set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
endif()

if(WIN32)
  set(CPACK_SYSTEM_NAME "windows_x86_64")
elseif(APPLE)
  set(CPACK_SYSTEM_NAME "macos_universal")
elseif(UNIX)
  set(CPACK_SYSTEM_NAME "linux_x86_64")
endif()

set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_${CPACK_SYSTEM_NAME}")

include(CPack)
