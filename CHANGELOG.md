Changes since ELFBSP 1.2
========================

New features:
* Added simple performance profiler, benchmarking the time needed for certain parts of the code to finish work.
* Added support for the brand new XBM1 32-bit blockmap lump format.
* Added support for the Doom 64 binary map format, and its `LEAFS` lump.
* Changed default BSP tree lump format in Doom & Hexen map formats to XNOD, this will help massively reduce slime trails,

Bugfixes:
* Restored `REJECT` builder's debug logging, i.e fix `--debug-reject` not working before.
