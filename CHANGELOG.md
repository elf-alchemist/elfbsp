Changes since ELFBSP 1.2
========================

Features:
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
* Added support for the Doom 64 binary map format, and its `LEAFS` lump
  * Includes the DeePBSPV4 BSP tree lump format for large maps exceeding the vanilla limits
  * Includes the XBM1 blockmap lump format for large maps exceeding the vanilla limits
* Added support for new special effects in Doom 64 map format levels
  * Sector special of 999 is blocked from the reject grouping algorithm, makes enemies within it "blind"
  * Lines containing the "Do not add to blockmap" linedef flag will be excluded from the blockmap
  * Lines containing _both_ the "Show on automap" AND "Hide from automap" linedef flags will be excluded the blockmap
* Added simple performance profiler on CLI output, benchmarking the time taken by certain parts of the builder to work
* Added `--skip-bsp`, `--skip-bmap` and `--skip-reject` CLI options to skip building maps that are already compiled

Bugfixes:
* Restored `REJECT` builder's debug logging, i.e fix `--debug-reject` not working before
* Fixed critical UDMF reading issues
  * Fixed map format detection loading UDMF levels as Hexen map format levels
  * Fixed long-standing borked UDMF lump parsing
