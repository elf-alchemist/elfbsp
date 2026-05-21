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

WadIO::WadIO(std::string in_file, std::string out_file)
{
  // Input stuff
  this->read_file = fopen(in_file.c_str(), "rb");
  if (this->read_file == NULL)
  {
    throw std::runtime_error("ERROR: Could not open input file");
  }

  this->SafeRead(&this->read_header, sizeof(this->read_header));
  if (this->read_header.ident[0] != 'I'    // check for IWADs (base game resources)
      && this->read_header.ident[0] != 'P' // or PWADs (additional/modded resources)
      && this->read_header.ident[1] != 'W' //
      && this->read_header.ident[2] != 'A' //
      && this->read_header.ident[3] != 'D')
  {
    fclose(this->read_file);
    this->read_file = NULL;
    throw std::runtime_error("ERROR: Input file is not a wad");
  }

  auto num_entries = GetLittleEndian(this->read_header.num_entries);
  auto dir_start = GetLittleEndian(this->read_header.dir_start);

  if (fseek(this->read_file, dir_start, SEEK_SET))
  {
    throw std::runtime_error("ERROR: Could not read wad directory");
  }

  this->read_directory.resize(num_entries);
  this->SafeRead(this->read_directory.data(), this->read_directory.size() * sizeof(WadLump));

  for (size_t i = 0; i < this->read_directory.size(); ++i)
  {
    this->read_directory[i].pos = GetLittleEndian(this->read_directory[i].pos);
    this->read_directory[i].size = GetLittleEndian(this->read_directory[i].size);
  }

  // Output stuff
  this->write_file = fopen(out_file.c_str(), "wb");
  if (this->write_file == NULL)
  {
    throw std::runtime_error("ERROR: Could not open output file");
  }

  this->write_header.ident[0] = this->read_header.ident[0];
  this->write_header.ident[1] = this->read_header.ident[1];
  this->write_header.ident[2] = this->read_header.ident[2];
  this->write_header.ident[3] = this->read_header.ident[3];
  this->write_header.num_entries = 0;
  this->write_header.dir_start = 0;

  this->SafeWrite(&this->write_header, sizeof(this->write_header));
}

WadIO::~WadIO(void)
{
  // Input stuff
  fclose(this->read_file);
  this->read_directory.clear();

  // Output stuff
  this->SafeWrite(this->write_directory.data(), sizeof(WadLump) * this->write_directory.size());

  uint32_t num_entries = GetLittleEndian(IndexToInt(this->write_directory.size()));
  uint32_t dir_start = GetLittleEndian(static_cast<uint32_t>(ftell(this->write_file)));

  fseek(this->write_file, sizeof(char[4]), SEEK_SET);
  this->SafeWrite(&num_entries, sizeof(num_entries));
  this->SafeWrite(&dir_start, sizeof(dir_start));
  fclose(this->write_file);

  this->write_file = NULL;
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

const char *WadIO::LumpName(size_t lump_index) const
{
  static char name[9];
  strncpy(name, this->read_directory[lump_index].name, 8);
  name[8] = 0;
  return name;
}

bool WadIO::IsLevel(size_t k)
{
  // Determine what lumps in the wad are level markers, based on the
  // lumps which follow it.  Store the result in the 'levels' vector.
  // The test here is rather lax, since wads exist with a non-standard
  // ordering of level lumps.
  size_t part_mask = 0;
  size_t part_count = 0;

  // Ignore non-header map lumps
  // Fixes sliding window bug on single-level WADs
  if (WhatLevelPart(read_directory[k].name) != 0)
  {
    return false;
  }

  // check for UDMF levels
  if (StringCaseCmp(read_directory[k + 1].name, "TEXTMAP"))
  {
    if (HAS_BIT(config.debug, DEBUG_WAD))
    {
      PrintLine(LOG_DEBUG, "[%s] Detected level : %s (UDMF)", __func__, read_directory[k].name);
    }
    return true;
  }

  // check whether the next four lumps are level lumps
  for (size_t i = 1; i <= 4; i++)
  {
    if (k + i >= NumLumps())
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
    if (HAS_BIT(config.debug, DEBUG_WAD))
    {
      PrintLine(LOG_DEBUG, "[%s] Detected level : %s", __func__, read_directory[k].name);
    }
    return true;
  }

  return false;
}

map_format_e WadIO::LevelFormat(size_t lump_index)
{
  // UDMF maps can contain BEHAVIOR or MACROS
  // check exclusively TEXTMAP
  if (this->LevelLookupLump(lump_index, "TEXTMAP") != NO_INDEX)
  {
    return MapFormat_UDMF;
  }

  if (this->LevelLookupLump(lump_index, "BEHAVIOR") != NO_INDEX)
  {
    return MapFormat_Hexen;
  }

  if (this->LevelLookupLump(lump_index, "LIGHTS") != NO_INDEX && this->LevelLookupLump(lump_index, "MACROS") != NO_INDEX)
  {
    return MapFormat_Doom64;
  }

  return MapFormat_Doom;
}

size_t WadIO::LevelLastLump(size_t lump_index)
{
  size_t count = 1;

  // UDMF level?
  if (lump_index + 1 < this->NumLumps() && StringCaseCmp(read_directory[lump_index + 1].name, "TEXTMAP"))
  {
    while (count < MAX_LUMPS_IN_A_LEVEL && lump_index + count < this->NumLumps())
    {
      if (StringCaseCmp(read_directory[lump_index + count].name, "ENDMAP"))
      {
        count++;
        break;
      }

      count++;
    }
  }
  else // standard DOOM or HEXEN format
  {
    while (count < MAX_LUMPS_IN_A_LEVEL && lump_index + count < this->NumLumps()
           && IsLevelLump(read_directory[lump_index + count].name))
    {
      count++;
    }
  }

  return lump_index + count - 1;
}

// returns a lump index, NO_INDEX if not found
size_t WadIO::LevelLookupLump(size_t lump_index, const char *name)
{
  size_t finish = this->LevelLastLump(lump_index);

  for (size_t k = lump_index + 1; k <= finish; k++)
  {
    SYS_ASSERT(k < this->NumLumps());

    if (StringCaseCmp(read_directory[k].name, name) == 0)
    {
      return k;
    }
  }

  return NO_INDEX; // not found
}

//
// Write operations
//
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

//
// Partial writing
//

void WadIO::StartWritingLump(const char *name)
{
  CreateEmptyLump(name);
}

void WadIO::AddToLump(const void *data, size_t len)
{
  size_t current = this->write_directory.size() - 1;
  this->SafeWrite(data, len);
  this->write_directory[current].size += len;
}

WadIO &WadIO::operator<<(uint8_t value)
{
  AddToLumpZ(&value, sizeof(uint8_t));
  return *this;
}

WadIO &WadIO::operator<<(uint16_t value)
{
  value = GetLittleEndian(value);
  AddToLumpZ(reinterpret_cast<byte *>(&value), sizeof(uint16_t));
  return *this;
}

WadIO &WadIO::operator<<(uint32_t value)
{
  value = GetLittleEndian(value);
  AddToLumpZ(reinterpret_cast<byte *>(&value), sizeof(uint32_t));
  return *this;
}

WadIO &WadIO::operator<<(int16_t value)
{
  value = GetLittleEndian(value);
  AddToLumpZ(reinterpret_cast<byte *>(&value), sizeof(int16_t));
  return *this;
}

WadIO &WadIO::operator<<(fixed_t value)
{
  value = GetLittleEndian(value);
  AddToLumpZ(reinterpret_cast<byte *>(&value), sizeof(fixed_t));
  return *this;
}

//
// Zlib compression support
//

void WadIO::Begin_Zlib(void)
{
  this->zlib_init = true;
  this->zout_stream.zalloc = nullptr;
  this->zout_stream.zfree = nullptr;
  this->zout_stream.opaque = nullptr;
  if (Z_OK != zng_deflateInit(&this->zout_stream, Z_DEFAULT_COMPRESSION))
  {
    PrintLine(LOG_ERROR, "ERROR: Trouble starting Zlib compression.");
  }
  this->zout_stream.next_out = this->zout_buffer;
  this->zout_stream.avail_out = sizeof(this->zout_buffer);
}

void WadIO::AddToLumpZ(const void *data, size_t length)
{
  if (!this->zlib_init)
  {
    this->AddToLump(data, length);
    return;
  }

  SYS_ASSERT(length > 0);

  this->zout_stream.next_in = static_cast<const byte *>(data);
  this->zout_stream.avail_in = IndexToInt(length);

  while (this->zout_stream.avail_in > 0)
  {
    if (Z_OK != zng_deflate(&this->zout_stream, Z_NO_FLUSH))
    {
      PrintLine(LOG_ERROR, "ERROR: Trouble Zlib compressing %zu bytes.", length);
    }

    if (this->zout_stream.avail_out == 0)
    {
      this->AddToLump(this->zout_buffer, sizeof(this->zout_buffer));

      this->zout_stream.next_out = this->zout_buffer;
      this->zout_stream.avail_out = sizeof(this->zout_buffer);
    }
  }
}

void WadIO::Finish_Zlib(void)
{
  SYS_ASSERT(this->zout_stream.avail_out > 0)
  size_t left_over;
  this->zlib_init = false;

  this->zout_stream.next_in = Z_NULL;
  this->zout_stream.avail_in = 0;

  while (true)
  {
    int32_t err = zng_deflate(&this->zout_stream, Z_FINISH);

    if (err == Z_STREAM_END) break;

    if (err != Z_OK)
    {
      PrintLine(LOG_ERROR, "ERROR: Trouble finishing Zlib compression.");
    }

    if (this->zout_stream.avail_out == 0)
    {
      this->AddToLump(&this->zout_buffer, sizeof(this->zout_buffer));

      this->zout_stream.next_out = this->zout_buffer;
      this->zout_stream.avail_out = sizeof(this->zout_buffer);
    }
  }

  left_over = sizeof(zout_buffer) - zout_stream.avail_out;

  if (left_over > 0) this->AddToLump(zout_buffer, left_over);

  zng_deflateEnd(&zout_stream);
}
