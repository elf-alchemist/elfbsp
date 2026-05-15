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
#include "core.hpp"
#include "local.hpp"

#include <cstdio>
#include <stdexcept>

//------------------------------------------------------------------------
//  Utilities
//------------------------------------------------------------------------

static size_t WhatLevelPart(const char *name)
{
  if (StringCaseCmp(name, "THINGS") == 0) return 1;
  if (StringCaseCmp(name, "LINEDEFS") == 0) return 2;
  if (StringCaseCmp(name, "SIDEDEFS") == 0) return 3;
  if (StringCaseCmp(name, "VERTEXES") == 0) return 4;
  if (StringCaseCmp(name, "SECTORS") == 0) return 5;

  return 0;
}

static bool IsLevelLump(const char *name)
{
  if (StringCaseCmp(name, "SEGS") == 0) return true;
  if (StringCaseCmp(name, "SSECTORS") == 0) return true;
  if (StringCaseCmp(name, "NODES") == 0) return true;
  if (StringCaseCmp(name, "REJECT") == 0) return true;
  if (StringCaseCmp(name, "BLOCKMAP") == 0) return true;
  if (StringCaseCmp(name, "BEHAVIOR") == 0) return true;
  if (StringCaseCmp(name, "SCRIPTS") == 0) return true;
  if (StringCaseCmp(name, "LEAFS") == 0) return true;
  if (StringCaseCmp(name, "LIGHTS") == 0) return true;
  if (StringCaseCmp(name, "MACROS") == 0) return true;

  return WhatLevelPart(name) != 0;
}

//------------------------------------------------------------------------
//  WAD Reading Interface
//------------------------------------------------------------------------

WadIO::WadIO(const char *filename) : read_file(nullptr), read_directory(), read_levels()
{
  this->read_file = fopen(filename, "rb");
  if (this->read_file == NULL)
  {
    throw std::runtime_error("ERROR: Could not open input file");
  }

  this->SafeRead(&this->read_header, sizeof(this->read_header));
  if (StringCaseCmpMax(this->read_header.ident, "PWAD", 4) != 0 || StringCaseCmpMax(this->read_header.ident, "IWAD", 4) != 0)
  {
    fclose(read_file);
    read_file = NULL;
    throw std::runtime_error("ERROR: Input file is not a wad");
  }

  this->read_header.num_entries = GetLittleEndian(this->read_header.num_entries);
  this->read_header.dir_start = GetLittleEndian(this->read_header.dir_start);

  if (fseek(read_file, this->read_header.dir_start, SEEK_SET))
  {
    throw std::runtime_error("ERROR: Could not read wad directory");
  }

  this->read_directory.resize(this->read_header.num_entries);
  this->SafeRead(this->read_directory.data(), this->read_header.num_entries * sizeof(WadLump));

  for (size_t i = 0; i < this->read_header.num_entries; ++i)
  {
    this->read_directory[i].pos = GetLittleEndian(this->read_directory[i].pos);
    this->read_directory[i].size = GetLittleEndian(this->read_directory[i].size);
  }
}

WadIO::~WadIO(void)
{
  fclose(read_file);
  this->read_directory.clear();
}

// call only after seeking
void WadIO::SafeRead(void *buffer, size_t size)
{
  if (fread(buffer, 1, size, this->read_file) != size)
  {
    PrintLine(LOG_ERROR, "Failed to read");
  }
}

size_t WadIO::NumLumps(void) const
{
  return this->read_directory.size();
}

size_t WadIO::LevelCount(void) const
{
  return this->read_levels.size();
}

const char *WadIO::LumpName(size_t lump_index) const
{
  static char name[9];
  strncpy(name, this->read_directory[lump_index].name, 8);
  name[8] = 0;
  return name;
}

size_t WadIO::LevelHeader(size_t lev_num)
{
  SYS_ASSERT(lev_num < LevelCount());
  return this->read_levels[lev_num];
}

size_t WadIO::LevelLastLump(size_t lev_num)
{
  size_t start = this->LevelHeader(lev_num);
  size_t count = 1;

  // UDMF level?
  if (StringCaseCmp(read_directory[start + 1].name, "TEXTMAP"))
  {
    while (count < MAX_LUMPS_IN_A_LEVEL && start + count < this->NumLumps())
    {
      if (StringCaseCmp(read_directory[start + count].name, "ENDMAP"))
      {
        count++;
        break;
      }

      count++;
    }
  }
  else // standard DOOM or HEXEN format
  {
    while (count < MAX_LUMPS_IN_A_LEVEL && start + count < this->NumLumps() && IsLevelLump(read_directory[start + count].name))
    {
      count++;
    }
  }

  return start + count - 1;
}

// returns a lump index, NO_INDEX if not found
size_t WadIO::LevelLookupLump(size_t lev_num, const char *name)
{
  size_t start = this->LevelHeader(lev_num);
  size_t finish = this->LevelLastLump(lev_num);

  for (size_t k = start + 1; k <= finish; k++)
  {
    SYS_ASSERT(k < this->NumLumps());

    if (StringCaseCmp(read_directory[k].name, name) == 0)
    {
      return k;
    }
  }

  return NO_INDEX; // not found
}

map_format_e WadIO::LevelFormat(size_t lev_num)
{
  // UDMF maps can contain BEHAVIOR or MACROS
  // check exclusively TEXTMAP
  if (this->LevelLookupLump(lev_num, "TEXTMAP") != NO_INDEX)
  {
    return MapFormat_UDMF;
  }

  if (this->LevelLookupLump(lev_num, "BEHAVIOR") != NO_INDEX)
  {
    return MapFormat_Hexen;
  }

  if (this->LevelLookupLump(lev_num, "LIGHTS") != NO_INDEX && this->LevelLookupLump(lev_num, "MACROS") != NO_INDEX)
  {
    return MapFormat_Doom64;
  }

  return MapFormat_Doom;
}

void WadIO::DetectLevels(void)
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
    if (WhatLevelPart(read_directory[k].name) != 0)
    {
      continue;
    }

    // check for UDMF levels
    if (StringCaseCmp(read_directory[k + 1].name, "TEXTMAP"))
    {
      this->read_levels.push_back(k);
      if (HAS_BIT(config.debug, DEBUG_WAD))
      {
        PrintLine(LOG_DEBUG, "[%s] Detected level : %s (UDMF)", __func__, read_directory[k].name);
      }

      continue;
    }

    // check whether the next four lumps are level lumps
    for (size_t i = 1; i <= 4; i++)
    {
      if (k + i >= this->NumLumps())
      {
        break;
      }

      size_t part = WhatLevelPart(read_directory[k + i].name);

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
      read_levels.push_back(k);

      if (HAS_BIT(config.debug, DEBUG_WAD))
      {
        PrintLine(LOG_DEBUG, "[%s] Detected level : %s", __func__, read_directory[k].name);
      }
    }
  }
}

template <typename T>
std::vector<T> WadIO::ReadLump(size_t index)
{
  std::vector<T> lump;
  if (index >= this->read_header.num_entries)
  {
    return lump;
  }
  if (fseek(this->read_file, this->read_directory[index].pos, SEEK_SET))
  {
    throw std::runtime_error("ERROR: Failed to seek");
  }
  size_t size = this->read_directory[index].size / sizeof(T);
  lump.resize(size);
  this->SafeRead(lump.data(), size * sizeof(T));
  return lump;
}

template <typename T>
std::vector<T> WadIO::ReadMapLump(level_t &level, const char *name)
{
  return ReadLump<T>(this->LevelLookupLump(level.level_num, name));
}

//------------------------------------------------------------------------
//  WAD Writing Interface
//------------------------------------------------------------------------

void WadIO::SafeWrite(const void *buffer, size_t size)
{
  if (fwrite(buffer, 1, size, write_file) != size)
  {
    fclose(this->write_file);
    this->write_file = NULL;
    throw std::runtime_error(
        "ERROR: Failed to write. Check that this directory is writable and that you have enough free disk space.");
  }
}

void WadIO::CreateEmptyLump(const char *name)
{
  WadLump lump;
  strncpy(lump.name, name, 8);
  lump.pos = GetLittleEndian(static_cast<uint32_t>(ftell(this->write_file)));
  lump.size = 0;
  this->write_directory.push_back(lump);
}

void WadIO::StartWritingLump(const char *name)
{
  CreateEmptyLump(name);
}

void WadIO::WriteLump(const char *name, const void *data, size_t size)
{
  WadLump lump;
  strncpy(lump.name, name, 8);
  lump.pos = GetLittleEndian(static_cast<uint32_t>(ftell(this->write_file)));
  lump.size = IndexToInt(size);
  this->write_directory.push_back(lump);
  this->SafeWrite(data, size);
}

void WadIO::CopyLump(size_t index)
{
  auto lump = this->ReadLump<byte>(index);
  WriteLump(this->LumpName(index), lump.data(), lump.size());
}

void WadIO::AddToLump(const void *data, size_t len)
{
  size_t current = this->write_directory.size() - 1;
  this->SafeWrite(data, len);
  this->write_directory[current].size += len;
}

WadIO &WadIO::operator<<(uint8_t value)
{
  AddToLump(&value, sizeof(uint8_t));
  return *this;
}

WadIO &WadIO::operator<<(uint16_t value)
{
  value = GetLittleEndian(value);
  AddToLump(reinterpret_cast<byte *>(&value), sizeof(uint16_t));
  return *this;
}

WadIO &WadIO::operator<<(uint32_t value)
{
  value = GetLittleEndian(value);
  AddToLump(reinterpret_cast<byte *>(&value), sizeof(uint32_t));
  return *this;
}

WadIO &WadIO::operator<<(int16_t value)
{
  value = GetLittleEndian(value);
  AddToLump(reinterpret_cast<byte *>(&value), sizeof(int16_t));
  return *this;
}

WadIO &WadIO::operator<<(fixed_t value)
{
  value = GetLittleEndian(value);
  AddToLump(reinterpret_cast<byte *>(&value), sizeof(fixed_t));
  return *this;
}
