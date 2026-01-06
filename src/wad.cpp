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

#include <algorithm>
#include <cstdint>

//------------------------------------------------------------------------
//  LUMP Handling
//------------------------------------------------------------------------

Lump_c::Lump_c(Wad_file *_par, const char *_name, size_t _start, size_t _len)
    : parent(_par), lumpname(), l_start(_start), l_length(_len)
{
  // ensure lump name is uppercase
  Rename(_name);
}

Lump_c::Lump_c(Wad_file *_par, const raw_wad_entry_t *entry) : parent(_par), lumpname()
{
  // handle the entry name, which can lack a terminating NUL
  char buffer[10];
  strncpy(buffer, entry->name, 8);
  buffer[8] = 0;

  // ensure lump name is uppercase
  Rename(buffer);

  l_start = GetLittleEndian(entry->pos);
  l_length = GetLittleEndian(entry->size);

  if constexpr (DEBUG_WAD)
  {
    Debug("new lump '%s' @ %zu len:%zu\n", Name(), l_start, l_length);
  }
}

Lump_c::~Lump_c(void)
{
  // nothing needed
}

void Lump_c::MakeEntry(raw_wad_entry_t *entry)
{
  // do a dance to avoid a compiler warning from strncpy(), *sigh*
  memset(entry->name, 0, 8);
  memcpy(entry->name, lumpname.c_str(), lumpname.size());

  entry->pos = GetLittleEndian((uint32_t)l_start);
  entry->size = GetLittleEndian((uint32_t)l_length);
}

bool Lump_c::Match(const char *s) const
{
  return (0 == StringCaseCmp(lumpname.c_str(), s));
}

void Lump_c::Rename(const char *new_name)
{
  // ensure lump name is uppercase
  lumpname.clear();

  for (const char *s = new_name; *s != 0; s++)
  {
    lumpname.push_back(static_cast<char>(toupper(*s)));
  }
}

bool Lump_c::Seek(int offset)
{
  SYS_ASSERT(offset >= 0);

  return (fseek(parent->fp, (int32_t)l_start + offset, SEEK_SET) == 0);
}

bool Lump_c::Read(void *data, size_t len)
{
  SYS_ASSERT(data && len > 0);

  return (fread(data, len, 1, parent->fp) == 1);
}

bool Lump_c::Write(const void *data, size_t len)
{
  SYS_ASSERT(data && len > 0);

  l_length += len;

  return (fwrite(data, len, 1, parent->fp) == 1);
}

bool Lump_c::Finish(void)
{
  if (l_length == 0)
  {
    l_start = 0;
  }

  return parent->FinishLump(l_length);
}

//------------------------------------------------------------------------
//  WAD Reading Interface
//------------------------------------------------------------------------

Wad_file::Wad_file(const char *_name, char _mode, FILE *_fp)
    : filename(_name), mode(_mode), fp(_fp), kind('P'), total_size(0), directory(), dir_start(0), dir_count(0), levels(),
      patches(), sprites(), flats(), tx_tex(), begun_write(false), insert_point(NO_INDEX)
{
  // nothing needed
}

Wad_file::~Wad_file(void)
{
  if constexpr (DEBUG_WAD)
  {
    Debug("Closing WAD file: %s\n", filename.c_str());
  }

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

  if constexpr (DEBUG_WAD)
  {
    Debug("Opening WAD file: %s\n", filename);
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
      if constexpr (DEBUG_WAD)
      {
        Debug("Open r/w failed, trying again in read mode...\n");
      }
      mode = 'r';
      goto retry;
    }

    int what = errno;
    if constexpr (DEBUG_WAD)
    {
      Debug("Open file failed: %s\n", strerror(what));
    }
    return nullptr;
  }

  Wad_file *w = new Wad_file(filename, mode, fp);

  // determine total size (seek to end)
  if (fseek(fp, 0, SEEK_END) != 0)
  {
    FatalError("Error determining WAD size.\n");
  }

  w->total_size = (size_t)ftell(fp);

  if constexpr (DEBUG_WAD)
  {
    Debug("total_size = %zu\n", w->total_size);
  }

  if ((int64_t)w->total_size < 0)
  {
    FatalError("Error determining WAD size.\n");
  }

  w->ReadDirectory();
  w->DetectLevels();
  w->ProcessNamespaces();

  return w;
}

Wad_file *Wad_file::Create(const char *filename, char mode)
{
  if constexpr (DEBUG_WAD)
  {
    Debug("Creating new WAD file: %s\n", filename);
  }

  FILE *fp = fopen(filename, "w+b");
  if (!fp)
  {
    return nullptr;
  }

  Wad_file *w = new Wad_file(filename, mode, fp);

  // write out base header
  raw_wad_header_t header;

  memset(&header, 0, raw_wad_header_size);
  memcpy(header.ident, "PWAD", 4);

  fwrite(&header, raw_wad_header_size, 1, fp);
  fflush(fp);

  w->total_size = raw_wad_header_size;

  return w;
}

bool Wad_file::Validate(const char *filename)
{
  FILE *fp = fopen(filename, "rb");

  if (!fp)
  {
    return false;
  }

  raw_wad_header_t header;

  if (fread(&header, raw_wad_header_size, 1, fp) != 1)
  {
    fclose(fp);
    return false;
  }

  if (!(header.ident[1] == 'W' && header.ident[2] == 'A' && header.ident[3] == 'D'))
  {
    fclose(fp);
    return false;
  }

  fclose(fp);

  return true; // OK
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

Lump_c *Wad_file::FindLump(const char *name)
{
  for (size_t k = 0; k < NumLumps(); k++)
  {
    if (directory[k]->Match(name))
    {
      return directory[k];
    }
  }

  return nullptr; // not found
}

size_t Wad_file::FindLumpNum(const char *name)
{
  for (size_t k = 0; k < NumLumps(); k++)
  {
    if (directory[k]->Match(name))
    {
      return k;
    }
  }

  return NO_INDEX; // not found
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

size_t Wad_file::LevelFind(const char *name)
{
  for (size_t k = 0; k < levels.size(); k++)
  {
    size_t index = levels[k];

    SYS_ASSERT(index < NumLumps());
    SYS_ASSERT(directory[index]);

    if (directory[index]->Match(name))
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

size_t Wad_file::LevelFindByNumber(int32_t number)
{
  // sanity check
  if (number <= 0 || number > 99)
  {
    return NO_INDEX;
  }

  char buffer[10];
  size_t index;

  // try MAP## first
  M_snprintf(buffer, sizeof(buffer), "MAP%02d", number);

  index = LevelFind(buffer);
  if (index != NO_INDEX)
  {
    return index;
  }

  // otherwise try E#M#
  div_t exmy = div(number, 10);
  M_snprintf(buffer, sizeof(buffer), "E%dM%d", std::max(1, exmy.quot), exmy.rem);

  index = LevelFind(buffer);
  if (index != NO_INDEX)
  {
    return index;
  }

  return NO_INDEX; // not found
}

size_t Wad_file::LevelFindFirst(void)
{
  if (levels.size() > 0)
  {
    return 0;
  }
  else
  {
    return NO_INDEX; // none
  }
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

Lump_c *Wad_file::FindLumpInNamespace(const char *name, char group)
{
  switch (group)
  {
    case 'P':
      for (size_t k = 0; k < patches.size(); k++)
      {
        if (directory[patches[k]]->Match(name))
        {
          return directory[patches[k]];
        }
      }
      break;

    case 'S':
      for (size_t k = 0; k < sprites.size(); k++)
      {
        if (directory[sprites[k]]->Match(name))
        {
          return directory[sprites[k]];
        }
      }
      break;

    case 'F':
      for (size_t k = 0; k < flats.size(); k++)
      {
        if (directory[flats[k]]->Match(name))
        {
          return directory[flats[k]];
        }
      }
      break;

    default:
      FatalError("FindLumpInNamespace: bad group '%c'\n", group);
  }

  return nullptr; // not found!
}

void Wad_file::ReadDirectory(void)
{
  // WISH: no fatal errors

  rewind(fp);

  raw_wad_header_t header;

  if (fread(&header, raw_wad_header_size, 1, fp) != 1)
  {
    FatalError("Error reading WAD header.\n");
  }

  // WISH: check ident for PWAD or IWAD

  kind = header.ident[0];

  dir_start = GetLittleEndian(header.dir_start);
  dir_count = GetLittleEndian(header.num_entries);

  if (dir_count > 32000)
  {
    FatalError("Bad WAD header, too many entries (%zu)\n", dir_count);
  }

  if (fseek(fp, (int32_t)dir_start, SEEK_SET) != 0)
  {
    FatalError("Error seeking to WAD directory.\n");
  }

  for (size_t i = 0; i < dir_count; i++)
  {
    raw_wad_entry_t entry;

    if (fread(&entry, raw_wad_entry_size, 1, fp) != 1)
    {
      FatalError("Error reading WAD directory.\n");
    }

    Lump_c *lump = new Lump_c(this, &entry);

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
      if constexpr (DEBUG_WAD)
      {
        Debug("Detected level : %s (UDMF)\n", directory[k]->Name());
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

      if constexpr (DEBUG_WAD)
      {
        Debug("Detected level : %s\n", directory[k]->Name());
      }
    }
  }

  // sort levels into alphabetical order
  SortLevels();
}

void Wad_file::SortLevels(void)
{
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
        if constexpr (DEBUG_WAD)
        {
          Debug("missing %c_END marker.\n", active);
        }
      }

      active = 'P';
      continue;
    }
    else if (StringCaseCmp(name, "P_END") == 0 || StringCaseCmp(name, "PP_END") == 0)
    {
      if (active != 'P')
      {
        if constexpr (DEBUG_WAD)
        {
          Debug("stray P_END marker found.\n");
        }
      }

      active = 0;
      continue;
    }

    if (StringCaseCmp(name, "S_START") == 0 || StringCaseCmp(name, "SS_START") == 0)
    {
      if (active && active != 'S')
      {
        if constexpr (DEBUG_WAD)
        {
          Debug("missing %c_END marker.\n", active);
        }
      }

      active = 'S';
      continue;
    }
    else if (StringCaseCmp(name, "S_END") == 0 || StringCaseCmp(name, "SS_END") == 0)
    {
      if (active != 'S')
      {
        if constexpr (DEBUG_WAD)
        {
          Debug("stray S_END marker found.\n");
        }
      }

      active = 0;
      continue;
    }

    if (StringCaseCmp(name, "F_START") == 0 || StringCaseCmp(name, "FF_START") == 0)
    {
      if (active && active != 'F')
      {
        if constexpr (DEBUG_WAD)
        {
          Debug("missing %c_END marker.\n", active);
        }
      }

      active = 'F';
      continue;
    }
    else if (StringCaseCmp(name, "F_END") == 0 || StringCaseCmp(name, "FF_END") == 0)
    {
      if (active != 'F')
      {
        if constexpr (DEBUG_WAD)
        {
          Debug("stray F_END marker found.\n");
        }
      }

      active = 0;
      continue;
    }

    if (StringCaseCmp(name, "TX_START") == 0)
    {
      if (active && active != 'T')
      {
        if constexpr (DEBUG_WAD)
        {
          Debug("missing %c_END marker.\n", active);
        }
      }

      active = 'T';
      continue;
    }
    else if (StringCaseCmp(name, "TX_END") == 0)
    {
      if (active != 'T')
      {
        if constexpr (DEBUG_WAD)
        {
          Debug("stray TX_END marker found.\n");
        }
      }

      active = 0;
      continue;
    }

    if (active)
    {
      if (directory[k]->Length() == 0)
      {
        if constexpr (DEBUG_WAD)
        {
          Debug("skipping empty lump %s in %c_START\n", name, active);
        }
        continue;
      }

      if constexpr (DEBUG_WAD)
      {
        Debug("Namespace %c lump : %s\n", active, name);
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
          FatalError("ProcessNamespaces: active = 0x%02x\n", (int)active);
      }
    }
  }

  if constexpr (DEBUG_WAD)
  {
    if (active)
    {
      Debug("Missing %c_END marker (at EOF)\n", active);
    }
  }
}

bool Wad_file::WasExternallyModified(void)
{
  // this method is an unused stub
  return false;
}

//------------------------------------------------------------------------
//  WAD Writing Interface
//------------------------------------------------------------------------

void Wad_file::BeginWrite(void)
{
  if (mode == 'r')
  {
    FatalError("Wad_file::BeginWrite() called on read-only file\n");
  }

  if (begun_write)
  {
    FatalError("Wad_file::BeginWrite() called again without EndWrite()\n");
  }

  // put the size into a quantum state
  total_size = 0;
  begun_write = true;
}

void Wad_file::EndWrite(void)
{
  if (!begun_write)
  {
    FatalError("Wad_file::EndWrite() called without BeginWrite()\n");
  }

  begun_write = false;

  WriteDirectory();

  // reset the insertion point
  insert_point = NO_INDEX;
}

void Wad_file::RenameLump(size_t index, const char *new_name)
{
  SYS_ASSERT(begun_write);
  SYS_ASSERT(index < NumLumps());

  Lump_c *lump = directory[index];
  SYS_ASSERT(lump);

  lump->Rename(new_name);
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

  directory.resize(directory.size() - (size_t)count);

  // fix various arrays containing lump indices
  FixGroup(levels, index, 0, count);
  FixGroup(patches, index, 0, count);
  FixGroup(sprites, index, 0, count);
  FixGroup(flats, index, 0, count);
  FixGroup(tx_tex, index, 0, count);

  // reset the insertion point
  insert_point = NO_INDEX;
}

void Wad_file::RemoveLevel(size_t lev_num)
{
  SYS_ASSERT(begun_write);
  SYS_ASSERT(lev_num < LevelCount());

  size_t start = LevelHeader(lev_num);
  size_t finish = LevelLastLump(lev_num);

  // NOTE: FixGroup() will remove the entry in levels[]
  RemoveLumps(start, finish - start + 1);
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

  Lump_c *lump = new Lump_c(this, name, start, 0);

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

    directory.insert(directory.begin() + (int32_t)insert_point, lump);

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

Lump_c *Wad_file::AddLevel(const char *name, size_t max_size, size_t *lev_num)
{
  size_t actual_point = insert_point;

  if (actual_point > NumLumps())
  {
    actual_point = NumLumps();
  }

  Lump_c *lump = AddLump(name, max_size);

  if (lev_num)
  {
    *lev_num = levels.size();
  }

  levels.push_back(actual_point);

  return lump;
}

void Wad_file::InsertPoint(size_t index)
{
  // this is validated on usage
  insert_point = index;
}

size_t Wad_file::HighWaterMark(void)
{
  size_t offset = raw_wad_header_size;

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

  std::sort(sorted_dir.begin(), sorted_dir.end(), Lump_c::offset_CMP_pred());

  size_t offset = raw_wad_header_size;

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
  size_t want_pos;

  if (max_size == NO_INDEX)
  {
    want_pos = HighWaterMark();
  }
  else
  {
    want_pos = FindFreeSpace(max_size);
  }

  // determine if position is past end of file
  // (difference should only be a few bytes)
  //
  // Note: doing this for every new lump may be a little expensive,
  //       but trying to optimise it away will just make the code
  //       needlessly complex and hard to follow.

  if (fseek(fp, 0, SEEK_END) < 0)
  {
    FatalError("Error seeking to new write position.\n");
  }

  total_size = (size_t)ftell(fp);

  if ((int32_t)total_size < 0)
  {
    FatalError("Error seeking to new write position.\n");
  }

  if (want_pos > total_size)
  {
    SYS_ASSERT(want_pos < total_size + 8);

    WritePadding(want_pos - total_size);
  }
  else if (want_pos == total_size)
  {
    /* ready to write */
  }
  else
  {
    if (fseek(fp, (int32_t)want_pos, SEEK_SET) < 0)
    {
      FatalError("Error seeking to new write position.\n");
    }
  }

  if constexpr (DEBUG_WAD)
  {
    Debug("POSITION FOR WRITE: %zu  (total_size %zu)\n", want_pos, total_size);
  }

  return want_pos;
}

bool Wad_file::FinishLump(size_t final_size)
{
  fflush(fp);

  // sanity check
  if (final_size > begun_max_size)
  {
    FatalError("Internal Error: wrote too much in lump (%zu > %zu)\n", final_size, begun_max_size);
  }

  size_t pos = (size_t)ftell(fp);

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

  if constexpr (DEBUG_WAD)
  {
    Debug("WriteDirectory...\n");
    Debug("dir_start:%zu  dir_count:%zu\n", dir_start, dir_count);
  }

  for (size_t k = 0; k < dir_count; k++)
  {
    Lump_c *lump = directory[k];
    SYS_ASSERT(lump);

    raw_wad_entry_t entry;

    lump->MakeEntry(&entry);

    if (fwrite(&entry, raw_wad_entry_size, 1, fp) != 1)
    {
      FatalError("Error writing WAD directory.\n");
    }
  }

  fflush(fp);

  total_size = (size_t)ftell(fp);

  if constexpr (DEBUG_WAD)
  {
    Debug("total_size: %zu\n", total_size);
  }

  if ((int32_t)total_size < 0)
  {
    FatalError("Error determining WAD size.\n");
  }

  // update header at start of file

  rewind(fp);

  raw_wad_header_t header;

  memcpy(header.ident, (kind == 'I') ? "IWAD" : "PWAD", 4);

  header.dir_start = GetLittleEndian((uint32_t)dir_start);
  header.num_entries = GetLittleEndian((uint32_t)dir_count);

  if (fwrite(&header, raw_wad_entry_size, 1, fp) != 1)
  {
    FatalError("Error writing WAD header.\n");
  }

  fflush(fp);
}

bool Wad_file::Backup(const char *new_filename)
{
  fflush(fp);

  return FileCopy(PathName(), new_filename);
}
