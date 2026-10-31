# ELFBSP 1.3

New features:
* Added support for the new XBM1 32-bit blockmap lump format
  * Instead of outputting empty BLOCKMAP lumps when the vanilla format overflows, it will output XBM1 lumps
* Changed default BSP tree lump format in Doom & Hexen map formats to XNOD, this will help massively reduce slime trails
* Restored support for compressed variants of ZDBSP formats (ZNOD, ZGLN, ZGL2, ZGL3)
  * ZGL3 is now the default for UDMF levels
  * The new `--compress` CLI flag will force the use of the compressed counterparts in the Doom & Hexen map formats
* Improved blockmap compression for smaller lump size
  * Added automatic removal of non-special self-referencing linedefs from the blockmap
  * Revised "identical blocklist deduplication" algorithm, now cuts down a few more blocks
  * Added "Blocklist size sorting" to squeeze out smaller offsets
  * Added `--blockmap-skip-zero-header` CLI flag for vanilla-optimized blockmaps
* Added support for the Doom 64 binary map format, and its `LEAFS` lump
  * Includes the DeePBSPV4 BSP tree lump format for large maps exceeding the vanilla limits
  * Includes the XBM1 blockmap lump format for large maps exceeding the vanilla limits
* Added support for proper `REJECT` building algorithm with visibility checks (thanks @KnightMB)
  * Using `--fast` makes the builder skip the visibility algorithm, and uses only the sector grouping algorithm
* Added support for special effects in Doom 64 map format levels
  * Sector special of 999 is blocked from the `REJECT` grouping algorithm, makes enemies within it "blind"
  * Lines containing the "Do not add to blockmap" linedef flag will be excluded from the blockmap
  * Lines containing _both_ the "Show on automap" AND "Hide from automap" linedef flags will be excluded the blockmap
* Added UDMF namespace awareness for polyobjects, DoomEdNums are now inferred automticaly from the namespace
  * Hexen polyobject editor numbers for the `Hexen` and `DelphiHexen` namespaces
  * ZDoom polyobject editor numbers for all other namespaces
  * The `--polyobj` CLI flag is now ignored for UDMF maps
* Added simple performance profiler on CLI output, benchmarking the time taken by certain parts of the builder to work
* Added `--skip-bsp`, `--skip-bmap` and `--skip-reject` CLI flags to skip building maps that are already compiled

Bugfixes:
* Restored `REJECT` builder's debug logging, i.e fix `--debug-reject` not working before
* Fixed critical UDMF reading issues
  * Fixed map format detection loading UDMF levels as Hexen map format levels
  * Fixed long-standing borked UDMF lump parsing

Meta:
* Removed support for Intel MacOS, only ARM MacOS (Apple Silicon) is now supported
* Fixed builds not being static, should now run on UDB Flatpak just fine
* Windows builds now use MSYS2 Clang instead of MSYS2 MinGW
* Added Linux GCC to CI pipeline

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
