# Formats for BSP tree data structures
The "binary space partition" tree is the core data structure used in Doom's [software rendering engine](https://doomwiki.org/wiki/Doom_rendering_engine).
Compiled after the mapper-facing data for the level has already been put together (i.e. Things, Vertexes, Lines, Sides and Sectors).
It is comprised of Nodes, SubSectors and Segments -- the renderer recursively traversees the Nodes tree, until encoutering a SubSector, then drawing its associated Segments.

| BSP tree type | ZIP compressed version | Lump lagic number   | ZIP lump magic number |
|---------------|------------------------|---------------------|-----------------------|
| Vanilla       |                        |                     |                       |
| DeepBSP       |                        | "`xNd4\0\0\0\0`"    |                       |
| XNOD          | ZNOD                   | "`XNOD`"            | "`ZNOD`"              |
| XGLN          | ZGLN                   | "`XGLN`"            | "`ZGLN`"              |
| XGL2          | ZGL2                   | "`XGL2`"            | "`ZGL2`"              |
| XGL3          | ZGL3                   | "`XGL3`"            | "`ZGL3`"              |

Despite the confusing nomenclture, the XGL/ZGL types have nothing to do with OpenGL rendering -- hardware rendering data follows a difference structure entirely.

## Type definitions
For development purposes, all of these following are assumed to be "packed structs", as opposed to "memory-aligned structs".
Assume standard C99 fixed-width integer types:
* `uint8_t`
* `uint16_t`
* `uint32_t`
* `int16_t`
* `int32_t`

Doom uses "-1" (either 16bit 0xFFFF or 32bit 0xFFFFFFFF) to represent the lack of an index in index types -- henceforth `NO_INDEX`.

## Vanilla types

### Vanilla Blockmap structure
Pulled from the Doomwiki page on the [BLOCKMAP](https://doomwiki.org/wiki/Blockmap) lump.

#### Bounding box (`bbox_t`)

| Type      | Description |
|-----------|-------------|
| `int16_t` | Maximum Y coordinate |
| `int16_t` | Minimum Y coordinate |
| `int16_t` | Minimum X coordinate |
| `int16_t` | Maximum X coordinate |

#### Blockmap header (`blockmap_header_t`)

| Type      | Description |
|-----------|-------------|
| `int16_t` | Grid origin X coordinate |
| `int16_t` | Grid origin Y coordinate |
| `int16_t` | Number of columns        |
| `int16_t` | Number of rows           |

## Vanilla BSP structures
These are the data structure definitions associated with the vanilla BSP tree implementation.

### Vanilla node (`doom_node_t`), 28 bytes
Pulled from the Doomwiki page on the [NODES](https://doomwiki.org/wiki/Node) lump.

| Type       | Description |
|------------|-------------|
| `int16_t`  | Starting point X coordinate |
| `int16_t`  | Starting point Y coordinate |
| `int16_t`  | Delta from X to ending point |
| `int16_t`  | Delta from Y to ending point |
| `bbox_t`   | Bounding box on right side |
| `bbox_t`   | Bounding box on left side |
| `uint16_t` | Child node index for right side -- if higest bit is set, is a subsector |
| `uint16_t` | Child node index for left side -- if higest bit is set, is a subsector |

### Vanilla subsector (`doom_subsec_t`), 4 bytes
Pulled from the Doomwiki page on the [SSECTORS](https://doomwiki.org/wiki/Subsector) lump.

| Type       | Description |
|------------|-------------|
| `uint16_t` | Number of segments in this subsector |
| `uint16_t` | Index of first segment |

### Vanilla segment (`doom_seg_t`), 12 bytes
Pulled from the Doomwiki page on the [SEGS]( https://doomwiki.org/wiki/Seg) lump.

| Type       | Description |
|------------|-------------|
| `uint16_t` | Index of starting vertex |
| `uint16_t` | Index of ending vertex |
| `uint16_t` | BAM angle, 0 is east, 16384 is north, and so on |
| `uint16_t` | Index of associated linedef |
| `uint16_t` | False (0) if on the same side as linedef, true (1) if oppposite side |
| `uint16_t` | Offset from starting point to ending point |

## DeepSea BSP structures
These are the data structure definitions associated with the DeepSea editor's BSP format extension.
The sole difference from the vanilla BSP types, is that the types referring to indexes are now 32bit instead of 16bit, but are otherwise indentical.
Pulled from the official [DeepBSPV4 webpage spec](https://www.sbsoftware.com/files/DeePBSPV4specs.txt).

### BeepBSP node (`deep_node_t`), 32 bytes

| Type       | Description |
|------------|-------------|
| `int16_t`  | Starting point X coordinate |
| `int16_t`  | Starting point Y coordinate |
| `int16_t`  | Delta from X to ending point |
| `int16_t`  | Delta from Y to ending point |
| `bbox_t`   | Bounding box on right side |
| `bbox_t`   | Bounding box on left side |
| `uint32_t` | Child node index for right side -- if higest bit is set, is a subsector |
| `uint32_t` | Child node index for left side -- if higest bit is set, is a subsector |

### BeepBSP subsector (`deep_subsec_t`), 6 bytes

| Type       | Description |
|------------|-------------|
| `uint16_t` | Number of segments in this subsector |
| `uint32_t` | Index of first segment |

### BeepBSP segment (`deep_seg_t`), 16 bytes

| Type       | Description |
|------------|-------------|
| `uint32_t` | Index of starting vertex |
| `uint32_t` | Index of ending vertex |
| `uint16_t` | BAM angle, 0 is east, 16384 is north, and so on |
| `uint16_t` | Index of associated linedef |
| `uint16_t` | False (0) if on the same side as linedef, true (1) if oppposite side |
| `uint16_t` | Offset from starting point to ending point |

## ZDoom extented BSP structures
Pulled from the ZDoom wiki page on the [extended Node formats](https://zdoom.org/w/index.php?title=Node#ZDoom_extended_nodes).
Differences from the vanilla BSP data include:
* the use of 32bit indexes
* storing all data in a specific sequence on a single lump, as opposed to using multiple lumps (`NODES` for XNOD, and `SSECTOR` for XGLN/2/3).
* each version is built on top of the previous, replacing the predecessor's given struct type
* the removal of angles and offsets from Segments
* higher precision data types for fractional coordinates
* "mini-segs", indicated by `NO_INDEX`, to be skipped by the renderer, but needed for traversal
* "linear" storage of Seg indexes on subsectors, see above

### XNOD vertex (`xnod_vertex_t`), 8 bytes

| Type      | Description |
|-----------|-------------|
| `int32_t` | Fixed point X coordinate |
| `int32_t` | Fixed point Y coordinate |

### XNOD node (`xnod_node_t`), 32 bytes

| Type       | Description |
|------------|-------------|
| `int16_t`  | Starting point X coordinate |
| `int16_t`  | Starting point Y coordinate |
| `int16_t`  | Delta from X to ending point |
| `int16_t`  | Delta from Y to ending point |
| `bbox_t`   | Bounding box on right side |
| `bbox_t`   | Bounding box on left side |
| `uint32_t` | Child node index for right side -- if higest bit is set, is a subsector |
| `uint32_t` | Child node index for left side -- if higest bit is set, is a subsector |


### XNOD subsector (`xnod_subsec_t`), 4 bytes

| Type       | Description |
|------------|-------------|
| `uint32_t` | Number of segments in this subsector |

### XNOD segment (`xnod_seg_t`), 11 bytes

| Type       | Description |
|------------|-------------|
| `uint32_t` | Index of starting vertex |
| `uint32_t` | Index of ending vertex |
| `uint16_t` | Index of associated linedef, `NO_INDEX` if mini-seg |
| `uint8_t`  | False (0) if on the same side as linedef, true (1) if oppposite side |

### XGLN segment (`xgln_seg_t`), 11 bytes

| Type       | Description |
|------------|-------------|
| `uint32_t` | Index of starting vertex |
| `uint32_t` | Index of ending vertex (Unused, due to the linearity of the subsector structure) |
| `uint16_t` | Index of associated linedef, `NO_INDEX` if mini-seg |
| `uint8_t`  | False (0) if on the same side as linedef, true (1) if oppposite side |

### XGL2 segment (`xgl2_seg_t`), 13 bytes

| Type       | Description |
|------------|-------------|
| `uint32_t` | Index of starting vertex |
| `uint32_t` | Index of ending vertex (Unused, due to the linearity of the subsector structure) |
| `uint32_t` | Index of associated linedef, `NO_INDEX` if mini-seg |
| `uint8_t`  | False (0) if on the same side as linedef, true (1) if oppposite side |

### XGL3 node (`xgl3_node_t`), 40 bytes

| Type       | Description |
|------------|-------------|
| `int32_t`  | Starting point X coordinate |
| `int32_t`  | Starting point Y coordinate |
| `int32_t`  | Delta from X to ending point |
| `int32_t`  | Delta from Y to ending point |
| `bbox_t`   | Bounding box on right side |
| `bbox_t`   | Bounding box on left side |
| `uint32_t` | Child node index for right side -- if higest bit is set, is a subsector |
| `uint32_t` | Child node index for left side -- if higest bit is set, is a subsector |
