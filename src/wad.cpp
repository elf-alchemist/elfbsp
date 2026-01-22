//------------------------------------------------------------------------------
//
//  ELFBSP -- WAD Reading / Writing
//
//------------------------------------------------------------------------------
//
//  Copyright 2025-2026 Guilherme Miranda
//  Copyright 2001-2018 Andrew Apted
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

#include "wad.hpp"
#include "elfbsp.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>

//------------------------------------------------------------------------
//  LUMP Handling
//------------------------------------------------------------------------

Lump_c *MakeLump(Wad_file *wad, const char *lumpname, size_t l_start, size_t l_length)
{
  Lump_c *new_lump = new Lump_c;
  new_lump->Rename(lumpname);
  new_lump->parent = wad;
  new_lump->l_start = l_start;
  new_lump->l_length = l_length;
  return new_lump;
}

Lump_c *MakeLumpFromEntry(Wad_file *wad, const raw_wad_entry_t *entry)
{
  Lump_c *new_lump = new Lump_c;

  // handle the entry name, which can lack a terminating NUL
  char buffer[9];
  strncpy(buffer, entry->name, 8);
  buffer[8] = 0;
  new_lump->Rename(buffer);
  new_lump->parent = wad;
  new_lump->l_start = GetLittleEndian(entry->pos);
  new_lump->l_length = GetLittleEndian(entry->size);

  if (HAS_BIT(config.debug, DEBUG_WAD))
  {
    PrintLine(LOG_DEBUG, "[%s] New lump '%s' @ %zu len:%zu", __func__, new_lump->Name(), new_lump->l_start, new_lump->l_length);
  }
  return new_lump;
}

void Lump_c::MakeEntry(raw_wad_entry_t *entry)
{
  // do a dance to avoid a compiler warning from strncpy(), *sigh*
  memset(entry->name, 0, 8);
  memcpy(entry->name, lumpname.c_str(), lumpname.size());

  entry->pos = GetLittleEndian(static_cast<uint32_t>(l_start));
  entry->size = GetLittleEndian(static_cast<uint32_t>(l_length));
}

//------------------------------------------------------------------------
//  WAD Reading Interface
//------------------------------------------------------------------------

Wad_file::Wad_file(const char *_name, char _mode, FILE *_fp)
    : mode(_mode), fp(_fp), kind('P'), total_size(0), directory(), dir_start(0), dir_count(0), levels(), patches(), sprites(),
      flats(), tx_tex(), begun_write(false), insert_point(NO_INDEX)
{
  // nothing needed
}

Wad_file::~Wad_file(void)
{
  fclose(fp);

  // free the directory
  for (size_t k = 0; k < NumLumps(); k++)
  {
    delete directory[k];
  }

  directory.clear();
}

Wad_file *Wad_file::Open(const char *filename, char mode)
{
  SYS_ASSERT(mode == 'r' || mode == 'w' || mode == 'a');

  if (mode == 'w')
  {
    return Create(filename, mode);
  }

  if (HAS_BIT(config.debug, DEBUG_WAD))
  {
    PrintLine(LOG_DEBUG, "[%s] Opening WAD file: %s", __func__, filename);
  }

  FILE *fp = nullptr;

retry:
  fp = fopen(filename, (mode == 'r' ? "rb" : "r+b"));

  if (!fp)
  {
    // mimic the fopen() semantics
    if (mode == 'a' && errno == ENOENT)
    {
      return Create(filename, mode);
    }

    // if file is read-only, open in 'r' mode instead
    if (mode == 'a' && (errno == EACCES || errno == EROFS))
    {
      if (HAS_BIT(config.debug, DEBUG_WAD))
      {
        PrintLine(LOG_DEBUG, "[%s] Open r/w failed, trying again in read mode...", __func__);
      }
      mode = 'r';
      goto retry;
    }

    if (HAS_BIT(config.debug, DEBUG_WAD))
    {
      PrintLine(LOG_DEBUG, "[%s] Open file failed: %s", __func__, strerror(errno));
    }
    return nullptr;
  }

  Wad_file *w = new Wad_file(filename, mode, fp);

  // determine total size (seek to end)
  if (fseeko(fp, 0, SEEK_END) != 0)
  {
    PrintLine(LOG_ERROR, "Error determining WAD size.");
  }

  w->total_size = ftello(fp);

  if (HAS_BIT(config.debug, DEBUG_WAD))
  {
    PrintLine(LOG_DEBUG, "[%s] total_size = %zu", __func__, w->total_size);
  }

  if (w->total_size < 0)
  {
    PrintLine(LOG_ERROR, "Error determining WAD size.");
  }

  w->ReadDirectory();
  w->DetectLevels();
  w->ProcessNamespaces();

  return w;
}

Wad_file *Wad_file::Create(const char *filename, char mode)
{
  if (HAS_BIT(config.debug, DEBUG_WAD))
  {
    PrintLine(LOG_DEBUG, "[%s] Creating new WAD file: %s", __func__, filename);
  }

  FILE *fp = fopen(filename, "w+b");
  if (!fp)
  {
    return nullptr;
  }

  Wad_file *w = new Wad_file(filename, mode, fp);

  // write out base header
  raw_wad_header_t header;

  memset(&header, 0, sizeof(header));
  memcpy(header.ident, "PWAD", 4);

  fwrite(&header, sizeof(header), 1, fp);
  fflush(fp);

  w->total_size = sizeof(header);

  return w;
}

static size_t WhatLevelPart(const char *name)
{
  if (StringCaseCmp(name, "THINGS") == 0)
  {
    return 1;
  }
  if (StringCaseCmp(name, "LINEDEFS") == 0)
  {
    return 2;
  }
  if (StringCaseCmp(name, "SIDEDEFS") == 0)
  {
    return 3;
  }
  if (StringCaseCmp(name, "VERTEXES") == 0)
  {
    return 4;
  }
  if (StringCaseCmp(name, "SECTORS") == 0)
  {
    return 5;
  }

  return 0;
}

static bool IsLevelLump(const char *name)
{
  if (StringCaseCmp(name, "SEGS") == 0)
  {
    return true;
  }
  if (StringCaseCmp(name, "SSECTORS") == 0)
  {
    return true;
  }
  if (StringCaseCmp(name, "NODES") == 0)
  {
    return true;
  }
  if (StringCaseCmp(name, "REJECT") == 0)
  {
    return true;
  }
  if (StringCaseCmp(name, "BLOCKMAP") == 0)
  {
    return true;
  }
  if (StringCaseCmp(name, "BEHAVIOR") == 0)
  {
    return true;
  }
  if (StringCaseCmp(name, "SCRIPTS") == 0)
  {
    return true;
  }

  return WhatLevelPart(name) != 0;
}

Lump_c *Wad_file::GetLump(size_t index)
{
  SYS_ASSERT(index < NumLumps());
  SYS_ASSERT(directory[index]);

  return directory[index];
}

static constexpr uint32_t MAX_LUMPS_IN_A_LEVEL = 21;

size_t Wad_file::LevelLookupLump(size_t lev_num, const char *name)
{
  size_t start = LevelHeader(lev_num);
  size_t finish = LevelLastLump(lev_num);

  for (size_t k = start + 1; k <= finish; k++)
  {
    SYS_ASSERT(k < NumLumps());

    if (directory[k]->Match(name))
    {
      return k;
    }
  }

  return NO_INDEX; // not found
}

size_t Wad_file::LevelLastLump(size_t lev_num)
{
  size_t start = LevelHeader(lev_num);
  size_t count = 1;

  // UDMF level?
  if (LevelFormat(lev_num) == MAPF_UDMF)
  {
    while (count < MAX_LUMPS_IN_A_LEVEL && start + count < NumLumps())
    {
      if (directory[start + count]->Match("ENDMAP"))
      {
        count++;
        break;
      }

      count++;
    }
  }
  else // standard DOOM or HEXEN format
  {
    while (count < MAX_LUMPS_IN_A_LEVEL && start + count < NumLumps() && IsLevelLump(directory[start + count]->Name()))
    {
      count++;
    }
  }

  return start + count - 1;
}

size_t Wad_file::LevelHeader(size_t lev_num)
{
  SYS_ASSERT(lev_num < LevelCount());

  return levels[lev_num];
}

map_format_e Wad_file::LevelFormat(size_t lev_num)
{
  size_t start = LevelHeader(lev_num);

  if (start + 2 < NumLumps())
  {
    const char *name = GetLump(start + 1)->Name();

    if (StringCaseCmp(name, "TEXTMAP") == 0)
    {
      return MAPF_UDMF;
    }
  }

  if (start + LL_BEHAVIOR < NumLumps())
  {
    const char *name = GetLump(start + LL_BEHAVIOR)->Name();

    if (StringCaseCmp(name, "BEHAVIOR") == 0)
    {
      return MAPF_Hexen;
    }
  }

  return MAPF_Doom;
}

void Wad_file::ReadDirectory(void)
{
  // WISH: no fatal errors

  rewind(fp);

  raw_wad_header_t header;

  if (fread(&header, sizeof(header), 1, fp) != 1)
  {
    PrintLine(LOG_ERROR, "Error reading WAD header.");
  }

  // WISH: check ident for PWAD or IWAD

  kind = header.ident[0];

  dir_start = GetLittleEndian(header.dir_start);
  dir_count = GetLittleEndian(header.num_entries);

  if (dir_count > 32000)
  {
    PrintLine(LOG_ERROR, "Bad WAD header, too many entries (%zu)", dir_count);
  }

  if (fseeko(fp, static_cast<off_t>(dir_start), SEEK_SET) != 0)
  {
    PrintLine(LOG_ERROR, "Error seeking to WAD directory.");
  }

  for (size_t i = 0; i < dir_count; i++)
  {
    raw_wad_entry_t entry;

    if (fread(&entry, sizeof(entry), 1, fp) != 1)
    {
      PrintLine(LOG_ERROR, "Error reading WAD directory.");
    }

    Lump_c *lump = MakeLumpFromEntry(this, &entry);

    // WISH: check if entry is valid

    directory.push_back(lump);
  }
}

void Wad_file::DetectLevels(void)
{
  // Determine what lumps in the wad are level markers, based on the
  // lumps which follow it.  Store the result in the 'levels' vector.
  // The test here is rather lax, since wads exist with a non-standard
  // ordering of level lumps.
  for (size_t k = 0; k + 1 < NumLumps(); k++)
  {
    size_t part_mask = 0;
    size_t part_count = 0;

    // Ignore non-header map lumps
    // Fixes sliding window bug on single-level WADs
    if (WhatLevelPart(directory[k]->Name()) != 0)
    {
      continue;
    }

    // check for UDMF levels
    if (directory[k + 1]->Match("TEXTMAP"))
    {
      levels.push_back(k);
      if (HAS_BIT(config.debug, DEBUG_WAD))
      {
        PrintLine(LOG_DEBUG, "[%s] Detected level : %s (UDMF)", __func__, directory[k]->Name());
      }

      continue;
    }

    // check whether the next four lumps are level lumps
    for (size_t i = 1; i <= 4; i++)
    {
      if (k + i >= NumLumps())
      {
        break;
      }

      size_t part = WhatLevelPart(directory[k + i]->Name());

      if (part == 0)
      {
        break;
      }

      // do not allow duplicates
      if (part_mask & (1 << part))
      {
        break;
      }

      part_mask |= (1 << part);
      part_count++;
    }

    if (part_count == 4)
    {
      levels.push_back(k);

      if (HAS_BIT(config.debug, DEBUG_WAD))
      {
        PrintLine(LOG_DEBUG, "[%s] Detected level : %s", __func__, directory[k]->Name());
      }
    }
  }

  // sort levels into alphabetical order
  SortLevels();
}

void Wad_file::SortLevels(void)
{
  // predicate for sorting the levels[] vector
  struct level_name_CMP_pred
  {
    Wad_file *wad;

    level_name_CMP_pred(Wad_file *_w) : wad(_w)
    {
    }

    inline bool operator()(const size_t A, const size_t B) const
    {
      const Lump_c *L1 = wad->directory[A];
      const Lump_c *L2 = wad->directory[B];

      return (strcmp(L1->Name(), L2->Name()) < 0);
    }
  };

  std::sort(levels.begin(), levels.end(), level_name_CMP_pred(this));
}

static bool IsDummyMarker(const char *name)
{
  // matches P1_START, F3_END etc...
  if (strlen(name) < 3)
  {
    return false;
  }

  if (!strchr("PSF", toupper(name[0])))
  {
    return false;
  }

  if (!isdigit(name[1]))
  {
    return false;
  }

  if (StringCaseCmp(name + 2, "_START") == 0 || StringCaseCmp(name + 2, "_END") == 0)
  {
    return true;
  }

  return false;
}

void Wad_file::ProcessNamespaces(void)
{
  char active = 0;

  for (size_t k = 0; k < NumLumps(); k++)
  {
    const char *name = directory[k]->Name();

    // skip the sub-namespace markers
    if (IsDummyMarker(name))
    {
      continue;
    }

    if (StringCaseCmp(name, "P_START") == 0 || StringCaseCmp(name, "PP_START") == 0)
    {
      if (active && active != 'P')
      {
        if (HAS_BIT(config.debug, DEBUG_WAD))
        {
          PrintLine(LOG_DEBUG, "[%s] Missing %c_END marker.", __func__, active);
        }
      }

      active = 'P';
      continue;
    }
    else if (StringCaseCmp(name, "P_END") == 0 || StringCaseCmp(name, "PP_END") == 0)
    {
      if (active != 'P')
      {
        if (HAS_BIT(config.debug, DEBUG_WAD))
        {
          PrintLine(LOG_DEBUG, "[%s] Stray P_END marker found.", __func__);
        }
      }

      active = 0;
      continue;
    }

    if (StringCaseCmp(name, "S_START") == 0 || StringCaseCmp(name, "SS_START") == 0)
    {
      if (active && active != 'S')
      {
        if (HAS_BIT(config.debug, DEBUG_WAD))
        {
          PrintLine(LOG_DEBUG, "[%s] Missing %c_END marker.", __func__, active);
        }
      }

      active = 'S';
      continue;
    }
    else if (StringCaseCmp(name, "S_END") == 0 || StringCaseCmp(name, "SS_END") == 0)
    {
      if (active != 'S')
      {
        if (HAS_BIT(config.debug, DEBUG_WAD))
        {
          PrintLine(LOG_DEBUG, "[%s] stray S_END marker found.", __func__);
        }
      }

      active = 0;
      continue;
    }

    if (StringCaseCmp(name, "F_START") == 0 || StringCaseCmp(name, "FF_START") == 0)
    {
      if (active && active != 'F')
      {
        if (HAS_BIT(config.debug, DEBUG_WAD))
        {
          PrintLine(LOG_DEBUG, "[%s] missing %c_END marker.", __func__, active);
        }
      }

      active = 'F';
      continue;
    }
    else if (StringCaseCmp(name, "F_END") == 0 || StringCaseCmp(name, "FF_END") == 0)
    {
      if (active != 'F')
      {
        if (HAS_BIT(config.debug, DEBUG_WAD))
        {
          PrintLine(LOG_DEBUG, "[%s] stray F_END marker found.", __func__);
        }
      }

      active = 0;
      continue;
    }

    if (StringCaseCmp(name, "TX_START") == 0)
    {
      if (active && active != 'T')
      {
        if (HAS_BIT(config.debug, DEBUG_WAD))
        {
          PrintLine(LOG_DEBUG, "[%s] missing %c_END marker.", __func__, active);
        }
      }

      active = 'T';
      continue;
    }
    else if (StringCaseCmp(name, "TX_END") == 0)
    {
      if (active != 'T')
      {
        if (HAS_BIT(config.debug, DEBUG_WAD))
        {
          PrintLine(LOG_DEBUG, "[%s] stray TX_END marker found.", __func__);
        }
      }

      active = 0;
      continue;
    }

    if (active)
    {
      if (directory[k]->Length() == 0)
      {
        if (HAS_BIT(config.debug, DEBUG_WAD))
        {
          PrintLine(LOG_DEBUG, "[%s] skipping empty lump %s in %c_START", __func__, name, active);
        }
        continue;
      }

      if (HAS_BIT(config.debug, DEBUG_WAD))
      {
        PrintLine(LOG_DEBUG, "[%s] Namespace %c lump : %s", __func__, active, name);
      }

      switch (active)
      {
        case 'P':
          patches.push_back(k);
          break;
        case 'S':
          sprites.push_back(k);
          break;
        case 'F':
          flats.push_back(k);
          break;
        case 'T':
          tx_tex.push_back(k);
          break;

        default:
          PrintLine(LOG_ERROR, "ProcessNamespaces: active = 0x%02x", active);
      }
    }
  }

  if (HAS_BIT(config.debug, DEBUG_WAD))
  {
    if (active)
    {
      PrintLine(LOG_DEBUG, "[%s] Missing %c_END marker (at EOF)", __func__, active);
    }
  }
}

//------------------------------------------------------------------------
//  WAD Writing Interface
//------------------------------------------------------------------------

void Wad_file::BeginWrite(void)
{
  if (mode == 'r')
  {
    PrintLine(LOG_ERROR, "Wad_file::BeginWrite() called on read-only file");
  }

  if (begun_write)
  {
    PrintLine(LOG_ERROR, "Wad_file::BeginWrite() called again without EndWrite()");
  }

  // put the size into a quantum state
  total_size = 0;
  begun_write = true;
}

void Wad_file::EndWrite(void)
{
  if (!begun_write)
  {
    PrintLine(LOG_ERROR, "Wad_file::EndWrite() called without BeginWrite()");
  }

  begun_write = false;

  WriteDirectory();

  // reset the insertion point
  insert_point = NO_INDEX;
}

void Wad_file::RemoveLumps(size_t index, size_t count)
{
  SYS_ASSERT(begun_write);
  SYS_ASSERT(index < NumLumps());
  SYS_ASSERT(directory[index]);

  for (size_t i = 0; i < count; i++)
  {
    delete directory[index + i];
  }

  for (size_t i = index; i + count < NumLumps(); i++)
  {
    directory[i] = directory[i + count];
  }

  directory.resize(directory.size() - count);

  // fix various arrays containing lump indices
  FixGroup(levels, index, 0, count);
  FixGroup(patches, index, 0, count);
  FixGroup(sprites, index, 0, count);
  FixGroup(flats, index, 0, count);
  FixGroup(tx_tex, index, 0, count);

  // reset the insertion point
  insert_point = NO_INDEX;
}

void Wad_file::RemoveZNodes(size_t lev_num)
{
  SYS_ASSERT(begun_write);
  SYS_ASSERT(lev_num < LevelCount());

  size_t start = LevelHeader(lev_num);
  size_t finish = LevelLastLump(lev_num);

  for (; start <= finish; start++)
  {
    if (StringCaseCmp(directory[start]->Name(), "ZNODES") == 0)
    {
      RemoveLumps(start, 1);
      break;
    }
  }
}

void Wad_file::FixGroup(std::vector<size_t> &group, size_t index, size_t num_added, size_t num_removed)
{
  bool did_remove = false;

  for (size_t k = 0; k < group.size(); k++)
  {
    if (group[k] < index)
    {
      continue;
    }

    if (group[k] < index + num_removed)
    {
      group[k] = NO_INDEX;
      did_remove = true;
      continue;
    }

    group[k] += num_added;
    group[k] -= num_removed;
  }

  if (did_remove)
  {
    std::vector<size_t>::iterator ENDP;
    ENDP = std::remove(group.begin(), group.end(), -1);
    group.erase(ENDP, group.end());
  }
}

Lump_c *Wad_file::AddLump(const char *name, size_t max_size)
{
  SYS_ASSERT(begun_write);

  begun_max_size = max_size;

  size_t start = PositionForWrite(max_size);

  Lump_c *lump = MakeLump(this, name, start, 0);

  // check if the insert_point is still valid
  if (insert_point >= NumLumps())
  {
    insert_point = NO_INDEX;
  }

  if (insert_point != NO_INDEX)
  {
    // fix various arrays containing lump indices
    FixGroup(levels, insert_point, 1, 0);
    FixGroup(patches, insert_point, 1, 0);
    FixGroup(sprites, insert_point, 1, 0);
    FixGroup(flats, insert_point, 1, 0);
    FixGroup(tx_tex, insert_point, 1, 0);

    directory.insert(directory.begin() + static_cast<int32_t>(insert_point), lump);

    insert_point++;
  }
  else // add to end
  {
    directory.push_back(lump);
  }

  return lump;
}

void Wad_file::RecreateLump(Lump_c *lump, size_t max_size)
{
  SYS_ASSERT(begun_write);

  begun_max_size = max_size;

  size_t start = PositionForWrite(max_size);

  lump->l_start = start;
  lump->l_length = 0;
}

void Wad_file::InsertPoint(size_t index)
{
  // this is validated on usage
  insert_point = index;
}

size_t Wad_file::HighWaterMark(void)
{
  size_t offset = sizeof(raw_wad_header_t);

  for (size_t k = 0; k < NumLumps(); k++)
  {
    Lump_c *lump = directory[k];

    // ignore zero-length lumps (their offset could be anything)
    if (lump->Length() <= 0)
    {
      continue;
    }

    size_t l_end = lump->l_start + lump->l_length;

    l_end = ((l_end + 3) / 4) * 4;

    if (offset < l_end)
    {
      offset = l_end;
    }
  }

  return offset;
}

size_t Wad_file::FindFreeSpace(size_t length)
{
  length = ((length + 3) / 4) * 4;

  // collect non-zero length lumps and sort by their offset
  std::vector<Lump_c *> sorted_dir;

  for (size_t k = 0; k < NumLumps(); k++)
  {
    Lump_c *lump = directory[k];

    if (lump->Length() > 0)
    {
      sorted_dir.push_back(lump);
    }
  }

  struct offset_CMP_pred
  {
    inline bool operator()(const Lump_c *A, const Lump_c *B) const
    {
      return A->l_start < B->l_start;
    }
  };

  std::sort(sorted_dir.begin(), sorted_dir.end(), offset_CMP_pred());

  size_t offset = sizeof(raw_wad_header_t);

  for (size_t k = 0; k < sorted_dir.size(); k++)
  {
    Lump_c *lump = sorted_dir[k];

    size_t l_start = lump->l_start;
    size_t l_end = lump->l_start + lump->l_length;

    l_end = ((l_end + 3) / 4) * 4;

    if (l_end <= offset)
    {
      continue;
    }

    if (l_start >= offset + length)
    {
      continue;
    }

    // the lump overlapped the current gap, so bump offset

    offset = l_end;
  }

  return offset;
}

size_t Wad_file::PositionForWrite(size_t max_size)
{
  off_t want_pos = static_cast<off_t>(max_size == NO_INDEX ? HighWaterMark() : FindFreeSpace(max_size));

  // determine if position is past end of file
  // (difference should only be a few bytes)
  //
  // Note: doing this for every new lump may be a little expensive,
  //       but trying to optimise it away will just make the code
  //       needlessly complex and hard to follow.

  if (fseeko(fp, 0, SEEK_END) < 0)
  {
    PrintLine(LOG_ERROR, "Error seeking to new write position.");
  }

  total_size = ftello(fp);

  if (total_size < 0)
  {
    PrintLine(LOG_ERROR, "Error seeking to new write position.");
  }

  if (want_pos > total_size)
  {
    SYS_ASSERT(want_pos < total_size + 8);

    WritePadding(static_cast<size_t>(want_pos - total_size));
  }
  else if (want_pos == total_size)
  {
    /* ready to write */
  }
  else
  {
    if (fseeko(fp, want_pos, SEEK_SET) < 0)
    {
      PrintLine(LOG_ERROR, "Error seeking to new write position.");
    }
  }

  if (HAS_BIT(config.debug, DEBUG_WAD))
  {
    PrintLine(LOG_DEBUG, "[%s] POSITION FOR WRITE: %zu  (total_size %zu)", __func__, want_pos, total_size);
  }

  return static_cast<size_t>(want_pos);
}

bool Wad_file::FinishLump(size_t final_size)
{
  fflush(fp);

  // sanity check
  if (final_size > begun_max_size)
  {
    PrintLine(LOG_ERROR, "Internal Error: wrote too much in lump (%zu > %zu)", final_size, begun_max_size);
  }

  off_t pos = ftello(fp);

  if (pos & 3)
  {
    WritePadding(4 - (pos & 3));
  }

  fflush(fp);
  return true;
}

size_t Wad_file::WritePadding(size_t count)
{
  static byte zeros[8] = {0, 0, 0, 0, 0, 0, 0, 0};

  SYS_ASSERT(1 <= count && count <= 8);

  fwrite(zeros, count, 1, fp);

  return count;
}

//
// IDEA : Truncate file to "total_size" after writing the directory.
//
//        On Linux / MacOSX, this can be done as follows:
//                 - fflush(fp)   -- ensure STDIO has empty buffers
//                 - ftruncate(fileno(fp), total_size);
//                 - freopen(fp)
//
//        On Windows:
//                 - instead of ftruncate, use _chsize() or _chsize_s()
//                   [ investigate what the difference is.... ]
//

void Wad_file::WriteDirectory(void)
{
  dir_start = PositionForWrite();
  dir_count = NumLumps();

  if (HAS_BIT(config.debug, DEBUG_WAD))
  {
    PrintLine(LOG_DEBUG, "[%s] dir_start:%zu  dir_count:%zu", __func__, dir_start, dir_count);
  }

  for (size_t k = 0; k < dir_count; k++)
  {
    Lump_c *lump = directory[k];
    SYS_ASSERT(lump);

    raw_wad_entry_t entry;

    lump->MakeEntry(&entry);

    if (fwrite(&entry, sizeof(entry), 1, fp) != 1)
    {
      PrintLine(LOG_ERROR, "Error writing WAD directory.");
    }
  }

  fflush(fp);

  total_size = ftello(fp);

  if (HAS_BIT(config.debug, DEBUG_WAD))
  {
    PrintLine(LOG_DEBUG, "[%s] total_size: %zu", __func__, total_size);
  }

  if (total_size < 0)
  {
    PrintLine(LOG_ERROR, "Error determining WAD size.");
  }

  // update header at start of file

  rewind(fp);

  raw_wad_header_t header;

  memcpy(header.ident, (kind == 'I') ? "IWAD" : "PWAD", 4);

  header.dir_start = GetLittleEndian(static_cast<uint32_t>(dir_start));
  header.num_entries = GetLittleEndian(static_cast<uint32_t>(dir_count));

  if (fwrite(&header, sizeof(header), 1, fp) != 1)
  {
    PrintLine(LOG_ERROR, "Error writing WAD header.");
  }

  fflush(fp);
}
