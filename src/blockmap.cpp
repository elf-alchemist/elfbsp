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

/*
 * Important bullshit to note:
 *  1. The original DoomBSP code wrote an erroneous 0-value at the start of every block list.
 *  2. TeamTNT, in their infinite Boom-designing wisdom, choose to always skip the erroneous
 *     value, where vanilla does not.
 *  3. ZokumBSP provides an option to not write down the cludge zero, this was used in BTSX
 *     due to vanilla EXE limits.
 *
 */

static constexpr size_t LIST_ZERO = 1;
static constexpr size_t LIST_END = 1;
static constexpr size_t EXTRA_LINES = LIST_ZERO + LIST_END;

static constexpr size_t m_zero = ZERO_INDEX;
static constexpr size_t m_neg1 = NO_INDEX;

static constexpr size_t NullBlockSize_Short = sizeof(uint16_t) * 2;
static constexpr size_t NullBlockSize_Long = sizeof(uint32_t) * 2;

using OffsetListStats = struct OffsetListStats
{
  size_t current_offset;
  size_t original_size;
  size_t new_size;
  size_t duplicate_count;
};

/* ----- create blockmap ------------------------------------ */

static void FindBlockmapLimits(level_t &level, bbox_t *bbox)
{
  double mid_x = 0;
  double mid_y = 0;
  int16_t block_mid_x = 0;
  int16_t block_mid_y = 0;

  bbox->minx = bbox->miny = SHRT_MAX;
  bbox->maxx = bbox->maxy = SHRT_MIN;

  for (size_t i = 0; i < level.linedefs.size(); i++)
  {
    const linedef_t *L = level.linedefs[i];

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

    if (lx < bbox->minx) bbox->minx = lx;
    if (ly < bbox->miny) bbox->miny = ly;
    if (hx > bbox->maxx) bbox->maxx = hx;
    if (hy > bbox->maxy) bbox->maxy = hy;

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
    PrintLine(LOG_DEBUG, "[%s] Blockmap lines centered at (%d,%d)", __func__, block_mid_x, block_mid_y);
  }
}

void InitBlockmap(level_t &level)
{
  bbox_t map_bbox;

  // find limits of linedefs, and store as map limits
  FindBlockmapLimits(level, &map_bbox);

  if (config.verbose)
  {
    PrintLine(LOG_NORMAL, "Map limits: (%d,%d) to (%d,%d)", map_bbox.minx, map_bbox.miny, map_bbox.maxx, map_bbox.maxy);
  }

  level.block_x = map_bbox.minx - (map_bbox.minx & 0x7);
  level.block_y = map_bbox.miny - (map_bbox.miny & 0x7);

  level.block_w = static_cast<size_t>((map_bbox.maxx - level.block_x) / 128) + 1;
  level.block_h = static_cast<size_t>((map_bbox.maxy - level.block_y) / 128) + 1;

  level.block_count = level.block_w * level.block_h;
}

static void FreeBlockmap(level_t &level)
{
  for (size_t i = 0; i < level.block_count; i++)
  {
    level.block_lines[i].lines.clear();
  }

  level.block_lines.clear();
  level.block_offsets.clear();
  level.block_duplicates.clear();
}

static void BlockAdd(level_t &level, size_t blk_num, size_t line_index)
{
  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(LOG_DEBUG, "[%s] Block %zu has line %zu", __func__, blk_num, line_index);
  }

  if (blk_num >= level.block_count)
  {
    PrintLine(LOG_ERROR, "ERROR: BlockAdd: bad block number %zu", blk_num);
  }

  auto &blk = level.block_lines[blk_num];

  // compute new checksum
  blk.hash = std::rotl(blk.hash, 4) ^ line_index;

  blk.lines.push_back(line_index);
}

static void BlockAddLine(level_t &level, const linedef_t *L)
{
  auto x1 = FloatToShort(L->start->x);
  auto y1 = FloatToShort(L->start->y);
  auto x2 = FloatToShort(L->end->x);
  auto y2 = FloatToShort(L->end->y);

  size_t line_index = L->index;

  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(LOG_DEBUG, "[%s] %zu (%d,%d) -> (%d,%d)", __func__, line_index, x1, y1, x2, y2);
  }

  auto bx1_temp = (std::min(x1, x2) - level.block_x) / 128;
  auto by1_temp = (std::min(y1, y2) - level.block_y) / 128;
  auto bx2_temp = (std::max(x1, x2) - level.block_x) / 128;
  auto by2_temp = (std::max(y1, y2) - level.block_y) / 128;

  // handle truncated blockmaps
  size_t bx1 = static_cast<size_t>(std::max(bx1_temp, 0));
  size_t by1 = static_cast<size_t>(std::max(by1_temp, 0));
  size_t bx2 = static_cast<size_t>(std::min(bx2_temp, static_cast<int32_t>(level.block_w - 1)));
  size_t by2 = static_cast<size_t>(std::min(by2_temp, static_cast<int32_t>(level.block_h - 1)));

  if (bx2 < bx1 || by2 < by1)
  {
    return;
  }

  // handle simple case #1: completely horizontal
  if (by1 == by2)
  {
    for (size_t bx = bx1; bx <= bx2; bx++)
    {
      size_t blk_num = by1 * level.block_w + bx;
      BlockAdd(level, blk_num, line_index);
    }
    return;
  }

  // handle simple case #2: completely vertical
  if (bx1 == bx2)
  {
    for (size_t by = by1; by <= by2; by++)
    {
      size_t blk_num = by * level.block_w + bx1;
      BlockAdd(level, blk_num, line_index);
    }
    return;
  }

  // handle the rest (diagonals)

  for (size_t by = by1; by <= by2; by++)
  {
    for (size_t bx = bx1; bx <= bx2; bx++)
    {
      size_t blk_num = bx + by * level.block_w;

      auto minx = level.block_x + 128 * static_cast<int32_t>(bx);
      auto miny = level.block_y + 128 * static_cast<int32_t>(by);
      auto maxx = minx + 127;
      auto maxy = miny + 127;

      if (CheckLinedefInsideBox(minx, miny, maxx, maxy, x1, y1, x2, y2))
      {
        BlockAdd(level, blk_num, line_index);
      }
    }
  }
}

// initial phase: create internal blockmap containing the index of
// all lines in each block.
static void CreateBlockmap(level_t &level)
{
  // What the fuck?
  level.block_lines.assign(level.block_count, blocklist_t{.hash = 0x1234123412341234, .lines = {}});

  for (size_t i = 0; i < level.linedefs.size(); i++)
  {
    const linedef_t *L = level.linedefs[i];

    if (HAS_BIT(L->effects, FX_NoBlockmap | FX_ZeroLength))
    {
      continue;
    }

    BlockAddLine(level, L);
  }

  // XBM2 allows 32bit line indices
  if (level.bmap_format < BMAP_XBM2 && level.linedefs.size() > LIMIT_LINE)
  {
    PrintLine(LOG_NORMAL, "WARNING: Blockmap overflow. Forcing XBM2 format.");
    config.total_warnings++;
    RaiseValue(level.bmap_format, BMAP_XBM2);
  }
}

// scan duplicate array and build up offset array
// offsets depend on type size, try with 16bit offset list first
bool CompressBlockmapWorker(level_t &level, auto &BlockCompare, OffsetListStats &stats, size_t NullBlockSize)
{
  stats.current_offset = level.block_count + NullBlockSize + 2;
  stats.original_size = level.block_count + NullBlockSize;
  stats.new_size = stats.current_offset;
  stats.duplicate_count = 0;

  for (size_t i = 0; i < level.block_count; i++)
  {
    size_t blk_num = level.block_duplicates[i];

    // empty block ?
    if (level.block_lines[blk_num].lines.empty())
    {
      level.block_offsets[blk_num] = level.block_count + NullBlockSize;
      level.block_duplicates[i] = NO_INDEX;

      stats.original_size += EXTRA_LINES;
      continue;
    }

    size_t count = level.block_lines[blk_num].lines.size() + EXTRA_LINES;

    // duplicate ?  Only the very last one of a sequence of duplicates
    // will update the current offset value.
    if (i + 1 < level.block_count && BlockCompare(level.block_duplicates[i], level.block_duplicates[i + 1]) == 0)
    {
      level.block_offsets[blk_num] = stats.current_offset;
      level.block_duplicates[i] = NO_INDEX;

      // free the memory of the duplicated block
      level.block_lines[blk_num].lines.clear();
      stats.duplicate_count++;
      stats.original_size += count;
      continue;
    }

    // OK, this block is either the last of a series of duplicates, or
    // just a singleton.
    level.block_offsets[blk_num] = stats.current_offset;
    stats.current_offset += count;
    stats.original_size += count;
    stats.new_size += count;
  }

  if (level.bmap_format == BMAP_DoomBlockmap && stats.current_offset > LIMIT_BMAP_OFFSET)
  {
    // Overflowed?
    PrintLine(LOG_NORMAL, "WARNING: Blockmap overflow. Forcing XBM1 format.");
    config.total_warnings++;
    RaiseValue(level.bmap_format, BMAP_XBM1);
    return true;
  }
  else
  {
    // Either small map with vanilla format, or any map with either XBM1/XBM2
    return false;
  }
}

// -AJA- second phase: compress the blockmap.  We do this by sorting
//       the blocks, which is a typical way to detect duplicates in
//       a large list.  This also detects BLOCKMAP overflow.
static void CompressBlockmap(level_t &level, auto &BlockCompare)
{
  OffsetListStats stats = {0,0,0,0};
  bool overflowed = false;

  level.block_offsets.reserve(level.block_count);
  level.block_duplicates.reserve(level.block_count);

  // sort duplicate-detecting array.  After the sort, all duplicates
  // will be next to each other.  The duplicate array gives the order
  // of the blocklists in the BLOCKMAP lump.
  for (size_t i = 0; i < level.block_count; i++)
  {
    level.block_duplicates[i] = i;
  }

  std::sort(level.block_duplicates.begin(), level.block_duplicates.end(), BlockCompare);

  // Is using vanilla format?
  if (level.bmap_format == BMAP_DoomBlockmap)
  {
    overflowed = CompressBlockmapWorker(level, BlockCompare, stats, NullBlockSize_Short);
  }

  // Is using any other format, OR, previous attempt exceeded vanilla limits?
  if (level.bmap_format != BMAP_DoomBlockmap || overflowed)
  {
    CompressBlockmapWorker(level, BlockCompare, stats, NullBlockSize_Long);
  }

  if (config.verbose)
  {
    PrintLine(LOG_DEBUG, "[%s] Last ptr = %zu  duplicates = %zu", __func__, stats.current_offset, stats.duplicate_count);
  }

  level.block_compression = (static_cast<double>(stats.original_size) - static_cast<double>(stats.new_size))
                            / static_cast<double>(stats.original_size);

  // there's a tiny chance of new_size > orig_size
  level.block_compression = std::max(0.0, level.block_compression);
}

// compute size of final BLOCKMAP lump.
template <typename OffsetType = uint16_t, typename LineType = uint16_t> static size_t CalcBlockmapSize(level_t &level)
{
  // Header + null block
  size_t size = sizeof(raw_blockmap_header_t);

  // the pointers (offsets to the line lists)
  size += level.block_count * sizeof(OffsetType);

  // add size of each block
  for (size_t i = 0; i < level.block_count; i++)
  {
    size_t blk_num = level.block_duplicates[i];

    // ignore duplicate or empty blocks
    if (blk_num == NO_INDEX)
    {
      continue;
    }

    const auto &blk = level.block_lines[blk_num];

    size += ((blk.lines.size() + EXTRA_LINES) * sizeof(LineType));
  }

  return size;
}

template <typename OffsetType = uint16_t, typename LineType = uint16_t> static void WriteBlockmap(level_t &level)
{
  size_t max_size = CalcBlockmapSize<OffsetType, LineType>(level);
  Lump_c *lump = CreateLevelLump(level, "BLOCKMAP", max_size);

  // fill in header
  raw_blockmap_header_t header;

  header.x_origin = GetLittleEndian(level.block_x);
  header.y_origin = GetLittleEndian(level.block_y);
  header.x_blocks = GetLittleEndian(IndexToShort(level.block_w));
  header.y_blocks = GetLittleEndian(IndexToShort(level.block_h));

  lump->Write(&header, sizeof(header));

  // handle pointers
  for (size_t i = 0; i < level.block_count; i++)
  {
    OffsetType ptr = GetLittleEndian(static_cast<OffsetType>(level.block_offsets[i]));

    if (ptr == 0)
    {
      PrintLine(LOG_ERROR, "ERROR: WriteBlockmap: offset %zu not set.", i);
    }

    lump->Write(&ptr, sizeof(OffsetType));
  }

  // add the null block which *all* empty blocks will use
  lump->Write(&m_zero, sizeof(LineType));
  lump->Write(&m_neg1, sizeof(LineType));

  // handle each block list
  for (size_t i = 0; i < level.block_count; i++)
  {
    size_t blk_num = level.block_duplicates[i];

    // ignore duplicate or empty blocks
    if (blk_num == NO_INDEX)
    {
      continue;
    }

    const auto &blk = level.block_lines[blk_num];

    lump->Write(&m_zero, sizeof(LineType));
    for (size_t line : blk.lines)
    {
      LineType le_line = GetLittleEndian(static_cast<LineType>(line));
      lump->Write(&le_line, sizeof(LineType));
    }
    lump->Write(&m_neg1, sizeof(LineType));
  }

  lump->Finish();
}

void PutBlockmap(level_t &level)
{
  auto mark = Benchmarker(__func__);

  // just create an empty blockmap lump
  if (level.linedefs.size() == 0)
  {
    CreateLevelLump(level, "BLOCKMAP")->Finish();
    return;
  }

  // Sorting for compression
  auto BlockCompare = [&level](const size_t &blk_num1, const size_t &blk_num2) -> int
  {
    const auto &A = level.block_lines[blk_num1];
    const auto &B = level.block_lines[blk_num2];

    if (A.lines.size() != B.lines.size()) return A.lines.size() < B.lines.size() ? -1 : 1;
    if (A.hash != B.hash) return A.hash < B.hash ? -1 : 1;
    if (A.lines == B.lines) return 0;

    return A.lines < B.lines ? -1 : 1;
  };

  CreateBlockmap(level);
  CompressBlockmap(level, BlockCompare);

  bmap_format_t bmap_format = std::max(config.bmap_format, level.bmap_format);

  switch (bmap_format)
  {
  case BMAP_DoomBlockmap:
    WriteBlockmap<uint16_t, uint16_t>(level);
    break;
  case BMAP_XBM1:
    WriteBlockmap<uint32_t, uint16_t>(level);
    break;
  case BMAP_XBM2:
    WriteBlockmap<uint32_t, uint32_t>(level);
    break;
  default:
    // how did we get here
    // leave an empty blockmap lump
    CreateLevelLump(level, "BLOCKMAP")->Finish();
    PrintLine(LOG_NORMAL, "WARNING: Blockmap overflowed (lump will be empty)");
    config.total_warnings++;
    break;
  }

  if (config.verbose)
  {
    PrintLine(LOG_NORMAL, "Blockmap size: %zux%zu (compression: %d%%)", level.block_w, level.block_h,
              static_cast<int32_t>(level.block_compression * 100));
  }
  FreeBlockmap(level);
}
