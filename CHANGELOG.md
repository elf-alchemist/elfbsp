Changes since ELFBSP 1.0
========================

Bugfixes:
* Fixed a bug where freshly created Doom/Hexen maps would be built with incorrect lump orders
* i.e. SECTORS would not be placed between NODES and REJECT, but would remain placed right after VERTEXES
