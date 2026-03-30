# ELFBSP 1.2

New features:
* Added `--analysis` CLI parameter for BSP tree data analysis and visualization
* Added `--debug-*` CLI parameters for runtime debugging, previously a compile-time directive, now a runtime option
* Added support for the DeePBSPV4, XGLN and XGL2 node formats, now enforced via the `--type` CLI parameter
* Added support for special effects in Hexen-format maps via the line special 108
* Added `--polyobj` to make poly-object editor number detection more explicit, ZDoom editor numbers are now the default
* Added more information to `--analysis` output, thanks to Marc Rousseau's BSPInfo utility
* Added `--no-effects` to disable specials effects during build time, e.g tags 900-999, etc

Bugfixes:
* Improved correctness for certain special effects, numerical effects of number 998 & 999 are now read from the line's tag instead of line's special
* Improved internal handling of special effects, now being determined when reading certain map formats, i.e special effects that only apply on Doom-, Hexen- or UDMF formats, will no longer "leak" onto the other formats
* Improved autodetection of unbuilt Hexen-format levels

Meta:
* Added nightly builds on the GitHub Releases page, for easy access to development builds
* Ported various documenttaion files from other builders
* Moved to only building with LLVM Clang on all supported systems, now all three of the Windows, Mac OS and Linux builds use Clang.

# ELFBSP 1.1

Bugfixes:
* Fixed a bug where freshly created Doom/Hexen maps would be built with incorrect lump orders
* i.e. SECTORS would not be placed between NODES and REJECT, but would remain placed right after VERTEXES

# ELFBSP 1.0

New features:
* Added REJECT and BLOCKMAP building for UDMF maps.
* Added initial, experimental support for build-time line specials from ZokumBSP
* These line specials are only supported in Doom-format maps
* Special 999, which prevents a line from being added to the BlockMap building process, is known to work correctly.
* Specials 1080 through to 1083, which rotate a Seg's angle, are known to work correctly (however, only supported on vanilla Doom node format)
* Specials 998, and 1084 through to 1086, which prevent the generation of a given Seg, are NOT guranteed to work properlly -- only use this for testing!

Build system:
* Added automated GitHub Actions CI (Thanks @rfomin)
* Now using only CMake, instead of bespoke Makefile
* Now compiled with GCC for Linux, Clang for MacOS, and MinGW for Windows

Feature regressions:
* Removed support for generating hardware rendering node formats (i.e. `GL_`-prefixed lumps)
* Removed support for generating compressed software rendering nodes (i.e. ZNOD, ZGLN, ZGL2 & ZGL3)
