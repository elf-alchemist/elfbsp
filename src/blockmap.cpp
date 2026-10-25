//------------------------------------------------------------------------------
//
//  ELFBSP
//
//------------------------------------------------------------------------------
//
//  Copyright 2025-2026 Guilherme Miranda
//  Copyright 2000-2018 Andrew Apted, et al
//  Copyright 1994-1998 Colin Reed
//  Copyright 1997-1998 Lee Killough
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------------

#include "core.hpp"
#include "local.hpp"
#include <source_location>
#include <vector>

static constexpr size_t BlockMapGrid_BitShift = 7;
static constexpr auto BlockMap_GridSize = 128;
static constexpr auto BlockMap_GridMask = 127;

/* ----- create blockmap ------------------------------------ */

static void Blockmap_FindLimits(Level &level, MapBoundBox &bbox)
{
  double mid_x = 0;
  double mid_y = 0;
  int16_t block_mid_x = 0;
  int16_t block_mid_y = 0;

  bbox.minx = bbox.miny = Limits<int16_t>::max();
  bbox.maxx = bbox.maxy = Limits<int16_t>::min();

  for (auto L : level.linedefs)
  {
    if (HAS_BIT(L->effects, FX_NoBlockmap | FX_ZeroLength))
    {
      continue;
    }

    double x1 = L->start->x;
    double y1 = L->start->y;
    double x2 = L->end->x;
    double y2 = L->end->y;

    int16_t lx = FloatToShort(floor(std::min(x1, x2)));
    int16_t ly = FloatToShort(floor(std::min(y1, y2)));
    int16_t hx = FloatToShort(ceil(std::max(x1, x2)));
    int16_t hy = FloatToShort(ceil(std::max(y1, y2)));

    if (lx < bbox.minx) bbox.minx = lx;
    if (ly < bbox.miny) bbox.miny = ly;
    if (hx > bbox.maxx) bbox.maxx = hx;
    if (hy > bbox.maxy) bbox.maxy = hy;

    // compute middle of cluster
    mid_x += (lx + hx) >> 1;
    mid_y += (ly + hy) >> 1;
  }

  if (level.linedefs.size() > 0)
  {
    block_mid_x = FloatToShort(floor(mid_x / static_cast<double>(level.linedefs.size())));
    block_mid_y = FloatToShort(floor(mid_y / static_cast<double>(level.linedefs.size())));
  }

  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(Log::DEBUG, "[{}] Blockmap lines centered at ({},{})", __func__, block_mid_x, block_mid_y);
  }
}

void Blockmap_Init(Level &level, BlockMap &bmap)
{
  MapBoundBox bbox;

  // find limits of linedefs, and store as map limits
  Blockmap_FindLimits(level, bbox);

  // Align to sub grid of 8 map unit step sizes
  // Equivalent to
  //  * (Coordinate - (Coordinate % 8))
  //  * (Coordinate &= ~7)
  bmap.origin_x = bbox.minx - (bbox.minx & 0x7);
  bmap.origin_y = bbox.miny - (bbox.miny & 0x7);

  bmap.columns = static_cast<size_t>((bbox.maxx - bmap.origin_x) >> BlockMapGrid_BitShift) + OFF_BY_ONE;
  bmap.rows = static_cast<size_t>((bbox.maxy - bmap.origin_y) >> BlockMapGrid_BitShift) + OFF_BY_ONE;

  bmap.cells.assign(bmap.BlockCount(), BlockList());

  for (auto i : Iota(bmap.BlockCount()))
  {
    bmap.cells[i].blk_num = i;
  }

  bmap.offsets.reserve(bmap.BlockCount());
  bmap.unpacked_offsets.reserve(bmap.BlockCount());

  if (config.verbose)
  {
    PrintLine(Log::NORMAL, "[{}] Map limits: ({},{}) to ({},{})", __func__, bbox.minx, bbox.miny, bbox.maxx, bbox.maxy);
    PrintLine(Log::NORMAL, "[{}] Origins are at ({},{})", __func__, bmap.origin_x, bmap.origin_y);
    PrintLine(Log::NORMAL, "[{}] There are {} columns and {} rows", __func__, bmap.columns, bmap.rows);
    PrintLine(Log::NORMAL, "[{}] Totalling {} blocks", __func__, bmap.BlockCount());
  }
}

static void Blockmap_Free(BlockMap &bmap)
{
  for (auto cell : bmap.cells)
  {
    cell.lines.clear();
  }

  bmap.cells.clear();
  bmap.blocks.clear();
  bmap.offsets.clear();
  bmap.unpacked_blocks.clear();
  bmap.unpacked_offsets.clear();
}

static void Block_Add(BlockMap &bmap, size_t blk_num, size_t line_index)
{
  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(Log::DEBUG, "[{}] Linedef {} is being added to block {}", __func__, line_index, blk_num);
  }

  if (blk_num >= bmap.BlockCount())
  {
    throw RuntimeException("BlockAdd: bad block number {}", blk_num);
  }

  auto &blk = bmap.cells[blk_num];

  // compute new checksum
  blk.sum.Compute(line_index);
  blk.lines.push_back(line_index);
}

static void Block_AddLine(BlockMap &bmap, const Linedef *L)
{
  auto x1 = FloatToShort(L->start->x);
  auto y1 = FloatToShort(L->start->y);
  auto x2 = FloatToShort(L->end->x);
  auto y2 = FloatToShort(L->end->y);

  size_t line_index = L->index;

  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(Log::DEBUG, "[{}] {} ({},{}) -> ({},{})", __func__, line_index, x1, y1, x2, y2);
  }

  // TODO, this really isn't needed, we don't support truncated blockmaps anymore
  auto bx1_temp = (std::min(x1, x2) - bmap.origin_x) >> BlockMapGrid_BitShift;
  auto by1_temp = (std::min(y1, y2) - bmap.origin_y) >> BlockMapGrid_BitShift;
  auto bx2_temp = (std::max(x1, x2) - bmap.origin_x) >> BlockMapGrid_BitShift;
  auto by2_temp = (std::max(y1, y2) - bmap.origin_y) >> BlockMapGrid_BitShift;

  size_t bx1 = static_cast<size_t>(std::max(bx1_temp, 0));
  size_t by1 = static_cast<size_t>(std::max(by1_temp, 0));
  size_t bx2 = static_cast<size_t>(std::min(bx2_temp, static_cast<int32_t>(bmap.columns - OFF_BY_ONE)));
  size_t by2 = static_cast<size_t>(std::min(by2_temp, static_cast<int32_t>(bmap.rows - OFF_BY_ONE)));

  if (bx2 < bx1 || by2 < by1)
  {
    return;
  }

  // handle simple case #1: completely horizontal
  if (by1 == by2)
  {
    for (size_t bx = bx1; bx <= bx2; bx++)
    {
      size_t blk_num = by1 * bmap.columns + bx;
      Block_Add(bmap, blk_num, line_index);
    }
    return;
  }

  // handle simple case #2: completely vertical
  if (bx1 == bx2)
  {
    for (size_t by = by1; by <= by2; by++)
    {
      size_t blk_num = by * bmap.columns + bx1;
      Block_Add(bmap, blk_num, line_index);
    }
    return;
  }

  // handle the rest (diagonals)

  for (size_t by = by1; by <= by2; by++)
  {
    for (size_t bx = bx1; bx <= bx2; bx++)
    {
      size_t blk_num = bx + by * bmap.columns;

      auto minx = bmap.origin_x + BlockMap_GridSize * static_cast<int32_t>(bx);
      auto miny = bmap.origin_y + BlockMap_GridSize * static_cast<int32_t>(by);
      auto maxx = minx + BlockMap_GridMask;
      auto maxy = miny + BlockMap_GridMask;

      if (CheckLinedefInsideBox(minx, miny, maxx, maxy, x1, y1, x2, y2))
      {
        Block_Add(bmap, blk_num, line_index);
      }
    }
  }
}

// initial phase: create internal blockmap containing the index of
// all lines in each block.
static void Blockmap_Create(Level &level, BlockMap &bmap)
{
  for (auto blk_num : Iota(bmap.BlockCount()))
  {
    Block_Add(bmap, blk_num, 0);
  }

  for (auto L : level.linedefs)
  {
    if (HAS_BIT(L->effects, FX_NoBlockmap | FX_ZeroLength))
    {
      continue;
    }

    Block_AddLine(bmap, L);
  }

  // Force extended format
  if (bmap.format < BlockmapFormat::XBM1 && level.linedefs.size() > LIMIT_LINE)
  {
    PrintLine(Log::WARN, "[{}] Blockmap overflow. Forcing XBM1 format.", __func__);
    config.total_warnings++;
    RaiseValue(bmap.format, BlockmapFormat::XBM1);
  }
}

// Place the uncompressed data, so we can get compression ratios after
static void Blockmap_FlattenUncompressed(BlockMap &bmap)
{
  // Skip header and offsets list
  size_t offset = 4 + bmap.BlockCount();

  PrintLine(Log::NORMAL, "[{}] Offsets starting at {}", __func__, offset);

  for (auto cell : bmap.cells)
  {
    bmap.unpacked_offsets.push_back(offset);
    for (auto line : cell.lines)
    {
      bmap.unpacked_blocks.push_back(line);
    }
    bmap.unpacked_blocks.push_back(NO_INDEX);
    offset += cell.lines.size() + OFF_BY_ONE;
  }
  PrintLine(Log::NORMAL, "[{}] Offsets ending at {}", __func__, offset);
}

void Blockmap_Deduplicate(BlockMap &bmap)
{
  // TODO
}

void Blockmap_FlattenDeduplicated(BlockMap &bmap)
{
  size_t offset = 4 + bmap.BlockCount();

  PrintLine(Log::NORMAL, "[{}] Offsets starting at {}", __func__, offset);

  for (auto cell : bmap.cells)
  {
    bmap.offsets.push_back(offset);

    // Must be sorted, with duplicates marked
    // TODO: make this not need sorting of any kind, just use dup_index
    if (cell.dup_index != NO_INDEX) continue;

    for (auto line : cell.lines)
    {
      bmap.blocks.push_back(line);
    }
    bmap.blocks.push_back(NO_INDEX);
    offset += cell.lines.size() + OFF_BY_ONE;
  }
  PrintLine(Log::NORMAL, "[{}] Offsets ending at {}", __func__, offset);
}

template <BlockmapFormat format, typename NumType>
static void Blockmap_Write(WadIO &io, BlockMap &bmap)
{
  constexpr size_t NumSize = sizeof(NumType);

  if constexpr (format == BlockmapFormat::XBM1)
  {
    io.AddToLump("XBM1\0\0\0\0", 8);

    MapBlockmapHeaderXBM1 xbm1_header;
    xbm1_header.x_origin = GetLittleEndian(IntToFixed(bmap.origin_x));
    xbm1_header.y_origin = GetLittleEndian(IntToFixed(bmap.origin_y));
    xbm1_header.x_blocks = GetLittleEndian(IndexToInt(bmap.columns));
    xbm1_header.y_blocks = GetLittleEndian(IndexToInt(bmap.rows));
    io.AddToLump(&xbm1_header, sizeof(xbm1_header));
  }
  else
  {
    MapBlockmapHeader header;
    header.x_origin = GetLittleEndian(bmap.origin_x);
    header.y_origin = GetLittleEndian(bmap.origin_y);
    header.x_blocks = GetLittleEndian(IndexToShort(bmap.columns));
    header.y_blocks = GetLittleEndian(IndexToShort(bmap.rows));
    io.AddToLump(&header, sizeof(header));
  }

  std::vector<NumType> lump_data;
  lump_data.reserve(bmap.offsets.size() + bmap.blocks.size());

  for (auto offset : bmap.offsets)
  {
    lump_data.push_back(GetLittleEndian(static_cast<NumType>(offset)));
  }

  for (auto block : bmap.blocks)
  {
    lump_data.push_back(GetLittleEndian(static_cast<NumType>(block)));
  }

  io.AddToLump(lump_data.data(), NumSize * lump_data.size());
}

void PutBlockmap(WadIO &io, Level &level)
{
  if (level.skip_bmap)
  {
    io.LevelCopyLump(level.lumps.label, "BLOCKMAP");
    return;
  }

  auto mark = Benchmarker(__func__);

  BlockMap bmap;
  io.StartWritingLump("BLOCKMAP");

  // just leave an empty blockmap lump
  if (level.linedefs.size() == 0)
  {
    return;
  }

  RaiseValue(bmap.format, config.bmap_format);

  Blockmap_Init(level, bmap);
  Blockmap_Create(level, bmap);
  Blockmap_FlattenUncompressed(bmap);
  Blockmap_Deduplicate(bmap);
  Blockmap_FlattenDeduplicated(bmap);
  // TODO: Blockmap_SubBlock(bmap);
  // TODO: Blockmap_FlattenSubBlock(bmap);

  switch (bmap.format)
  {
  case BlockmapFormat::DoomBSP:
    Blockmap_Write<BlockmapFormat::DoomBSP, uint16_t>(io, bmap);
    break;
  case BlockmapFormat::XBM1:
    Blockmap_Write<BlockmapFormat::XBM1, uint32_t>(io, bmap);
    break;
  default:
    // how did we get here
    // leave an empty blockmap lump
    PrintLine(Log::WARN, "[{}] Blockmap overflowed (lump will be empty)", __func__);
    config.total_warnings++;
    break;
  }

  if (config.verbose)
  {
    PrintLine(Log::NORMAL, "[{}] Blockmap size: {}x{} (compression: {}%)", __func__, bmap.columns, bmap.rows,
              static_cast<int32_t>(bmap.block_compression * 100));
  }
  Blockmap_Free(bmap);
}
