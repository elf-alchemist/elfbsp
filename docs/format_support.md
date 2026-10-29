# Support
There exists various formats for level data and compiled level data in Doom, and related idtech1 engine projects:
* https://doomwiki.org/wiki/Map_format
* https://doomwiki.org/wiki/Node_builder

"Map format" refers to the actual data structures used in the container storage lumps, and determines how the level's geometry data is stored.
It does not strictly determine what the data itself means, which is often determined by the game type itself.

"Compiled level data" is a broad category of various data structures compiled during the development of a particular level.
This category includes the likes of the BSP tree, the blockmap, the reject matrix, ACS or MACROS scripts and even baked lighting.

The "BSP tree", or binary space partitioning tree, is a recursively-traversed structure used for software rendering, line-of-sight checks,
detecting sector where a given set of coordinates is located in, among others.

The "blockmap" is a series of 128x128 blocks which list the linedefs that intersect said block, used in collision detection.

The "reject matrix" is a sector-to-sector visibility matrix and cuts down on the amount of unnecessary line-of-sight checks.
That is to say, the reject matrix stores information, on whether any particular enemy in a given sector is guaranteed to _NOT_ be able to see the player.

The "BEHAVIOR" and/or "MACROS" lumps, store compiled ACS or BLAM scripts, created by a level designer.

The "baked lighting" structures store pre-calculated lighting information for the various surfaces around the level.

## ELFBSP Support matrix

There are some important details to consider when looking into the reasons for each data structure being supported or not, making certain combinations valid, invalid or valid but not worth or meaningful to support.

Ultimately there two kinds of BSP tree, those that are HW renderer friendly, and those that aren't -- any other differences boil down to minor details. Usually this means the ability to index 32-bit values instead of 16-bit values, which matters even in binary map formats bound by 16-bit, as the number of the elements in the BSP tree can still exceed that limit in large enough maps.

The DoomBSP format is the original one, created by 'id Software' for the eponymous compilation tool, when it used with the classic Doom and Hexen formats it is inheritely SW renderer only, due to short vertex coordinates, and a lack of strictly convex subsectors. While the Doom 64 map format uses fractional vertex coordinates and has LEAFS to help enable HW renderer friendly BSP tree, while being otherwise identical to normal non-HW-friendly BSP tree.

The DeePBSPV4 format is otherwise identical to the DoomBSP format, but it is uses 32-bit indexes instead of 16-bit ones, allowing for much larger and/or detailed maps. It does not address the short vertex coordinates issue in the Doom and Hexen map formats, but is otherwise still ideal for Doom 64 format maps.

The ZDBSP family of formats is a more "modern" set compared to the previous two, despite ZNOD and ZGLN coming before DeePBSPV4 was made. This family uses 32-bit index and fractional coordinates by default, attacking the short vertex coordinates issue in the Doom and Hexen map formats, but is otherwise pointless in the Doom 64 format, aside from compression.

| Format    | Doom               | Hexen              | Doom 64            | UDMF               |
|-----------|--------------------|--------------------|--------------------|--------------------|
| DoomBSP   | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | N/A                |
| DeePBSPV4 | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | N/A                |
| XNOD/ZNOD | :heavy_check_mark: | :heavy_check_mark: | N/A                | :heavy_check_mark: |
| XGLN/ZGLN | :heavy_check_mark: | :heavy_check_mark: | N/A                | :heavy_check_mark: |
| XGL2/ZGL2 | :heavy_check_mark: | :heavy_check_mark: | N/A                | :heavy_check_mark: |
| XGL3/ZGL3 | :heavy_check_mark: | :heavy_check_mark: | N/A                | :heavy_check_mark: |
| —         | —                  | —                  | —                  | —                  |
| GL V1     | :x:                | :x:                | N/A                | N/A                |
| GL V2     | :x:                | :x:                | N/A                | N/A                |
| GL V3     | :x:                | :x:                | N/A                | N/A                |
| GL V4     | :x:                | :x:                | N/A                | N/A                |
| GL V5     | :x:                | :x:                | N/A                | N/A                |
| —         | —                  | —                  | —                  | —                  |
| Blockmap  | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| XBM1      | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| —         | —                  | —                  | —                  | —                  |
| Reject    | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| GL PVS    | :x:                | :x:                | N/A                | N/A                |
| —         | —                  | —                  | —                  | —                  |
| Behavior  | N/A                | :x:                | N/A                | :x:                |
| Macros    | N/A                | N/A                | :x:                | :x:                |
| —         | —                  | —                  | —                  | —                  |
| DLight    | :x:                | :x:                | :x:                | N/A                |
| ZDRay     | N/A                | N/A                | N/A                | :x:                |
| —         | —                  | —                  | —                  | —                  |
