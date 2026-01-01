Changes since previous release: AJBSP 1.07
==========================================

New features:
* Added initial, experimental support for build-time line specials from ZokumBSP
* These line specials are only supported in Doom-format maps
* Special 999, which prevents a line from being added to the BlockMap building process, is known to work correctly.
* Specials 1080 through to 1083, which rotate a Seg's angle, are known to work correctly (however, only supported on vanilla Doom node format)
* Specials 998, and 1084 through to 1086, which prevent the generation of a given Seg, are NOT guranteed to work properlly -- only use this for testing!

Build system:
* Added automated GitHub Actions CI
* Now using only CMake, instead of bespoke Makefile
* Now compiled with GCC for Linux, Clang for MacOS, and MinGW for Windows

Feature regressions:
* Removed support for generating hardware rendering node formats (i.e. `GL_`-prefixed lumps)
* Removed support for generating compressed software rendering nodes (i.e. ZNOD, ZGLN, ZGL2 & ZGL3)
