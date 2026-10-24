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

static constexpr auto VanillaHeaderSize = sizeof(MapBlockmapHeader);
static constexpr auto XBM1HeaderSize = sizeof(MapBlockmapHeaderXBM1);

static constexpr auto XBM1Prefix = "XBM1\0\0\0\0";
static constexpr auto XBM1PrefixSize = sizeof(XBM1Prefix);

static constexpr auto INT16 = sizeof(uint16_t);
static constexpr auto INT32 = sizeof(uint32_t);

/* ----- create blockmap ------------------------------------ */

static void Blockmap_FindLimits(Level &level, MapBoundBox *bbox)
{
  double mid_x = 0;
  double mid_y = 0;
  int16_t block_mid_x = 0;
  int16_t block_mid_y = 0;

  bbox->minx = bbox->miny = Limits<int16_t>::max();
  bbox->maxx = bbox->maxy = Limits<int16_t>::min();

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
    PrintLine(Log::DEBUG, "[{}] Blockmap lines centered at ({},{})", __func__, block_mid_x, block_mid_y);
  }
}

void Blockmap_Init(Level &level)
{
  MapBoundBox map_bbox;

  // find limits of linedefs, and store as map limits
  Blockmap_FindLimits(level, &map_bbox);

  if (config.verbose)
  {
    PrintLine(Log::NORMAL, "[{}] Map limits: ({},{}) to ({},{})", __func__, map_bbox.minx, map_bbox.miny, map_bbox.maxx,
              map_bbox.maxy);
  }

  level.block_x = map_bbox.minx - (map_bbox.minx & 0x7);
  level.block_y = map_bbox.miny - (map_bbox.miny & 0x7);

  level.block_w = static_cast<size_t>((map_bbox.maxx - level.block_x) / 128) + 1;
  level.block_h = static_cast<size_t>((map_bbox.maxy - level.block_y) / 128) + 1;

  level.block_count = level.block_w * level.block_h;
}

static void Blockmap_Free(Level &level)
{
  for (auto list : level.block_lists)
  {
    list.lines.clear();
  }

  level.block_lists.clear();
}

static void Block_Add(Level &level, size_t blk_num, size_t line_index)
{
  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(Log::DEBUG, "[{}] Block {} has line {}", __func__, blk_num, line_index);
  }

  if (blk_num >= level.block_count)
  {
    throw RuntimeException("[{}]: bad block number {}", __func__, blk_num);
  }

  auto &list = level.block_lists[blk_num];

  // compute new checksum
  list.sum.hash = list.sum.Compute(line_index);
  list.lines.push_back(line_index);
}

static void Block_AddLine(Level &level, const Linedef *L)
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

  auto bx1_temp = (std::min(x1, x2) - level.block_x) / 128;
  auto by1_temp = (std::min(y1, y2) - level.block_y) / 128;
  auto bx2_temp = (std::max(x1, x2) - level.block_x) / 128;
  auto by2_temp = (std::max(y1, y2) - level.block_y) / 128;

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
      Block_Add(level, blk_num, line_index);
    }
    return;
  }

  // handle simple case #2: completely vertical
  if (bx1 == bx2)
  {
    for (size_t by = by1; by <= by2; by++)
    {
      size_t blk_num = by * level.block_w + bx1;
      Block_Add(level, blk_num, line_index);
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
        Block_Add(level, blk_num, line_index);
      }
    }
  }
}

// initial phase: create internal blockmap containing the index of
// all lines in each block.
static void Blockmap_Create(Level &level)
{
  level.block_lists.assign(level.block_count, LineList());

  for (auto i : Iota(level.block_lists.size()))
  {
    level.block_lists[i].block_index = i;
  }

  /**
   **  Important bullshit to note:
   **  1. The original DoomBSP code wrote an extra 0-value at the start of every block list.
   **  2. TeamTNT, in their infinite Boom-designing wisdom, choose to always skip the erroneous value, where vanilla does not.
   **  3. We provide an option to not write down the zero, this was used in BTSX (with ZOkumBSP) due to vanilla EXE limits.
   **  4. Due to item number 2, we are forced to include the zero header by default, lest Boom-based ports shit the bed.
   **  5. In MBF21 ports, there exists a smart check to decide to skip or not, this only matters if an user wants to play a
   **     header-less map in MBF21 demo compatibility mode. See BTSX MAP20.
   **  6. If we do have a zero header, skip needlessly adding a second zero to any blocks, by starting the loop at line 1.
   **/
  size_t line_start = 0;

  if (!config.skip_bmap_zero)
  {
    line_start = 1;
    for (auto blk_num : Iota(level.block_count))
    {
      Block_Add(level, blk_num, 0);
    }
  }

  for (auto i : Iota(line_start, level.linedefs.size()))
  {
    auto L = level.linedefs[i];

    if (HAS_BIT(L->effects, FX_NoBlockmap | FX_ZeroLength))
    {
      continue;
    }

    Block_AddLine(level, L);
  }

  // Force extended format
  if (level.bmap_format < BlockmapFormat::XBM1 && level.linedefs.size() > LIMIT_LINE)
  {
    PrintLine(Log::WARN, "[{}] Blockmap overflow. Forcing XBM1 format.", __func__);
    config.total_warnings++;
    RaiseValue(level.bmap_format, BlockmapFormat::XBM1);
  }
}

// -Elf-
// Run through all duplicates by hash, and use an index to keep track of the values for offset list.
// Non-duplicate blocks get their normal index value, duplicates get the index of the first found instance.
static void Blockmap_Deduplication(Level &level)
{
  size_t duplicate_count = 0;

  IndexMap<Vec<size_t>> block_hash;
  block_hash.reserve(level.block_count);

  for (auto blk_num : Iota(level.block_count))
  {
    auto key = level.block_lists[blk_num].sum.hash;
    block_hash[key].push_back(blk_num);
  }

  for (auto [_, dupes] : block_hash)
  {
    // No duplicates, skip
    if (dupes.size() < 2)
    {
      continue;
    }

    // Duplicates, must check
    for (auto i : Iota(dupes.size()))
    {
      auto &list1 = level.block_lists[dupes[i]];

      // Already marked?
      if (list1.duplicate)
      {
        continue;
      }

      for (auto j : Iota(i + 1, dupes.size()))
      {
        auto &list2 = level.block_lists[dupes[j]];

        // Already marked?
        if (list2.duplicate)
        {
          continue;
        }

        // Confirmed duplicates?
        if (list1.lines == list2.lines)
        {
          // Mark the latter as duplicated so we write only the former
          list2.duplicate = true;
          list2.block_index = i;
          duplicate_count++;
        }
      }
    }
  }

  if (config.verbose)
  {
    auto final = level.block_count - duplicate_count;
    PrintLine(Log::DEBUG, "[{}] Total = {}, duplicates = {}, final = {}", __func__, level.block_count, duplicate_count, final);
  }
}

void Blockmap_FlattenList(Level &level)
{
  // handle each block list
  for (auto list : level.block_lists)
  {
    list.flat_list_index = level.block_flat_list.size();

    // keep track of uncompressed size for comparison's sake
    for (auto _ : list.lines)
    {
      level.block_flat_list_uncompressed_size += 1;
    }
    level.block_flat_list_uncompressed_size += 1;

    // ignore duplicate blocks for real output
    if (list.duplicate) continue;

    for (auto line : list.lines)
    {
      level.block_flat_list.push_back(line);
    }
    level.block_flat_list.push_back(NO_INDEX);
  }
}

void Blockmap_CheckOverflow(Level &level)
{
  auto OffsetList = level.block_lists.size();
  auto BlockFlatListCompressed = level.block_flat_list.size();
  auto BlockFlatListUncompressed = level.block_flat_list_uncompressed_size;

  auto LastFlatListIndex = level.block_lists.back().flat_list_index;
  auto VanillaOverflowCheck = VanillaHeaderSize + (OffsetList * INT16) + (LastFlatListIndex * INT16);

  auto CompressedVanilla = VanillaHeaderSize + (OffsetList * INT16) + (BlockFlatListCompressed * INT16);
  auto UncompressedVanilla = VanillaHeaderSize + (OffsetList * INT16) + (BlockFlatListUncompressed * INT16);

  auto CompressedXBM1 = XBM1PrefixSize + XBM1HeaderSize + (OffsetList * INT32) + (BlockFlatListCompressed * INT32);
  auto UncompressedXBM1 = XBM1PrefixSize + XBM1HeaderSize + (OffsetList * INT32) + (BlockFlatListUncompressed * INT32);

  if (level.bmap_format < BlockmapFormat::XBM1 && VanillaOverflowCheck > LIMIT_BMAP_INDEX)
  {
    level.block_compression = static_cast<double>(UncompressedXBM1 - CompressedXBM1) / static_cast<double>(UncompressedXBM1);
    // Overflowed?
    PrintLine(Log::WARN, "[{}] Blockmap overflow. Forcing XBM1 format.", __func__);
    config.total_warnings++;
    RaiseValue(level.bmap_format, BlockmapFormat::XBM1);
  }
  else
  {
    level.block_compression =
        static_cast<double>(UncompressedVanilla - CompressedVanilla) / static_cast<double>(UncompressedVanilla);
  }

  if (config.verbose)
  {
    PrintLine(Log::DEBUG, "[{}] Uncompressed vanilla size: {}, Compressed vanilla size: {}, Vanilla ratio: {:.2}%", __func__,
              UncompressedVanilla, CompressedVanilla, level.block_compression * 100);
    PrintLine(Log::DEBUG, "[{}] Uncompressed XBM1 size: {}, Compressed XBM1 size: {}, XBM1 ratio: {:.2}%", __func__,
              UncompressedXBM1, CompressedXBM1, level.block_compression * 100);
  }
}

template <BlockmapFormat format, typename NumType>
static void Blockmap_Write(WadIO &io, Level &level)
{
  constexpr size_t NumSize = sizeof(NumType);
  size_t lump_offset_size = 0;

  if constexpr (format == BlockmapFormat::XBM1)
  {
    io.AddToLump(XBM1Prefix, XBM1PrefixSize);
    lump_offset_size += XBM1PrefixSize;

    MapBlockmapHeaderXBM1 xbm1_header;
    xbm1_header.x_origin = GetLittleEndian(IntToFixed(level.block_x));
    xbm1_header.y_origin = GetLittleEndian(IntToFixed(level.block_y));
    xbm1_header.x_blocks = GetLittleEndian(IndexToInt(level.block_w));
    xbm1_header.y_blocks = GetLittleEndian(IndexToInt(level.block_h));
    io.AddToLump(&xbm1_header, XBM1HeaderSize);
    lump_offset_size += XBM1HeaderSize;
  }
  else
  {
    MapBlockmapHeader header;
    header.x_origin = GetLittleEndian(level.block_x);
    header.y_origin = GetLittleEndian(level.block_y);
    header.x_blocks = GetLittleEndian(IndexToShort(level.block_w));
    header.y_blocks = GetLittleEndian(IndexToShort(level.block_h));
    io.AddToLump(&header, VanillaHeaderSize);
    lump_offset_size += VanillaHeaderSize;
  }

  lump_offset_size += (level.block_lists.size() * NumSize);

  // handle pointers
  for (auto cell : level.block_lists)
  {
    auto list = level.block_lists[cell.block_index];
    auto lump_index = lump_offset_size + (list.flat_list_index * NumSize);

    NumType offset = GetLittleEndian(static_cast<NumType>(lump_index));
    if (offset == 0 or offset == static_cast<NumType>(NO_INDEX))
    {
      throw RuntimeException("[{}]: offset {} not set.", __func__, list.block_index);
    }
    io.AddToLump(&offset, NumSize);
  }

  for (auto value : level.block_flat_list)
  {
    io.AddToLump(&value, NumSize);
  }
}

void PutBlockmap(WadIO &io, Level &level)
{
  if (level.skip_bmap)
  {
    io.LevelCopyLump(level.lumps.label, "BLOCKMAP");
    return;
  }

  auto mark = Benchmarker(__func__);
  io.StartWritingLump("BLOCKMAP");

  // just leave an empty blockmap lump
  if (level.linedefs.size() == 0)
  {
    return;
  }

  RaiseValue(level.bmap_format, config.bmap_format);

  Blockmap_Create(level);
  // Blockmap_Deduplication(level);
  Blockmap_FlattenList(level);
  Blockmap_CheckOverflow(level);

  switch (level.bmap_format)
  {
  case BlockmapFormat::DoomBSP:
    Blockmap_Write<BlockmapFormat::DoomBSP, uint16_t>(io, level);
    break;
  case BlockmapFormat::XBM1:
    Blockmap_Write<BlockmapFormat::XBM1, uint32_t>(io, level);
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
    PrintLine(Log::NORMAL, "[{}] Blockmap size: {}x{} ", __func__, level.block_w, level.block_h,
              static_cast<int32_t>(level.block_compression * 100));
  }
  Blockmap_Free(level);
}
