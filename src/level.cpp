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

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

static constexpr std::uint32_t DUMMY_DUP = 0xFFFF;

Wad_file *cur_wad;

static int block_x, block_y;
static size_t block_w, block_h;
static size_t block_count;

static int block_mid_x = 0;
static int block_mid_y = 0;

static uint16_t **block_lines;

static uint16_t *block_ptrs;
static uint16_t *block_dups;

static int32_t block_compression;
static bool block_overflowed;

int CheckLinedefInsideBox(int xmin, int ymin, int xmax, int ymax, int x1, int y1, int x2, int y2)
{
  int count = 2;
  int tmp;

  for (;;)
  {
    if (y1 > ymax)
    {
      if (y2 > ymax)
      {
        return false;
      }

      x1 = x1
           + static_cast<int32_t>(static_cast<double>(x2 - x1) * static_cast<double>(ymax - y1) / static_cast<double>(y2 - y1));
      y1 = ymax;

      count = 2;
      continue;
    }

    if (y1 < ymin)
    {
      if (y2 < ymin)
      {
        return false;
      }

      x1 = x1
           + static_cast<int32_t>(static_cast<double>(x2 - x1) * static_cast<double>(ymin - y1) / static_cast<double>(y2 - y1));
      y1 = ymin;

      count = 2;
      continue;
    }

    if (x1 > xmax)
    {
      if (x2 > xmax)
      {
        return false;
      }

      y1 = y1
           + static_cast<int32_t>(static_cast<double>(y2 - y1) * static_cast<double>(xmax - x1) / static_cast<double>(x2 - x1));
      x1 = xmax;

      count = 2;
      continue;
    }

    if (x1 < xmin)
    {
      if (x2 < xmin)
      {
        return false;
      }

      y1 = y1
           + static_cast<int32_t>(static_cast<double>(y2 - y1) * static_cast<double>(xmin - x1) / static_cast<double>(x2 - x1));
      x1 = xmin;

      count = 2;
      continue;
    }

    count--;

    if (count == 0)
    {
      break;
    }

    // swap end points
    tmp = x1;
    x1 = x2;
    x2 = tmp;
    tmp = y1;
    y1 = y2;
    y2 = tmp;
  }

  // linedef touches block
  return true;
}

/* ----- create blockmap ------------------------------------ */

static constexpr uint32_t BK_NUM = 0;
static constexpr uint32_t BK_MAX = 1;
static constexpr uint32_t BK_XOR = 2;
static constexpr uint32_t BK_FIRST = 3;
static constexpr uint32_t BK_QUANTUM = 32;

static void BlockAdd(size_t blk_num, size_t line_index)
{
  uint16_t *cur = block_lines[blk_num];

  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(LOG_DEBUG, "[%s] Block %zu has line %zu", __func__, blk_num, line_index);
  }

  if (blk_num >= block_count)
  {
    PrintLine(LOG_ERROR, "ERROR: BlockAdd: bad block number %zu", blk_num);
  }

  if (!cur)
  {
    // create empty block
    block_lines[blk_num] = cur = UtilCalloc<uint16_t>(BK_QUANTUM * sizeof(uint16_t));
    cur[BK_NUM] = 0;
    cur[BK_MAX] = BK_QUANTUM;
    cur[BK_XOR] = 0x1234;
  }

  if (BK_FIRST + cur[BK_NUM] == cur[BK_MAX])
  {
    // no more room, so allocate some more...
    cur[BK_MAX] += BK_QUANTUM;

    block_lines[blk_num] = cur = UtilRealloc(cur, cur[BK_MAX] * sizeof(uint16_t));
  }

  // compute new checksum
  cur[BK_XOR] = static_cast<uint16_t>(cur[BK_XOR] << 4) | (cur[BK_XOR] >> 12);
  cur[BK_XOR] ^= static_cast<uint16_t>(line_index & 0xFFFF);

  cur[BK_FIRST + cur[BK_NUM]] = GetLittleEndian(static_cast<uint16_t>(line_index));
  cur[BK_NUM]++;
}

static void BlockAddLine(const linedef_t *L)
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

  int32_t bx1_temp = (std::min(x1, x2) - block_x) / 128;
  int32_t by1_temp = (std::min(y1, y2) - block_y) / 128;
  int32_t bx2_temp = (std::max(x1, x2) - block_x) / 128;
  int32_t by2_temp = (std::max(y1, y2) - block_y) / 128;

  // handle truncated blockmaps
  size_t bx1 = static_cast<size_t>(std::max(bx1_temp, 0));
  size_t by1 = static_cast<size_t>(std::max(by1_temp, 0));
  size_t bx2 = static_cast<size_t>(std::min(bx2_temp, static_cast<int32_t>(block_w - 1)));
  size_t by2 = static_cast<size_t>(std::min(by2_temp, static_cast<int32_t>(block_h - 1)));

  if (bx2 < bx1 || by2 < by1)
  {
    return;
  }

  // handle simple case #1: completely horizontal
  if (by1 == by2)
  {
    for (size_t bx = bx1; bx <= bx2; bx++)
    {
      size_t blk_num = static_cast<size_t>(by1 * block_w + bx);
      BlockAdd(blk_num, line_index);
    }
    return;
  }

  // handle simple case #2: completely vertical
  if (bx1 == bx2)
  {
    for (size_t by = by1; by <= by2; by++)
    {
      size_t blk_num = static_cast<size_t>(by * block_w + bx1);
      BlockAdd(blk_num, line_index);
    }
    return;
  }

  // handle the rest (diagonals)

  for (size_t by = by1; by <= by2; by++)
  {
    for (size_t bx = bx1; bx <= bx2; bx++)
    {
      size_t blk_num = static_cast<size_t>(bx + by * block_w);

      int32_t minx = block_x + 128 * static_cast<int32_t>(bx);
      int32_t miny = block_y + 128 * static_cast<int32_t>(by);
      int32_t maxx = minx + 127;
      int32_t maxy = miny + 127;

      if (CheckLinedefInsideBox(minx, miny, maxx, maxy, x1, y1, x2, y2))
      {
        BlockAdd(blk_num, line_index);
      }
    }
  }
}

static void CreateBlockmap(level_t &level)
{
  block_lines = UtilCalloc<uint16_t *>(block_count * sizeof(uint16_t *));

  for (size_t i = 0; i < level.linedefs.size(); i++)
  {
    const linedef_t *L = level.linedefs[i];

    if (HAS_BIT(L->effects, FX_NoBlockmap | FX_ZeroLength))
    {
      continue;
    }

    BlockAddLine(L);
  }
}

static int BlockCompare(const void *p1, const void *p2)
{
  int blk_num1 = (static_cast<const uint16_t *>(p1))[0];
  int blk_num2 = (static_cast<const uint16_t *>(p2))[0];

  const uint16_t *A = block_lines[blk_num1];
  const uint16_t *B = block_lines[blk_num2];

  if (A == B)
  {
    return 0;
  }
  if (A == nullptr)
  {
    return -1;
  }
  if (B == nullptr)
  {
    return +1;
  }

  if (A[BK_NUM] != B[BK_NUM])
  {
    return A[BK_NUM] - B[BK_NUM];
  }

  if (A[BK_XOR] != B[BK_XOR])
  {
    return A[BK_XOR] - B[BK_XOR];
  }

  return memcmp(A + BK_FIRST, B + BK_FIRST, A[BK_NUM] * sizeof(uint16_t));
}

static void CompressBlockmap(level_t &level)
{
  size_t cur_offset;
  size_t dup_count = 0;

  size_t orig_size, new_size;

  block_ptrs = UtilCalloc<uint16_t>(block_count * sizeof(uint16_t));
  block_dups = UtilCalloc<uint16_t>(block_count * sizeof(uint16_t));

  // sort duplicate-detecting array.  After the sort, all duplicates
  // will be next to each other.  The duplicate array gives the order
  // of the blocklists in the BLOCKMAP lump.
  for (size_t i = 0; i < block_count; i++)
  {
    block_dups[i] = static_cast<uint16_t>(i);
  }

  qsort(block_dups, block_count, sizeof(uint16_t), BlockCompare);

  // scan duplicate array and build up offset array
  cur_offset = 4 + block_count + 2;
  orig_size = 4 + block_count;
  new_size = cur_offset;

  for (size_t i = 0; i < block_count; i++)
  {
    size_t blk_num = block_dups[i];

    // empty block ?
    if (block_lines[blk_num] == nullptr)
    {
      block_ptrs[blk_num] = static_cast<uint16_t>(4 + block_count);
      block_dups[i] = DUMMY_DUP;

      orig_size += 2;
      continue;
    }

    size_t count = 2 + block_lines[blk_num][BK_NUM];

    // duplicate ?  Only the very last one of a sequence of duplicates
    // will update the current offset value.
    if (i + 1 < block_count && BlockCompare(block_dups + i, block_dups + i + 1) == 0)
    {
      block_ptrs[blk_num] = static_cast<uint16_t>(cur_offset);
      block_dups[i] = DUMMY_DUP;

      // free the memory of the duplicated block
      UtilFree(block_lines[blk_num]);
      block_lines[blk_num] = nullptr;

      if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
      {
        dup_count++;
      }

      orig_size += count;
      continue;
    }

    // OK, this block is either the last of a series of duplicates, or
    // just a singleton.
    block_ptrs[blk_num] = static_cast<uint16_t>(cur_offset);
    cur_offset += count;
    orig_size += count;
    new_size += count;
  }

  if (cur_offset > 65535)
  {
    block_overflowed = true;
    return;
  }

  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(LOG_DEBUG, "[%s] Last ptr = %zu  duplicates = %zu", __func__, cur_offset, dup_count);
  }

  block_compression = static_cast<int32_t>((static_cast<int32_t>(orig_size) - static_cast<int32_t>(new_size)) * 100
                                           / static_cast<int32_t>(orig_size));

  // there's a tiny chance of new_size > orig_size
  if (block_compression < 0)
  {
    block_compression = 0;
  }
}

static size_t CalcBlockmapSize(void)
{
  // compute size of final BLOCKMAP lump.
  // it does not need to be exact, but it *does* need to be bigger
  // (or equal) to the actual size of the lump.

  // header + null_block + a bit extra
  size_t size = 20;

  // the pointers (offsets to the line lists)
  size = size + block_count * 2;

  // add size of each block
  for (size_t i = 0; i < block_count; i++)
  {
    size_t blk_num = block_dups[i];

    // ignore duplicate or empty blocks
    if (blk_num == DUMMY_DUP)
    {
      continue;
    }

    uint16_t *blk = block_lines[blk_num];
    SYS_ASSERT(blk);

    size += static_cast<size_t>(((blk[BK_NUM]) + 1 + 1) * 2);
  }

  return size;
}

static void WriteBlockmap(level_t &level)
{
  size_t max_size = CalcBlockmapSize();
  Lump_c *lump = CreateLevelLump(level, "BLOCKMAP", max_size);

  uint16_t null_block[2] = {0x0000, 0xFFFF};
  uint16_t m_zero = 0x0000;
  uint16_t m_neg1 = 0xFFFF;

  // fill in header
  raw_blockmap_header_t header;

  header.x_origin = GetLittleEndian(static_cast<int16_t>(block_x));
  header.y_origin = GetLittleEndian(static_cast<int16_t>(block_y));
  header.x_blocks = GetLittleEndian(static_cast<int16_t>(block_w));
  header.y_blocks = GetLittleEndian(static_cast<int16_t>(block_h));

  lump->Write(&header, sizeof(header));

  // handle pointers
  for (size_t i = 0; i < block_count; i++)
  {
    uint16_t ptr = GetLittleEndian(block_ptrs[i]);

    if (ptr == 0)
    {
      PrintLine(LOG_ERROR, "ERROR: WriteBlockmap: offset %zu not set.", i);
    }

    lump->Write(&ptr, sizeof(uint16_t));
  }

  // add the null block which *all* empty blocks will use
  lump->Write(null_block, sizeof(null_block));

  // handle each block list
  for (size_t i = 0; i < block_count; i++)
  {
    int blk_num = block_dups[i];

    // ignore duplicate or empty blocks
    if (blk_num == DUMMY_DUP)
    {
      continue;
    }

    uint16_t *blk = block_lines[blk_num];
    SYS_ASSERT(blk);

    lump->Write(&m_zero, sizeof(uint16_t));
    lump->Write(blk + BK_FIRST, blk[BK_NUM] * sizeof(uint16_t));
    lump->Write(&m_neg1, sizeof(uint16_t));
  }

  lump->Finish();
}

static void FreeBlockmap(void)
{
  for (size_t i = 0; i < block_count; i++)
  {
    if (block_lines[i])
    {
      UtilFree(block_lines[i]);
    }
  }

  UtilFree(block_lines);
  UtilFree(block_ptrs);
  UtilFree(block_dups);
}

static void FindBlockmapLimits(level_t &level, bbox_t *bbox)
{
  double mid_x = 0;
  double mid_y = 0;

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

    int32_t lx = static_cast<int32_t>(floor(std::min(x1, x2)));
    int32_t ly = static_cast<int32_t>(floor(std::min(y1, y2)));
    int32_t hx = static_cast<int32_t>(ceil(std::max(x1, x2)));
    int32_t hy = static_cast<int32_t>(ceil(std::max(y1, y2)));

    if (lx < bbox->minx)
    {
      bbox->minx = lx;
    }
    if (ly < bbox->miny)
    {
      bbox->miny = ly;
    }
    if (hx > bbox->maxx)
    {
      bbox->maxx = hx;
    }
    if (hy > bbox->maxy)
    {
      bbox->maxy = hy;
    }

    // compute middle of cluster
    mid_x += (lx + hx) >> 1;
    mid_y += (ly + hy) >> 1;
  }

  if (level.linedefs.size() > 0)
  {
    block_mid_x = static_cast<int32_t>(floor(mid_x / static_cast<double>(level.linedefs.size())));
    block_mid_y = static_cast<int32_t>(floor(mid_y / static_cast<double>(level.linedefs.size())));
  }

  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(LOG_DEBUG, "[%s] Blockmap lines centered at (%d,%d)", __func__, block_mid_x, block_mid_y);
  }
}

static void InitBlockmap(level_t &level)
{
  bbox_t map_bbox;

  // find limits of linedefs, and store as map limits
  FindBlockmapLimits(level, &map_bbox);

  if (config.verbose)
  {
    PrintLine(LOG_NORMAL, "Map limits: (%d,%d) to (%d,%d)", map_bbox.minx, map_bbox.miny, map_bbox.maxx, map_bbox.maxy);
  }

  block_x = map_bbox.minx - (map_bbox.minx & 0x7);
  block_y = map_bbox.miny - (map_bbox.miny & 0x7);

  block_w = static_cast<size_t>((map_bbox.maxx - block_x) / 128) + 1;
  block_h = static_cast<size_t>((map_bbox.maxy - block_y) / 128) + 1;

  block_count = block_w * block_h;
}

static void PutBlockmap(level_t &level)
{
  if (level.linedefs.size() == 0)
  {
    // just create an empty blockmap lump
    CreateLevelLump(level, "BLOCKMAP")->Finish();
    return;
  }

  block_overflowed = false;

  // initial phase: create internal blockmap containing the index of
  // all lines in each block.
  CreateBlockmap(level);

  // -AJA- second phase: compress the blockmap.  We do this by sorting
  //       the blocks, which is a typical way to detect duplicates in
  //       a large list.  This also detects BLOCKMAP overflow.
  CompressBlockmap(level);

  // final phase: write it out in the correct format
  if (block_overflowed)
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
      PrintLine(LOG_NORMAL, "Blockmap size: %zux%zu (compression: %d%%)", block_w, block_h, block_compression);
    }
  }

  FreeBlockmap();
}

//------------------------------------------------------------------------
// REJECT : Generate the reject table
//------------------------------------------------------------------------

static uint8_t *rej_matrix;
static size_t rej_total_size; // in bytes
static std::vector<size_t> rej_sector_groups;

//
// Allocate the matrix, init sectors into individual groups.
//
static void Reject_Init(level_t &level)
{
  rej_total_size = (level.sectors.size() * level.sectors.size() + 7) / 8;

  rej_matrix = new uint8_t[rej_total_size];
  memset(rej_matrix, 0, rej_total_size);

  rej_sector_groups.resize(level.sectors.size());

  for (size_t i = 0; i < level.sectors.size(); i++)
  {
    rej_sector_groups[i] = i;
  }
}

static void Reject_Free(void)
{
  delete[] rej_matrix;
  rej_matrix = nullptr;
  rej_sector_groups.clear();
}

//
// Algorithm: Initially all sectors are in individual groups.
// Now we scan the linedef list.  For each two-sectored line,
// merge the two sector groups into one.  That's it !
//
static void Reject_GroupSectors(level_t &level)
{
  for (size_t i = 0; i < level.linedefs.size(); i++)
  {
    const linedef_t *line = level.linedefs[i];

    if (!line->right || !line->left)
    {
      continue;
    }

    sector_t *sec1 = line->right->sector;
    sector_t *sec2 = line->left->sector;

    if (!sec1 || !sec2 || sec1 == sec2)
    {
      continue;
    }

    // already in the same group ?
    size_t group1 = rej_sector_groups[sec1->index];
    size_t group2 = rej_sector_groups[sec2->index];

    if (group1 == group2)
    {
      continue;
    }

    // prefer the group numbers to become lower
    if (group1 > group2)
    {
      std::swap(group1, group2);
    }

    // merge the groups
    for (size_t s = 0; s < level.sectors.size(); s++)
    {
      if (rej_sector_groups[s] == group2)
      {
        rej_sector_groups[s] = group1;
      }
    }
  }
}

static void Reject_ProcessSectors(level_t &level)
{
  for (size_t view = 0; view < level.sectors.size(); view++)
  {
    for (size_t target = 0; target < view; target++)
    {
      if (rej_sector_groups[view] == rej_sector_groups[target])
      {
        continue;
      }

      // for symmetry, do both sides at same time

      size_t p1 = view * level.sectors.size() + target;
      size_t p2 = target * level.sectors.size() + view;

      rej_matrix[p1 >> 3] |= (1 << (p1 & 7));
      rej_matrix[p2 >> 3] |= (1 << (p2 & 7));
    }
  }
}

static void Reject_WriteLump(level_t &level)
{
  Lump_c *lump = CreateLevelLump(level, "REJECT", rej_total_size);
  lump->Write(rej_matrix, rej_total_size);
  lump->Finish();
}

//
// For now we only do very basic reject processing, limited to
// determining all isolated groups of sectors (islands that are
// surrounded by void space).
//
static void PutReject(level_t &level)
{
  if (level.sectors.size() == 0)
  {
    // just create an empty reject lump
    CreateLevelLump(level, "REJECT")->Finish();
    return;
  }

  Reject_Init(level);
  Reject_GroupSectors(level);
  Reject_ProcessSectors(level);
  Reject_WriteLump(level);
  Reject_Free();
  if (config.verbose)
  {
    PrintLine(LOG_NORMAL, "Reject size: %zu", rej_total_size);
  }
}

//------------------------------------------------------------------------
// LEVEL : Level structure read/write functions.
//------------------------------------------------------------------------

/* ----- allocation routines ---------------------------- */

vertex_t *NewVertex(level_t &level)
{
  vertex_t *V = UtilCalloc<vertex_t>(sizeof(vertex_t));
  V->index = level.vertices.size();
  level.vertices.push_back(V);
  return V;
}

linedef_t *NewLinedef(level_t &level)
{
  linedef_t *L = UtilCalloc<linedef_t>(sizeof(linedef_t));
  L->index = level.linedefs.size();
  level.linedefs.push_back(L);
  return L;
}

sidedef_t *NewSidedef(level_t &level)
{
  sidedef_t *S = UtilCalloc<sidedef_t>(sizeof(sidedef_t));
  S->index = level.sidedefs.size();
  level.sidedefs.push_back(S);
  return S;
}

sector_t *NewSector(level_t &level)
{
  sector_t *S = UtilCalloc<sector_t>(sizeof(sector_t));
  S->index = level.sectors.size();
  level.sectors.push_back(S);
  return S;
}

thing_t *NewThing(level_t &level)
{
  thing_t *T = UtilCalloc<thing_t>(sizeof(thing_t));
  T->index = level.things.size();
  level.things.push_back(T);
  return T;
}

seg_t *NewSeg(level_t &level)
{
  seg_t *S = UtilCalloc<seg_t>(sizeof(seg_t));
  level.segs.push_back(S);
  return S;
}

subsec_t *NewSubsec(level_t &level)
{
  subsec_t *S = UtilCalloc<subsec_t>(sizeof(subsec_t));
  level.subsecs.push_back(S);
  return S;
}

node_t *NewNode(level_t &level)
{
  node_t *N = UtilCalloc<node_t>(sizeof(node_t));
  level.nodes.push_back(N);
  return N;
}

walltip_t *NewWallTip(level_t &level)
{
  walltip_t *WT = UtilCalloc<walltip_t>(sizeof(walltip_t));
  level.walltips.push_back(WT);
  return WT;
}

intersection_t *NewIntersection(level_t &level)
{
  intersection_t *cut = new intersection_t;
  level.intercuts.push_back(cut);
  return cut;
}

/* ----- free routines ---------------------------- */

void FreeVertices(level_t &level)
{
  for (size_t i = 0; i < level.vertices.size(); i++)
  {
    UtilFree(level.vertices[i]);
  }

  level.vertices.clear();
}

void FreeLinedefs(level_t &level)
{
  for (size_t i = 0; i < level.linedefs.size(); i++)
  {
    UtilFree(level.linedefs[i]);
  }

  level.linedefs.clear();
}

void FreeSidedefs(level_t &level)
{
  for (size_t i = 0; i < level.sidedefs.size(); i++)
  {
    UtilFree(level.sidedefs[i]);
  }

  level.sidedefs.clear();
}

void FreeSectors(level_t &level)
{
  for (size_t i = 0; i < level.sectors.size(); i++)
  {
    UtilFree(level.sectors[i]);
  }

  level.sectors.clear();
}

void FreeThings(level_t &level)
{
  for (size_t i = 0; i < level.things.size(); i++)
  {
    UtilFree(level.things[i]);
  }

  level.things.clear();
}

void FreeSegs(level_t &level)
{
  for (size_t i = 0; i < level.segs.size(); i++)
  {
    UtilFree(level.segs[i]);
  }

  level.segs.clear();
}

void FreeSubsecs(level_t &level)
{
  for (size_t i = 0; i < level.subsecs.size(); i++)
  {
    UtilFree(level.subsecs[i]);
  }

  level.subsecs.clear();
}

void FreeNodes(level_t &level)
{
  for (size_t i = 0; i < level.nodes.size(); i++)
  {
    UtilFree(level.nodes[i]);
  }

  level.nodes.clear();
}

void FreeWallTips(level_t &level)
{
  for (size_t i = 0; i < level.walltips.size(); i++)
  {
    UtilFree(level.walltips[i]);
  }

  level.walltips.clear();
}

void FreeIntersections(level_t &level)
{
  for (size_t i = 0; i < level.intercuts.size(); i++)
  {
    delete level.intercuts[i];
  }

  level.intercuts.clear();
}

/* ----- reading routines ------------------------------ */

void ValidateLinedef(level_t &level, linedef_t *line)
{
  if (line->right || line->left)
  {
    level.num_real_lines++;
  }

  if (line->left && line->right && line->left->sector == line->right->sector)
  {
    line->effects |= FX_SelfReferential;
    if (config.verbose)
    {
      PrintLine(LOG_NORMAL, "Linedef #%zu is self-referencing", line->index);
    }
  }

  if (config.effects                                            // Effects enabled ...
      && line->left                                             // ... side exists ...
      && line->left->tex_lower[0] != '-'                        // ... lower texture isn't empty ...
      && StringCaseCmp(line->left->tex_lower, "BSPNOSEG") == 0) // ... lower texture is BSPNOSEG
  {
    line->effects |= FX_DoNotRenderBack;
  }

  if (config.effects                                             // Effects enabled ...
      && line->right                                             // ... side exists ...
      && line->right->tex_lower[0] != '-'                        // ... lower texture isn't empty ...
      && StringCaseCmp(line->right->tex_lower, "BSPNOSEG") == 0) // ... lower texture is BSPNOSEG
  {
    line->effects |= FX_DoNotRenderFront;
  }

  double deltax = line->start->x - line->end->x;
  double deltay = line->start->y - line->end->y;

  // check for extremely short lines
  if ((fabs(deltax) < DIST_EPSILON) && (fabs(deltay) < DIST_EPSILON))
  {
    line->effects |= FX_ZeroLength;
  }

  // check for extremely long lines
  if (hypot(deltax, deltay) >= 32000)
  {
    PrintLine(LOG_NORMAL, "WARNING: Linedef #%zu is VERY long, it may cause problems", line->index);
    config.total_warnings++;
  }
}

static void GetVertices_Doom(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("VERTEXES");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_vertex_t);
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "ERROR: Failure seeking to vertices.");
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_vertex_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "ERROR: Failure reading vertices.");
    }

    vertex_t *vert = NewVertex(level);

    vert->x = static_cast<double>(GetLittleEndian(raw.x));
    vert->y = static_cast<double>(GetLittleEndian(raw.y));
  }

  level.num_old_vert = level.vertices.size();
}

static void GetSectors_Doom(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("SECTORS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_sector_doom_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "ERROR: Failure seeking to sectors.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_sector_doom_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "ERROR: Failure reading sectors.");
    }

    sector_t *sector = NewSector(level);

    sector->height_floor = static_cast<double>(GetLittleEndian(raw.floorh));
    sector->height_ceiling = static_cast<double>(GetLittleEndian(raw.ceilh));
  }
}

static void GetThings_Doom(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("THINGS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_thing_doom_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "ERROR: Failure seeking to things.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_thing_doom_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "ERROR: Failure reading things.");
    }

    thing_t *thing = NewThing(level);

    thing->x = GetLittleEndian(raw.x);
    thing->y = GetLittleEndian(raw.y);
    thing->type = static_cast<doomednum_t>(GetLittleEndian(raw.type));
  }
}

static void GetSidedefs_Doom(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("SIDEDEFS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_sidedef_doom_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "ERROR: Failure seeking to sidedefs.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_sidedef_doom_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "ERROR: Failure reading sidedefs.");
    }

    sidedef_t *side = NewSidedef(level);

    side->offset_x = GetLittleEndian(raw.x_offset);
    side->offset_y = GetLittleEndian(raw.y_offset);
    memcpy(side->tex_upper, raw.upper_tex, 8);
    memcpy(side->tex_lower, raw.lower_tex, 8);
    memcpy(side->tex_middle, raw.mid_tex, 8);
    side->sector = level.SafeLookupSector(GetLittleEndian(raw.sector), i);
  }
}

static void GetLinedefs_Doom(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("LINEDEFS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_linedef_doom_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "ERROR: Failure seeking to linedefs.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_linedef_doom_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "ERROR: Failure reading linedefs.");
    }

    linedef_t *line = NewLinedef(level);

    line->start = level.SafeLookupVertex(GetLittleEndian(raw.start), i);
    line->end = level.SafeLookupVertex(GetLittleEndian(raw.end), i);
    line->right = level.SafeLookupSidedef(GetLittleEndian(raw.right));
    line->left = level.SafeLookupSidedef(GetLittleEndian(raw.left));
    line->flags = GetLittleEndian(raw.flags);
    line->special = GetLittleEndian(raw.special);
    line->args[0] = GetLittleEndian(raw.tag);

    line->start->is_used = true;
    line->end->is_used = true;

    ValidateLinedef(level, line);

    if (!config.effects) continue;

    // Line tags ( 900 <= x <=999 ) are considered "precious" and will, therefore, have a much higher seg split cost
    switch (line->args[0])
    {
    case 900 ... 997:
      line->effects |= FX_DoNotSplitSeg;
      break;
    case Tag_DoNotRender:
      line->effects |= FX_DoNotRenderFront | FX_DoNotRenderBack | FX_DoNotSplitSeg;
      break;
    case Tag_NoBlockmap:
      line->effects |= FX_NoBlockmap | FX_DoNotSplitSeg;
      break;
    }

    switch (line->special)
    {
    case Special_RotateRelativeDegrees:
      line->angle = FX_RotateRelativeDegrees;
      break;
    case Special_RotateAbsoluteDegrees:
      line->angle = FX_RotateAbsoluteDegrees;
      break;
    case Special_RotateRelativeBAM:
      line->angle = FX_RotateRelativeBAM;
      break;
    case Special_RotateAbsoluteBAM:
      line->angle = FX_RotateAbsoluteBAM;
      break;
    case Special_DoNotRenderSegmentBack:
      line->effects |= FX_DoNotRenderBack;
      break;
    case Special_DoNotRenderSegmentFront:
      line->effects |= FX_DoNotRenderFront;
      break;
    case Special_DoNotRenderSegmentBoth:
      line->effects |= FX_DoNotRenderFront | FX_DoNotRenderBack;
      break;
    case Special_DoNotSplitSeg:
      line->effects |= FX_DoNotSplitSeg;
      break;
    default:
      break;
    }
  }
}

static void GetThings_Hexen(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("THINGS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_thing_hexen_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "ERROR: Failure seeking to things.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_thing_hexen_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "ERROR: Failure reading things.");
    }

    thing_t *thing = NewThing(level);

    thing->x = GetLittleEndian(raw.x);
    thing->y = GetLittleEndian(raw.y);
    thing->type = static_cast<doomednum_t>(GetLittleEndian(raw.type));
  }
}

static void GetLinedefs_Hexen(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("LINEDEFS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_linedef_hexen_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "ERROR: Failure seeking to linedefs.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_linedef_hexen_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "ERROR: Failure reading linedefs.");
    }

    linedef_t *line = NewLinedef(level);

    line->start = level.SafeLookupVertex(GetLittleEndian(raw.start), i);
    line->end = level.SafeLookupVertex(GetLittleEndian(raw.end), i);
    line->flags = GetLittleEndian(raw.flags);
    line->special = raw.special;
    line->args[0] = raw.args[0];
    line->args[1] = raw.args[1];
    line->args[2] = raw.args[2];
    line->args[3] = raw.args[3];
    line->args[4] = raw.args[4];
    line->right = level.SafeLookupSidedef(GetLittleEndian(raw.right));
    line->left = level.SafeLookupSidedef(GetLittleEndian(raw.left));

    line->start->is_used = true;
    line->end->is_used = true;

    ValidateLinedef(level, line);

    if (!config.effects) continue;

    switch (line->special)
    {
    case BSP_SpecialEffects:
      if (line->args[0]) line->effects |= FX_NoBlockmap;
      if (line->args[1]) line->effects |= FX_DoNotSplitSeg;
      if (line->args[2]) line->effects |= FX_DoNotRenderBack;
      if (line->args[3]) line->effects |= FX_DoNotRenderFront;
      break;
    default:
      break;
    }
  }
}

/* ----- UDMF reading routines ------------------------- */

static constexpr uint32_t UDMF_THING = 1;
static constexpr uint32_t UDMF_VERTEX = 2;
static constexpr uint32_t UDMF_SECTOR = 3;
static constexpr uint32_t UDMF_SIDEDEF = 4;
static constexpr uint32_t UDMF_LINEDEF = 5;

static void ParseThingField(thing_t *thing, const std::string &key, token_kind_e kind, const std::string &value)
{
  if (key == "x")
  {
    thing->x = LEX_Double(value);
  }

  if (key == "y")
  {
    thing->y = LEX_Double(value);
  }

  if (key == "type")
  {
    thing->type = static_cast<doomednum_t>(LEX_Int16(value));
  }
}

static void ParseVertexField(vertex_t *vertex, const std::string &key, token_kind_e kind, const std::string &value)
{
  if (key == "x")
  {
    vertex->x = LEX_Double(value);
  }

  if (key == "y")
  {
    vertex->y = LEX_Double(value);
  }
}

static void ParseSectorField(sector_t *sector, const std::string &key, token_kind_e kind, const std::string &value)
{
  // nothing actually needed
}

static void ParseSidedefField(level_t &level, sidedef_t *side, const std::string &key, token_kind_e kind,
                              const std::string &value)
{
  if (key == "sector")
  {
    size_t num = LEX_Index(value);

    if (num >= level.sectors.size())
    {
      PrintLine(LOG_ERROR, "ERROR: illegal sector number #%zu", static_cast<size_t>(num));
    }

    side->sector = level.sectors[num];
  }
}

static void ParseLinedefField(level_t &level, linedef_t *line, const std::string &key, token_kind_e kind,
                              const std::string &value)
{
  if (key == "v1")
  {
    line->start = level.SafeLookupVertex(LEX_Index(value), static_cast<size_t>(line - level.linedefs[0]));
  }

  if (key == "v2")
  {
    line->end = level.SafeLookupVertex(LEX_Index(value), static_cast<size_t>(line - level.linedefs[0]));
  }

  if (key == "special")
  {
    line->special = LEX_Int(value);
  }

  if (key == "twosided")
  {
    if (LEX_Boolean(value))
    {
      line->flags |= MLF_TWOSIDED;
    };
  }

  if (key == "sidefront")
  {
    size_t num = LEX_Index(value);

    if (num >= level.sidedefs.size())
    {
      line->right = nullptr;
    }
    else
    {
      line->right = level.sidedefs[num];
    }
  }

  if (key == "sideback")
  {
    size_t num = LEX_Index(value);

    if (num >= level.sidedefs.size())
    {
      line->left = nullptr;
    }
    else
    {
      line->left = level.sidedefs[num];
    }
  }
}

static void ParseUDMF_Block(level_t &level, lexer_c &lex, int cur_type)
{
  vertex_t *vertex = nullptr;
  thing_t *thing = nullptr;
  sector_t *sector = nullptr;
  sidedef_t *side = nullptr;
  linedef_t *line = nullptr;

  switch (cur_type)
  {
  case UDMF_VERTEX:
    vertex = NewVertex(level);
    break;
  case UDMF_THING:
    thing = NewThing(level);
    break;
  case UDMF_SECTOR:
    sector = NewSector(level);
    break;
  case UDMF_SIDEDEF:
    side = NewSidedef(level);
    break;
  case UDMF_LINEDEF:
    line = NewLinedef(level);
    break;
  default:
    break;
  }

  for (;;)
  {
    if (lex.Match("}"))
    {
      break;
    }

    std::string key;
    std::string value;

    token_kind_e tok = lex.Next(key);

    if (tok == TOK_EOF)
    {
      PrintLine(LOG_ERROR, "ERROR: Malformed TEXTMAP lump: unclosed block");
    }

    if (tok != TOK_Ident)
    {
      PrintLine(LOG_ERROR, "ERROR: Malformed TEXTMAP lump: missing key");
    }

    if (!lex.Match("="))
    {
      PrintLine(LOG_ERROR, "ERROR: Malformed TEXTMAP lump: missing '='");
    }

    tok = lex.Next(value);

    if (tok == TOK_EOF || tok == TOK_ERROR || value == "}")
    {
      PrintLine(LOG_ERROR, "ERROR: Malformed TEXTMAP lump: missing value");
    }

    if (!lex.Match(";"))
    {
      PrintLine(LOG_ERROR, "ERROR: Malformed TEXTMAP lump: missing ';'");
    }

    switch (cur_type)
    {
    case UDMF_VERTEX:
      ParseVertexField(vertex, key, tok, value);
      break;
    case UDMF_THING:
      ParseThingField(thing, key, tok, value);
      break;
    case UDMF_SECTOR:
      ParseSectorField(sector, key, tok, value);
      break;
    case UDMF_SIDEDEF:
      ParseSidedefField(level, side, key, tok, value);
      break;
    case UDMF_LINEDEF:
      ParseLinedefField(level, line, key, tok, value);
      break;

    default: /* just skip it */
      break;
    }
  }

  // validate stuff

  if (line != nullptr)
  {
    if (line->start == nullptr || line->end == nullptr)
    {
      PrintLine(LOG_ERROR, "ERROR: Linedef #%zu is missing a vertex!", line->index);
    }

    ValidateLinedef(level, line);
  }
}

static void ParseUDMF_Pass(level_t &level, const std::string &data, int pass)
{
  // pass = 1 : vertices, sectors, things
  // pass = 2 : sidedefs
  // pass = 3 : linedefs

  lexer_c lex(data);

  for (;;)
  {
    std::string section;
    token_kind_e tok = lex.Next(section);

    if (tok == TOK_EOF)
    {
      return;
    }

    if (tok != TOK_Ident)
    {
      PrintLine(LOG_ERROR, "ERROR: Malformed TEXTMAP lump.");
      return;
    }

    // ignore top-level assignments
    if (lex.Match("="))
    {
      lex.Next(section);
      if (!lex.Match(";"))
      {
        PrintLine(LOG_ERROR, "ERROR: Malformed TEXTMAP lump: missing ';'");
      }
      continue;
    }

    if (!lex.Match("{"))
    {
      PrintLine(LOG_ERROR, "ERROR: Malformed TEXTMAP lump: missing '{'");
    }

    int cur_type = 0;

    if (section == "thing")
    {
      if (pass == 1)
      {
        cur_type = UDMF_THING;
      }
    }
    else if (section == "vertex")
    {
      if (pass == 1)
      {
        cur_type = UDMF_VERTEX;
      }
    }
    else if (section == "sector")
    {
      if (pass == 1)
      {
        cur_type = UDMF_SECTOR;
      }
    }
    else if (section == "sidedef")
    {
      if (pass == 2)
      {
        cur_type = UDMF_SIDEDEF;
      }
    }
    else if (section == "linedef")
    {
      if (pass == 3)
      {
        cur_type = UDMF_LINEDEF;
      }
    }

    // process the block
    ParseUDMF_Block(level, lex, cur_type);
  }
}

void ParseUDMF(level_t &level)
{
  Lump_c *lump = level.FindLevelLump("TEXTMAP");

  if (lump == nullptr || !lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "ERROR: Failure finding TEXTMAP lump.");
  }

  size_t remain = lump->Length();

  // load the lump into this string
  std::string data;

  while (remain > 0)
  {
    char buffer[4096];

    size_t want = std::min(remain, sizeof(buffer));

    if (!lump->Read(buffer, want))
    {
      PrintLine(LOG_ERROR, "ERROR: Failure reading TEXTMAP lump.");
    }

    data.append(buffer, want);

    remain -= want;
  }

  // now parse it...

  // the UDMF spec does not require objects to be in a dependency order.
  // for example: sidedefs may occur *after* the linedefs which refer to
  // them.  hence we perform multiple passes over the TEXTMAP data.
  ParseUDMF_Pass(level, data, 1);
  ParseUDMF_Pass(level, data, 2);
  ParseUDMF_Pass(level, data, 3);

  level.num_old_vert = level.vertices.size();
}

/* ----- writing routines ------------------------------ */

//
// Check Limits
//

static void CheckBinaryFormatLimits(level_t &level)
{
  if (level.num_old_vert > LIMIT_VERT)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Map has too many vertices.");
    level.overflows = true;
  }

  if (level.sectors.size() > LIMIT_SECTOR)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Map has too many sectors.");
    level.overflows = true;
  }

  if (level.sidedefs.size() > LIMIT_SIDE)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Map has too many sidedefs.");
    level.overflows = true;
  }

  if (level.linedefs.size() > LIMIT_LINE)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Map has too many linedefs.");
    level.overflows = true;
  }
}

bsp_type_t CheckFormatBSP(buildinfo_t &ctx, level_t &level)
{
  bsp_type_t level_type = ctx.bsp_type;

  if (level_type == BSP_VANILLA &&            // always allow for a valid map to be produced
      (level.vertices.size() > LIMIT_VERT     // even if it may not run on some older source ports
       || level.nodes.size() > LIMIT_NODE     // or the vanilla EXE
       || level.subsecs.size() > LIMIT_SUBSEC //
       || level.segs.size() > LIMIT_SEG))     //
  {
    PrintLine(LOG_NORMAL, "WARNING: BSP overflow. Forcing DeePBSPV4 node format.");
    config.total_warnings++;
    level_type = BSP_DEEPBSPV4;
  }

  return level_type;
}

/* ----- whole-level routines --------------------------- */

void LoadLevel(level_t &level)
{
  Lump_c *LEV = cur_wad->GetLump(level.level_header_lump_index);

  level.overflows = false;

  PrintLine(LOG_NORMAL, "[%s] Reading %s", __func__, LEV->Name());

  level.num_new_vert = 0;
  level.num_real_lines = 0;

  switch (level.format)
  {
  case MapFormat_Doom:
    GetVertices_Doom(level);
    GetSectors_Doom(level);
    GetSidedefs_Doom(level);
    GetLinedefs_Doom(level);
    GetThings_Doom(level);
    PruneVerticesAtEnd(level);
    break;
  case MapFormat_Hexen:
    GetVertices_Doom(level);
    GetSectors_Doom(level);
    GetSidedefs_Doom(level);
    GetLinedefs_Hexen(level);
    GetThings_Hexen(level);
    PruneVerticesAtEnd(level);
    break;
  case MapFormat_UDMF:
    ParseUDMF(level);
    break;
  case MapFormat_INVALID:
    PrintLine(LOG_ERROR, "[%s] Unknown level format on level %s", __func__, LEV->Name());
    break;
  }

  if (config.verbose)
  {
    PrintLine(LOG_NORMAL, "Loaded %zu vertices, %zu sectors, %zu sides, %zu lines, %zu things", level.vertices.size(),
              level.sectors.size(), level.sidedefs.size(), level.linedefs.size(), level.things.size());
  }

  DetectOverlappingVertices(level);
  DetectOverlappingLines(level);

  CalculateWallTips(level);

  // -JL- Find sectors containing polyobjs
  switch (level.format)
  {
  case MapFormat_Hexen:
  case MapFormat_UDMF:
    DetectPolyobjSectors(config, level);
    break;
  default:
    break;
  }
}

void FreeLevel(level_t &level)
{
  FreeVertices(level);
  FreeSidedefs(level);
  FreeLinedefs(level);
  FreeSectors(level);
  FreeThings(level);
  FreeSegs(level);
  FreeSubsecs(level);
  FreeNodes(level);
  FreeWallTips(level);
  FreeIntersections(level);
}

static void AddMissingLump(level_t &level, const char *name, const char *after)
{
  if (cur_wad->LevelLookupLump(level.level_num, name) != NO_INDEX)
  {
    return;
  }

  size_t exist = cur_wad->LevelLookupLump(level.level_num, after);

  // if this happens, the level structure is very broken
  if (exist == NO_INDEX)
  {
    PrintLine(LOG_NORMAL, "WARNING: Missing %s lump -- level structure is broken", after);
    config.total_warnings++;
    exist = cur_wad->LevelLastLump(level.level_num);
  }

  cur_wad->InsertPoint(exist + 1);
  cur_wad->AddLump(name)->Finish();
}

build_result_e SaveLevelBinaryFormat(level_t &level, node_t *root_node)
{
  // Note: root_node may be nullptr

  cur_wad->BeginWrite();

  // ensure all necessary level lumps are present
  AddMissingLump(level, "SEGS", "VERTEXES");
  AddMissingLump(level, "SSECTORS", "SEGS");
  AddMissingLump(level, "NODES", "SSECTORS");
  AddMissingLump(level, "SECTORS", "NODES");
  AddMissingLump(level, "REJECT", "SECTORS");
  AddMissingLump(level, "BLOCKMAP", "REJECT");

  // check for overflows...
  CheckBinaryFormatLimits(level);

  bsp_type_t level_type = CheckFormatBSP(config, level);

  switch (level_type)
  {
  case BSP_XGL3:
    SaveDoom_XGL3(level, root_node);
    break;
  case BSP_XGL2:
    SaveDoom_XGL2(level, root_node);
    break;
  case BSP_XGLN:
    SaveDoom_XGLN(level, root_node);
    break;
  case BSP_XNOD:
    SaveDoom_XNOD(level, root_node);
    break;
  case BSP_DEEPBSPV4:
    SaveDoom_DeePBSPV4(level, root_node);
    break;
  case BSP_VANILLA:
    SaveDoom_Vanilla(level, root_node);
    break;
  }

  PutBlockmap(level);
  PutReject(level);

  cur_wad->EndWrite();

  if (level.overflows)
  {
    // no message here
    // [ in verbose mode, each overflow already printed a message ]
    // [ in normal mode, we don't want any messages at all ]
    return BUILD_LumpOverflow;
  }

  return BUILD_OK;
}

build_result_e SaveLevelTextMap(level_t &level, node_t *root_node)
{
  cur_wad->BeginWrite();

  // remove any existing ZNODES lump
  cur_wad->RemoveZNodes(level.level_num);

  Lump_c *lump = CreateLevelLump(level, "ZNODES", NO_INDEX);

  // -Elf- Ensure needed lumps exist
  AddMissingLump(level, "REJECT", "ZNODES");
  AddMissingLump(level, "BLOCKMAP", "REJECT");

  if (level.num_real_lines == 0)
  {
    lump->Finish();
  }
  else
  {
    SaveTextmap_ZNODES(level, root_node);
  }

  // -Elf-
  PutBlockmap(level);
  PutReject(level);

  cur_wad->EndWrite();

  return BUILD_OK;
}

/* ---------------------------------------------------------------- */

Lump_c *CreateLevelLump(level_t &level, const char *name, size_t max_size)
{
  // look for existing one
  Lump_c *lump = level.FindLevelLump(name);

  if (lump)
  {
    cur_wad->RecreateLump(lump, max_size);
  }
  else
  {
    size_t last_idx = cur_wad->LevelLastLump(level.level_num);

    // in UDMF maps, insert before the ENDMAP lump, otherwise insert
    // after the last known lump of the level.
    if (level.format != MapFormat_UDMF)
    {
      last_idx += 1;
    }

    cur_wad->InsertPoint(last_idx);

    lump = cur_wad->AddLump(name, max_size);
  }

  return lump;
}

//------------------------------------------------------------------------
// MAIN STUFF
//------------------------------------------------------------------------

void OpenWad(const char *filename)
{
  cur_wad = Wad_file::Open(filename, 'a');
  if (cur_wad == nullptr)
  {
    PrintLine(LOG_ERROR, "ERROR: Cannot open file: %s", filename);
  }

  if (cur_wad->IsReadOnly())
  {
    delete cur_wad;
    cur_wad = nullptr;
    PrintLine(LOG_ERROR, "ERROR: file is read only: %s", filename);
  }
}

void CloseWad(void)
{
  if (cur_wad != nullptr)
  {
    // this closes the file
    delete cur_wad;
    cur_wad = nullptr;
  }
}

size_t LevelsInWad(void)
{
  if (cur_wad == nullptr)
  {
    return 0;
  }

  return cur_wad->LevelCount();
}

size_t ComputeBspHeight(const node_t *node)
{
  if (node == nullptr)
  {
    return 0;
  }

  size_t right = ComputeBspHeight(node->r.node);
  size_t left = ComputeBspHeight(node->l.node);

  return std::max(left, right) + 1;
}

/* ----- build nodes for a single level ----- */

build_result_e BuildLevel(level_t &level, const char *filename)
{
  node_t *root_node = nullptr;
  subsec_t *root_sub = nullptr;

  LoadLevel(level);

  InitBlockmap(level);

  if (level.num_real_lines > 0)
  {
    if (config.analysis)
    {
      PrintLine(LOG_NORMAL, "[%s] Starting analysis loop for %s", __func__, level.GetLevelName());
      GenerateAnalysis(level, filename);
    }

    bbox_t dummy;
    // recursively create nodes
    seg_t *seg_list = CreateSegs(level);
    BuildNodes(level, seg_list, 0, &dummy, &root_node, &root_sub, config.split_cost, config.fast, false);
  }

  if (config.verbose)
  {
    PrintLine(LOG_NORMAL, "Built %zu NODES, %zu SSECTORS, %zu SEGS, %zu VERTEXES", level.nodes.size(), level.subsecs.size(),
              level.segs.size(), level.num_old_vert + level.num_new_vert);
  }

  if (config.verbose && root_node != nullptr)
  {
    PrintLine(LOG_NORMAL, "Heights of subtrees: %zu / %zu", ComputeBspHeight(root_node->l.node),
              ComputeBspHeight(root_node->r.node));
  }

  ClockwiseBspTree(level);

  build_result_t ret = BUILD_OK;
  switch (level.format)
  {
  case MapFormat_Doom:
  case MapFormat_Hexen:
    ret = SaveLevelBinaryFormat(level, root_node);
    break;
  case MapFormat_UDMF:
    ret = SaveLevelTextMap(level, root_node);
    break;
  default:
    break;
  }

  FreeLevel(level);

  if (config.analysis)
  {
    WriteAnalysis(filename);
  }

  return ret;
}
