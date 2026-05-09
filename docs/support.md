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
| GL PVS    | :x:                | :x:                | N/A                | N/A                |
| —         | —                  | —                  | —                  | —                  |
| Blockmap  | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| XBM1      | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| —         | —                  | —                  | —                  | —                  |
| Reject    | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| —         | —                  | —                  | —                  | —                  |
| Behavior  | N/A                | :x:                | N/A                | :x:                |
| Macros    | N/A                | N/A                | :x:                | :x:                |
| —         | —                  | —                  | —                  | —                  |
| DLight    | :x:                | :x:                | :x:                | N/A                |
| ZDRay V?  | N/A                | N/A                | N/A                | :x:                |
| —         | —                  | —                  | —                  | —                  |
