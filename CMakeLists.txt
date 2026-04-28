# ELFBSP CMake Script
cmake_minimum_required(VERSION 3.15)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(CheckIncludeFile)

project(elfbsp
        VERSION "1.3.0"
        LANGUAGES C CXX
        HOMEPAGE_URL "https://elf-alchemist.github.io/elfbsp"
        # SPDX_LICENSE "GPL-2.0+" doesn't work?
        DESCRIPTION "Modern BSP tree builder for idTech1 games and source ports.")

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

# Grabs the git hash and branch a few different ways.
include(GetGitBranch)
include(GetGitRevisionDescription)
include(GetGitRevisionNumber)
get_git_head_revision(HEAD GIT_HASH)
git_describe(GIT_DESCRIBE --all --long --abbrev=8)
git_rev_count(HEAD GIT_REV_COUNT)
git_branch(HEAD GIT_BRANCH)
string(REGEX REPLACE "^(heads\/|tags\/)(.+)(-0-g)([0-9a-f]+)" "\\4" GIT_SHORT_HASH "${GIT_DESCRIBE}")

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
string(TIMESTAMP PROJECT_DATE "%Y-%m-%d %H:%M:%S UTC" UTC)

set(PROJECT_GENERIC_NAME "idTech1 BSP node builder")
set(PROJECT_CREATOR "elf-alchemist")
set(PROJECT_COPYRIGHT "Copyright \(C\) 1994-2026 - Guilherme Miranda, et al")
set(PROJECT_LICENSE "GNU General Public License, version 2")
set(PROJECT_SPDX_LICENSE "GPL-2.0+")

set(PROJECT_URL "https://github.com/elf-alchemist/elfbsp")
set(PROJECT_WIKI "${PROJECT_URL}/wiki")
set(PROJECT_ISSUES "${PROJECT_URL}/issues")

set(PROJECT_DEVELOPER_IDENTIFIER "io.github.elf-alchemist")
set(PROJECT_IDENTIFIER "${PROJECT_DEVELOPER_IDENTIFIER}.elfbsp")

set(PROJECT_VERSION_RC "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
set(PROJECT_ICO "${PROJECT_IDENTIFIER}.windows.ico")
set(PROJECT_SUMMARY "${PROJECT_DESCRIPTION} Based on Andrew Apted's AJBSP, with even more features.")

set(PROJECT_DESKTOP "${PROJECT_IDENTIFIER}.desktop")
set(PROJECT_INFO_PLIST "${PROJECT_IDENTIFIER}.Info.plist")
set(PROJECT_MANIFEST "${PROJECT_IDENTIFIER}.manifest.xml")
set(PROJECT_METAINFO "${PROJECT_IDENTIFIER}.metainfo.xml")
set(PROJECT_RESOURSE_RC "${PROJECT_IDENTIFIER}.version.rc")

set(MACOSX_BUNDLE_EXECUTABLE_NAME "${PROJECT_NAME}")
set(MACOSX_BUNDLE_INFO_STRING "${PROJECT_COPYRIGHT}")
set(MACOSX_BUNDLE_ICON_FILE "${PROJECT_IDENTIFIER}.macosx") # the *.icns extension is implied
set(MACOSX_BUNDLE_GUI_IDENTIFIER "${PROJECT_IDENTIFIER}")
set(MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}")
set(MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}")
set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
set(MACOSX_BUNDLE_COPYRIGHT "${PROJECT_COPYRIGHT}")

if(WIN32)
  configure_file("res/${PROJECT_MANIFEST}.in" "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_MANIFEST}" @ONLY)
  configure_file("res/${PROJECT_RESOURSE_RC}.in" "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_RESOURSE_RC}" @ONLY)
  target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_MANIFEST}" "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_RESOURSE_RC}")
elseif(APPLE)
  configure_file("res/${PROJECT_INFO_PLIST}.in" "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_INFO_PLIST}" @ONLY)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE FALSE # Temporary, GUI coming later
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_INFO_PLIST}"
  )
elseif(UNIX)
  configure_file("res/${PROJECT_DESKTOP}.in" "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_DESKTOP}"  @ONLY)
  configure_file("res/${PROJECT_METAINFO}.in" "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_METAINFO}" @ONLY)
endif()

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
  PROJECT_SPDX_LICENSE="${PROJECT_SPDX_LICENSE}"
  # Build stuff
  GIT_BRANCH="${GIT_BRANCH}"
  GIT_HASH="${GIT_HASH}"
  GIT_REV_COUNT="${GIT_REV_COUNT}"
  GIT_SHORT_HASH="${GIT_SHORT_HASH}"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_STANDARD 20
)

include(CheckedFlags)

if(NOT APPLE)
  _checked_add_compile_option(-stdlib=libc++)
  _checked_add_link_option(-stdlib=libc++ "-fuse-ld=lld")
  if (CONFIG MATCHES "Release" OR "RelWithDebInfo")
    _checked_add_compile_option(-ffunction-sections)
    _checked_add_compile_option(-fdata-sections)
    _checked_add_link_option(-Wl,--gc-sections)
  endif()
endif()

if(ELFBSP_STATIC AND NOT APPLE)
  if(WIN32)
    _checked_add_link_option(-static)
  else()
    _checked_add_link_option("-Wl,-Bstatic,-lc++,-lc++abi,-Bdynamic")
  endif()
endif()

_checked_add_compile_option(-Wextra)
_checked_add_compile_option(-Wshadow)
_checked_add_compile_option(-Wconversion)
_checked_add_compile_option(-Wsign-conversion)
_checked_add_compile_option(-Wno-unused-parameter)

_checked_add_compile_option(-fno-exceptions)
_checked_add_compile_option(-fno-rtti)
_checked_add_compile_option(-Wold-style-cast)

if(CONFIG MATCHES "Release" OR "RelWithDebInfo")
  _checked_add_compile_option(-O3)
elseif(CONFIG MATCHES "Debug")
  _checked_add_compile_option(-O0)
elseif(CONFIG MATCHES "MinSizeRel")
  _checked_add_compile_option(-Os)
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
  install(FILES LICENSE.txt DESTINATION "${CMAKE_INSTALL_DOCDIR}")
  install(FILES elfbsp.cfg DESTINATION "${CMAKE_INSTALL_DOCDIR}")
  install(FILES docs/changelog.md DESTINATION "${CMAKE_INSTALL_DOCDIR}")
  install(FILES docs/command_line.md DESTINATION "${CMAKE_INSTALL_DOCDIR}")
  install(FILES docs/special_effects.md DESTINATION "${CMAKE_INSTALL_DOCDIR}")
  if(NOT APPLE)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_DESKTOP}" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_METAINFO}" DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
  endif()
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
