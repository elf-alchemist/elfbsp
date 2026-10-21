Changes since ELFBSP 1.2
========================

Features:
* Added simple performance profiler, benchmarking the time taken by certain parts of the builder to work
* Added support for the new XBM1 32-bit blockmap lump format
  * Instead of outputting empty BLOCKMAP lumps when the vanilla format overflows, it will output XBM1 lumps
* Added support for the Doom 64 binary map format, and its `LEAFS` lump
  * Includes the DeePBSPV4 BSP tree lump format for large maps exceeding the vanilla limits
  * Includes the XBM1 blockmap lump format for large maps exceeding the vanilla limits
* Changed default BSP tree lump format in Doom & Hexen map formats to XNOD, this will help massively reduce slime trails
* Added support for new special effects in Doom 64 map format levels
  * Sector special of 999 is blocked from the reject grouping algorithm, makes enemies within it "blind"
  * Lines containing the "Do not add to blockmap" linedef flag will be excluded from the blockmap
  * Lines containing _both_ the "Show on automap" AND "Hide from automap" linedef flags will be excluded the blockmap
* Restored support for compressed variants of ZDBSP formats (ZNOD, ZGLN, ZGL2, ZGL3)
  * ZGL3 is now the default for UDMF levels
  * The new `--compress` CLI flag will force the use of the compressed counterparts in the Doom & Hexen map formats
* Added `--skip-bsp`, `--skip-bmap` and `--skip-reject` CLI options to skip building maps that are already compiled

Bugfixes:
* Restored `REJECT` builder's debug logging, i.e fix `--debug-reject` not working before
* Fixed map format detection loading UDMF level as Hexen map format levels
