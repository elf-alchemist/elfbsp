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

#include "local.hpp"

/* ----- create blockmap ------------------------------------ */

static constexpr uint32_t BK_NUM = 0;
static constexpr uint32_t BK_MAX = 1;
static constexpr uint32_t BK_XOR = 2;
static constexpr uint32_t BK_FIRST = 3;
static constexpr uint32_t BK_QUANTUM = 32;

static constexpr uint32_t DUMMY_DUP = 0xFFFF;

static void BlockAdd(level_t &level, size_t blk_num, size_t line_index)
{
  uint16_t *cur = level.block_lines[blk_num];

  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(LOG_DEBUG, "[%s] Block %zu has line %zu", __func__, blk_num, line_index);
  }

  if (blk_num >= level.block_count)
  {
    PrintLine(LOG_ERROR, "ERROR: BlockAdd: bad block number %zu", blk_num);
  }

  if (!cur)
  {
    // create empty block
    level.block_lines[blk_num] = cur = UtilCalloc<uint16_t>(BK_QUANTUM * sizeof(uint16_t));
    cur[BK_NUM] = 0;
    cur[BK_MAX] = BK_QUANTUM;
    cur[BK_XOR] = 0x1234;
  }

  if (BK_FIRST + cur[BK_NUM] == cur[BK_MAX])
  {
    // no more room, so allocate some more...
    cur[BK_MAX] += BK_QUANTUM;

    level.block_lines[blk_num] = cur = UtilRealloc(cur, cur[BK_MAX] * sizeof(uint16_t));
  }

  // compute new checksum
  cur[BK_XOR] = static_cast<uint16_t>(cur[BK_XOR] << 4) | (cur[BK_XOR] >> 12);
  cur[BK_XOR] ^= static_cast<uint16_t>(line_index & 0xFFFF);

  cur[BK_FIRST + cur[BK_NUM]] = GetLittleEndian(static_cast<uint16_t>(line_index));
  cur[BK_NUM]++;
}

static void BlockAddLine(level_t &level, const linedef_t *L)
{
  int32_t x1 = static_cast<int32_t>(L->start->x);
  int32_t y1 = static_cast<int32_t>(L->start->y);
  int32_t x2 = static_cast<int32_t>(L->end->x);
  int32_t y2 = static_cast<int32_t>(L->end->y);

  size_t line_index = L->index;

  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(LOG_DEBUG, "[%s] %zu (%d,%d) -> (%d,%d)", __func__, line_index, x1, y1, x2, y2);
  }

  int32_t bx1_temp = (std::min(x1, x2) - level.block_x) / 128;
  int32_t by1_temp = (std::min(y1, y2) - level.block_y) / 128;
  int32_t bx2_temp = (std::max(x1, x2) - level.block_x) / 128;
  int32_t by2_temp = (std::max(y1, y2) - level.block_y) / 128;

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
      size_t blk_num = static_cast<size_t>(by1 * level.block_w + bx);
      BlockAdd(level, blk_num, line_index);
    }
    return;
  }

  // handle simple case #2: completely vertical
  if (bx1 == bx2)
  {
    for (size_t by = by1; by <= by2; by++)
    {
      size_t blk_num = static_cast<size_t>(by * level.block_w + bx1);
      BlockAdd(level, blk_num, line_index);
    }
    return;
  }

  // handle the rest (diagonals)

  for (size_t by = by1; by <= by2; by++)
  {
    for (size_t bx = bx1; bx <= bx2; bx++)
    {
      size_t blk_num = static_cast<size_t>(bx + by * level.block_w);

      int32_t minx = level.block_x + 128 * static_cast<int32_t>(bx);
      int32_t miny = level.block_y + 128 * static_cast<int32_t>(by);
      int32_t maxx = minx + 127;
      int32_t maxy = miny + 127;

      if (CheckLinedefInsideBox(minx, miny, maxx, maxy, x1, y1, x2, y2))
      {
        BlockAdd(level, blk_num, line_index);
      }
    }
  }
}

static void CreateBlockmap(level_t &level)
{
  level.block_lines = UtilCalloc<uint16_t *>(level.block_count * sizeof(uint16_t *));

  for (size_t i = 0; i < level.linedefs.size(); i++)
  {
    const linedef_t *L = level.linedefs[i];

    if (HAS_BIT(L->effects, FX_NoBlockmap | FX_ZeroLength))
    {
      continue;
    }

    BlockAddLine(level, L);
  }
}

static void CompressBlockmap(level_t &level)
{
  size_t cur_offset;
  size_t dup_count = 0;

  size_t orig_size, new_size;

  level.block_ptrs.reserve(level.block_count);
  level.block_dups.reserve(level.block_count);

  // sort duplicate-detecting array.  After the sort, all duplicates
  // will be next to each other.  The duplicate array gives the order
  // of the blocklists in the BLOCKMAP lump.
  for (size_t i = 0; i < level.block_count; i++)
  {
    level.block_dups[i] = static_cast<uint16_t>(i);
  }

  auto BlockCompare = [&level](const uint16_t &blk_num1, const uint16_t &blk_num2) -> int
  {
    const uint16_t *A = level.block_lines[blk_num1];
    const uint16_t *B = level.block_lines[blk_num2];

    if (A == B) return 0;
    if (A == nullptr) return -1;
    if (B == nullptr) return +1;
    if (A[BK_NUM] != B[BK_NUM]) return A[BK_NUM] - B[BK_NUM];
    if (A[BK_XOR] != B[BK_XOR]) return A[BK_XOR] - B[BK_XOR];

    return memcmp(A + BK_FIRST, B + BK_FIRST, A[BK_NUM] * sizeof(uint16_t));
  };
  std::sort(level.block_dups.begin(), level.block_dups.end(), BlockCompare);

  // scan duplicate array and build up offset array
  cur_offset = 4 + level.block_count + 2;
  orig_size = 4 + level.block_count;
  new_size = cur_offset;

  for (size_t i = 0; i < level.block_count; i++)
  {
    size_t blk_num = level.block_dups[i];

    // empty block ?
    if (level.block_lines[blk_num] == nullptr)
    {
      level.block_ptrs[blk_num] = static_cast<uint16_t>(4 + level.block_count);
      level.block_dups[i] = DUMMY_DUP;

      orig_size += 2;
      continue;
    }

    size_t count = 2 + level.block_lines[blk_num][BK_NUM];

    // duplicate ?  Only the very last one of a sequence of duplicates
    // will update the current offset value.
    if (i + 1 < level.block_count && BlockCompare(level.block_dups[i], level.block_dups[i + 1]) == 0)
    {
      level.block_ptrs[blk_num] = static_cast<uint16_t>(cur_offset);
      level.block_dups[i] = DUMMY_DUP;

      // free the memory of the duplicated block
      UtilFree(level.block_lines[blk_num]);
      level.block_lines[blk_num] = nullptr;

      if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
      {
        dup_count++;
      }

      orig_size += count;
      continue;
    }

    // OK, this block is either the last of a series of duplicates, or
    // just a singleton.
    level.block_ptrs[blk_num] = static_cast<uint16_t>(cur_offset);
    cur_offset += count;
    orig_size += count;
    new_size += count;
  }

  if (cur_offset > 65535)
  {
    level.block_overflowed = true;
    return;
  }

  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(LOG_DEBUG, "[%s] Last ptr = %zu  duplicates = %zu", __func__, cur_offset, dup_count);
  }

  level.block_compression =
      (static_cast<int32_t>(orig_size) - static_cast<int32_t>(new_size)) * 100 / static_cast<int32_t>(orig_size);

  // there's a tiny chance of new_size > orig_size
  if (level.block_compression < 0)
  {
    level.block_compression = 0;
  }
}

static size_t CalcBlockmapSize(level_t &level)
{
  // compute size of final BLOCKMAP lump.
  // it does not need to be exact, but it *does* need to be bigger
  // (or equal) to the actual size of the lump.

  // header + null_block + a bit extra
  size_t size = 20;

  // the pointers (offsets to the line lists)
  size = size + level.block_count * 2;

  // add size of each block
  for (size_t i = 0; i < level.block_count; i++)
  {
    size_t blk_num = level.block_dups[i];

    // ignore duplicate or empty blocks
    if (blk_num == DUMMY_DUP)
    {
      continue;
    }

    uint16_t *blk = level.block_lines[blk_num];
    SYS_ASSERT(blk);

    size += static_cast<size_t>(((blk[BK_NUM]) + 1 + 1) * 2);
  }

  return size;
}

static void WriteBlockmap(level_t &level)
{
  size_t max_size = CalcBlockmapSize(level);
  Lump_c *lump = CreateLevelLump(level, "BLOCKMAP", max_size);

  uint16_t null_block[2] = {0x0000, 0xFFFF};
  uint16_t m_zero = 0x0000;
  uint16_t m_neg1 = 0xFFFF;

  // fill in header
  raw_blockmap_header_t header;

  header.x_origin = GetLittleEndian(static_cast<int16_t>(level.block_x));
  header.y_origin = GetLittleEndian(static_cast<int16_t>(level.block_y));
  header.x_blocks = GetLittleEndian(static_cast<int16_t>(level.block_w));
  header.y_blocks = GetLittleEndian(static_cast<int16_t>(level.block_h));

  lump->Write(&header, sizeof(header));

  // handle pointers
  for (size_t i = 0; i < level.block_count; i++)
  {
    uint16_t ptr = GetLittleEndian(level.block_ptrs[i]);

    if (ptr == 0)
    {
      PrintLine(LOG_ERROR, "ERROR: WriteBlockmap: offset %zu not set.", i);
    }

    lump->Write(&ptr, sizeof(uint16_t));
  }

  // add the null block which *all* empty blocks will use
  lump->Write(null_block, sizeof(null_block));

  // handle each block list
  for (size_t i = 0; i < level.block_count; i++)
  {
    int blk_num = level.block_dups[i];

    // ignore duplicate or empty blocks
    if (blk_num == DUMMY_DUP)
    {
      continue;
    }

    uint16_t *blk = level.block_lines[blk_num];
    SYS_ASSERT(blk);

    lump->Write(&m_zero, sizeof(uint16_t));
    lump->Write(blk + BK_FIRST, blk[BK_NUM] * sizeof(uint16_t));
    lump->Write(&m_neg1, sizeof(uint16_t));
  }

  lump->Finish();
}

static void FreeBlockmap(level_t &level)
{
  for (size_t i = 0; i < level.block_count; i++)
  {
    if (level.block_lines[i])
    {
      UtilFree(level.block_lines[i]);
    }
  }

  UtilFree(level.block_lines);
  level.block_ptrs.clear();
  level.block_dups.clear();
}

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

    int16_t lx = static_cast<int16_t>(floor(std::min(x1, x2)));
    int16_t ly = static_cast<int16_t>(floor(std::min(y1, y2)));
    int16_t hx = static_cast<int16_t>(ceil(std::max(x1, x2)));
    int16_t hy = static_cast<int16_t>(ceil(std::max(y1, y2)));

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
    block_mid_x = static_cast<int16_t>(floor(mid_x / static_cast<double>(level.linedefs.size())));
    block_mid_y = static_cast<int16_t>(floor(mid_y / static_cast<double>(level.linedefs.size())));
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

void PutBlockmap(level_t &level)
{
  auto mark = Benchmarker(__func__);
  if (level.linedefs.size() == 0)
  {
    // just create an empty blockmap lump
    CreateLevelLump(level, "BLOCKMAP")->Finish();
    return;
  }

  // initial phase: create internal blockmap containing the index of
  // all lines in each block.
  CreateBlockmap(level);

  // -AJA- second phase: compress the blockmap.  We do this by sorting
  //       the blocks, which is a typical way to detect duplicates in
  //       a large list.  This also detects BLOCKMAP overflow.
  CompressBlockmap(level);

  // final phase: write it out in the correct format
  if (level.block_overflowed)
  {
    // leave an empty blockmap lump
    CreateLevelLump(level, "BLOCKMAP")->Finish();
    PrintLine(LOG_NORMAL, "WARNING: Blockmap overflowed (lump will be empty)");
    config.total_warnings++;
  }
  else
  {
    WriteBlockmap(level);
    if (config.verbose)
    {
      PrintLine(LOG_NORMAL, "Blockmap size: %zux%zu (compression: %d%%)", level.block_w, level.block_h,
                level.block_compression);
    }
  }

  FreeBlockmap(level);
}
