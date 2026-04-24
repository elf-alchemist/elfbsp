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

static constexpr size_t HeaderIndexSize = 4;
static constexpr size_t NullBlockIndexSize = 2;

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
  // Intentionally leave `lines_block_reject` untouched
  // As we need it for reject matrix speed ups

  for (size_t i = 0; i < level.block_count; i++)
  {
    level.block_lines[i].lines.clear();
    // level.block_lines[i].lines_block_reject.clear();
  }

  // level.block_lines.clear();
  level.block_indexes.clear();
  level.block_duplicates.clear();
}

static void BlockAdd(level_t &level, size_t blk_num, size_t line_index, bool reject)
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
  if (reject) blk.lines_block_reject.push_back(line_index);
}

static void BlockAddLine(level_t &level, const linedef_t *L)
{
  auto x1 = FloatToShort(L->start->x);
  auto y1 = FloatToShort(L->start->y);
  auto x2 = FloatToShort(L->end->x);
  auto y2 = FloatToShort(L->end->y);

  size_t line_index = L->index;

  bool reject = HAS_NONE(L->effects, FX_RejectPortal) // isn't already a portal
                || HAS_BIT(L->effects, FX_NoReject);  // is a manually blocked portal

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
      BlockAdd(level, blk_num, line_index, reject);
    }
    return;
  }

  // handle simple case #2: completely vertical
  if (bx1 == bx2)
  {
    for (size_t by = by1; by <= by2; by++)
    {
      size_t blk_num = by * level.block_w + bx1;
      BlockAdd(level, blk_num, line_index, reject);
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
        BlockAdd(level, blk_num, line_index, reject);
      }
    }
  }
}

// initial phase: create internal blockmap containing the index of
// all lines in each block.
static void CreateBlockmap(level_t &level)
{
  level.block_lines.assign(level.block_count, blocklist_t{.hash = 0x1234123412341234, .lines = {}, .lines_block_reject = {}});

  for (size_t i = 0; i < level.linedefs.size(); i++)
  {
    const linedef_t *L = level.linedefs[i];

    if (HAS_BIT(L->effects, FX_NoBlockmap | FX_ZeroLength))
    {
      continue;
    }

    BlockAddLine(level, L);
  }

  // Force extended format
  if (level.bmap_format < BMAP_XBM1 && level.linedefs.size() > LIMIT_LINE)
  {
    PrintLine(LOG_NORMAL, "WARNING: Blockmap overflow. Forcing XBM1 format.");
    config.total_warnings++;
    RaiseValue(level.bmap_format, BMAP_XBM1);
  }
}

// -AJA- second phase: compress the blockmap.  We do this by sorting
//       the blocks, which is a typical way to detect duplicates in
//       a large list.  This also detects BLOCKMAP overflow.
static void CompressBlockmap(level_t &level)
{
  size_t current_index = 0;
  size_t original_size = 0;
  size_t new_size = 0;
  size_t duplicate_count = 0;

  level.block_indexes.reserve(level.block_count);
  level.block_duplicates.reserve(level.block_count);

  // sort duplicate-detecting array.  After the sort, all duplicates
  // will be next to each other.  The duplicate array gives the order
  // of the blocklists in the BLOCKMAP lump.
  for (size_t i = 0; i < level.block_count; i++)
  {
    level.block_duplicates[i] = i;
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
  std::sort(level.block_duplicates.begin(), level.block_duplicates.end(), BlockCompare);

  current_index = level.block_count + HeaderIndexSize + NullBlockIndexSize;
  original_size = level.block_count + HeaderIndexSize;
  new_size = current_index;
  duplicate_count = 0;

  for (size_t i = 0; i < level.block_count; i++)
  {
    size_t blk_num = level.block_duplicates[i];

    // empty block ?
    if (level.block_lines[blk_num].lines.empty())
    {
      level.block_indexes[blk_num] = level.block_count + HeaderIndexSize;
      level.block_duplicates[i] = NO_INDEX;

      original_size += EXTRA_LINES;
      continue;
    }

    size_t count = level.block_lines[blk_num].lines.size() + EXTRA_LINES;

    // duplicate ?  Only the very last one of a sequence of duplicates
    // will update the current index value.
    if (i + 1 < level.block_count && BlockCompare(level.block_duplicates[i], level.block_duplicates[i + 1]) == 0)
    {
      level.block_indexes[blk_num] = current_index;
      level.block_duplicates[i] = NO_INDEX;

      // free the memory of the duplicated block
      level.block_lines[blk_num].lines.clear();
      duplicate_count++;
      original_size += count;
      continue;
    }

    // OK, this block is either the last of a series of duplicates, or
    // just a singleton.
    level.block_indexes[blk_num] = current_index;
    current_index += count;
    original_size += count;
    new_size += count;
  }

  if (level.bmap_format < BMAP_XBM1 && current_index > LIMIT_BMAP_INDEX)
  {
    // Overflowed?
    PrintLine(LOG_NORMAL, "WARNING: Blockmap overflow. Forcing XBM1 format.");
    config.total_warnings++;
    RaiseValue(level.bmap_format, BMAP_XBM1);
  }

  if (config.verbose)
  {
    PrintLine(LOG_DEBUG, "[%s] Last ptr = %zu  duplicates = %zu", __func__, current_index, duplicate_count);
  }

  level.block_compression =
      (static_cast<double>(original_size) - static_cast<double>(new_size)) / static_cast<double>(original_size);

  // there's a tiny chance of new_size > orig_size
  level.block_compression = std::max(0.0, level.block_compression);
}

// compute size of final BLOCKMAP lump.
static size_t CalcBlockmapSize(level_t &level, std::string prefix = "", size_t PrefixSize = 0,
                               size_t NumSize = sizeof(uint16_t), size_t RawHeaderSize = sizeof(raw_blockmap_header_t))
{
  size_t size = PrefixSize;

  size += RawHeaderSize;

  // null block
  size += NumSize * 2;

  // the pointers (indexes to the line lists)
  size += level.block_count * NumSize;

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
    size += (blk.lines.size() + EXTRA_LINES) * NumSize;
  }

  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(LOG_DEBUG, "[%s] Lump prefix header \'%s\', num type size of %zu, total size of %zu", __func__, prefix.c_str(),
              NumSize, size);
  }

  return size;
}

// final phase: write it out in the correct format
template <typename NumType = uint16_t>
static void WriteBlockmap(level_t &level, std::string prefix = "")
{
  static constexpr size_t PrefixHeaderSize = 8; // "XBM1\0\0\0\0"
  size_t PrefixSize = (!prefix.empty() ? PrefixHeaderSize : 0);
  size_t NumSize = sizeof(NumType);
  size_t RawHeaderSize = (level.bmap_format == BMAP_XBM1 ? sizeof(raw_blockmap_xbm1_header_t) : sizeof(raw_blockmap_header_t));

  size_t max_size = CalcBlockmapSize(level, prefix, PrefixSize, NumSize, RawHeaderSize);
  Lump_c *lump = CreateLevelLump(level, "BLOCKMAP", max_size);

  // fill in header
  if (level.bmap_format == BMAP_XBM1)
  {
    lump->Write(prefix.data(), PrefixHeaderSize);

    raw_blockmap_xbm1_header_t xbm1_header;
    xbm1_header.x_origin = GetLittleEndian(IntToFixed(level.block_x));
    xbm1_header.y_origin = GetLittleEndian(IntToFixed(level.block_y));
    xbm1_header.x_blocks = GetLittleEndian(IndexToInt(level.block_w));
    xbm1_header.y_blocks = GetLittleEndian(IndexToInt(level.block_h));
    lump->Write(&xbm1_header, sizeof(xbm1_header));
  }
  else
  {
    raw_blockmap_header_t header;
    header.x_origin = GetLittleEndian(level.block_x);
    header.y_origin = GetLittleEndian(level.block_y);
    header.x_blocks = GetLittleEndian(IndexToShort(level.block_w));
    header.y_blocks = GetLittleEndian(IndexToShort(level.block_h));
    lump->Write(&header, sizeof(header));
  }

  // handle pointers
  for (size_t i = 0; i < level.block_count; i++)
  {
    NumType ptr = GetLittleEndian(static_cast<NumType>(level.block_indexes[i]));

    if (ptr == 0)
    {
      PrintLine(LOG_ERROR, "ERROR: WriteBlockmap: offset %zu not set.", i);
    }

    lump->Write(&ptr, NumSize);
  }

  // add the null block which *all* empty blocks will use
  lump->Write(&m_zero, NumSize);
  lump->Write(&m_neg1, NumSize);

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

    lump->Write(&m_zero, NumSize);
    for (size_t line : blk.lines)
    {
      NumType le_line = GetLittleEndian(static_cast<NumType>(line));
      lump->Write(&le_line, NumSize);
    }
    lump->Write(&m_neg1, NumSize);
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

  RaiseValue(level.bmap_format, config.bmap_format);

  CreateBlockmap(level);
  CompressBlockmap(level);

  switch (level.bmap_format)
  {
  case BMAP_DoomBlockmap:
    WriteBlockmap(level);
    break;
  case BMAP_XBM1:
    WriteBlockmap<uint32_t>(level, BMAP_MAGIC_XBM1);
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
