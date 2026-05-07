Changes since ELFBSP 1.2
========================

New features:
* Added simple performance profiler, benchmarking the time needed for certain parts of the code to finish work
* Added support for the brand new XBM1 32-bit blockmap lump format
* Added support for the Doom 64 binary map format, and its `LEAFS` lump
** Includes the DeePBSPV4 BSP tree lump format for large maps exceeding the vanilla limits
** Includes the XBM1 blockmap lump format for large maps exceeding the vanilla limits
* Changed default BSP tree lump format in Doom & Hexen map formats to XNOD, this will help massively reduce slime trails
* Added support for new special effects in Doom 64 map format levels
** Sector special of 999 is blocked from the reject grouping algorithm, makes enemies within it "blind"
** Lines containing the "Do not add to blockmap" will be excluded the blockmap
** Lines containing both of the "Show on automap" AND "Hide from automap" will be excluded the blockmap

Bugfixes:
* Restored `REJECT` builder's debug logging, i.e fix `--debug-reject` not working before
* Fixed map format detection loading UDMF level as Hexen map format levels
