# ELFBSP CMake Script
cmake_minimum_required(VERSION 3.15)
project(elfbsp
        VERSION "1.3"
        LANGUAGES C CXX
        HOMEPAGE_URL "https://elf-alchemist.github.io/elfbsp"
        SPDX_LICENSE "GPL-2.0+"
        DESCRIPTION "Modern BSP tree builder for idTech1 games and source ports.")

include(CheckIncludeFile)

set(CMAKE_INSTALL_DO_STRIP TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")

# Set a default build type if none was specified
set(default_build_type "RelWithDebInfo")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
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

add_executable(${PROJECT_NAME}
  src/blockmap.cpp
  src/bsp.cpp
  src/info.cpp
  src/level.cpp
  src/main.cpp
  src/misc.cpp
  src/node.cpp
  src/parse.cpp
  src/polyobj.cpp
  src/reject.cpp
  src/wad.cpp
)

string(TOUPPER "${PROJECT_NAME}" PROJECT_TITLE)
string(TIMESTAMP PROJECT_DATE "%Y-%m-%d" UTC)

set(PROJECT_GENERIC_NAME "idTech1 BSP node builder")
set(PROJECT_CREATOR "elf-alchemist")
set(PROJECT_COPYRIGHT "Copyright \(C\) 1994-2026 - Guilherme Miranda, et al")
set(PROJECT_LICENSE "GNU General Public License, version 2")

set(PROJECT_URL "https://github.com/elf-alchemist/elfbsp")
set(PROJECT_WIKI "${PROJECT_URL}/wiki")
set(PROJECT_ISSUES "${PROJECT_URL}/issues")

set(PROJECT_DEVELOPER_IDENTIFIER "io.github.elf-alchemist")
set(PROJECT_IDENTIFIER "${PROJECT_IDENTIFIER}.elfbsp")

set(PROJECT_VERSION_RC "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
set(PROJECT_ICO "${PROJECT_IDENTIFIER}.windows.ico")
set(PROJECT_SUMMARY "${PROJECT_DESCRIPTION} Based on Andrew Apted's AJBSP, with even more features.")

set(MACOSX_BUNDLE_EXECUTABLE_NAME "${PROJECT_NAME}")
set(MACOSX_BUNDLE_INFO_STRING "${PROJECT_COPYRIGHT}")
set(MACOSX_BUNDLE_ICON_FILE "${PROJECT_IDENTIFIER}.macosx") # the *.icns extension is implied
set(MACOSX_BUNDLE_GUI_IDENTIFIER "${PROJECT_IDENTIFIER}")
set(MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}")
set(MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}")
set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
set(MACOSX_BUNDLE_COPYRIGHT "${PROJECT_COPYRIGHT}")

target_compile_definitions(${PROJECT_NAME} PRIVATE
  # These are required for SDL
  PROJECT_NAME="${PROJECT_TITLE}"
  PROJECT_VERSION="${PROJECT_VERSION}"
  PROJECT_IDENTIFIER="${PROJECT_IDENTIFIER}"
  PROJECT_CREATOR="${PROJECT_CREATOR}"
  PROJECT_COPYRIGHT="${PROJECT_COPYRIGHT}"
  PROJECT_URL="${PROJECT_URL}"
  PROJECT_TYPE="application"
  # These are extras
  PROJECT_STRING="${PROJECT_TITLE} ${PROJECT_VERSION}"
  PROJECT_DATE="${PROJECT_DATE}"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  C_STANDARD 23
  CXX_STANDARD 20
)

target_compile_options(${PROJECT_NAME} PRIVATE
  -Wall
  -Wextra
  -Wshadow
  -Wconversion
  -Wsign-conversion
  -Wno-unused-parameter
)

target_compile_options(${PROJECT_NAME} PRIVATE
  $<$<CONFIG:Release>:-O3>
  $<$<CONFIG:RelWithDebInfo>:-O3>
  $<$<CONFIG:Debug>:-O0>
  $<$<CONFIG:MinSizeRel>:-Os>
)

target_compile_options(${PROJECT_NAME} PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
)

if(ENABLE_WERROR)
  target_compile_options(${PROJECT_NAME} PRIVATE -Werror)
endif()

if(ENABLE_ASAN)
  target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
  target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
endif()

if(ENABLE_UBSAN)
  target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=undefined)
  target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=undefined)
endif()

if(ENABLE_TSAN)
  target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=thread)
  target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=thread)
endif()

if(ENABLE_HARDENING)
  target_compile_options(${PROJECT_NAME} PRIVATE -fstack-protector-strong)
  target_link_options(${PROJECT_NAME} PRIVATE -Wl,-z,relro -Wl,-z,now)
endif()

if(ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
  if(ipo_supported)
    set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(WARNING "LTO not supported: ${ipo_error}")
  endif()
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR})

set(CPACK_GENERATOR ZIP)
if(WIN32)
  install(FILES LICENSE.txt DESTINATION .)
  install(FILES elfbsp.cfg DESTINATION .)
  install(FILES docs/changelog.md DESTINATION .)
  install(FILES docs/command_line.md DESTINATION .)
  install(FILES docs/special_effects.md DESTINATION .)
  install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
else()
  include(GNUInstallDirs)
  install(FILES LICENSE.txt DESTINATION "share/doc/${PROJECT_NAME}")
  install(FILES elfbsp.cfg DESTINATION "share/doc/${PROJECT_NAME}")
  install(FILES docs/changelog.md DESTINATION "share/doc/${PROJECT_NAME}")
  install(FILES docs/command_line.md DESTINATION "share/doc/${PROJECT_NAME}")
  install(FILES docs/special_effects.md DESTINATION "share/doc/${PROJECT_NAME}")
  install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
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

set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_${CPACK_SYSTEM_NAME}")

include(CPack)
