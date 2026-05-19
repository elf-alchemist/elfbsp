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

WadIO::WadIO(const char *filename)
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

const char *WadIO::LumpName(size_t lump_index) const
{
  static char name[9];
  strncpy(name, this->read_directory[lump_index].name, 8);
  name[8] = 0;
  return name;
}

size_t WadIO::LevelLastLump(size_t lump_index)
{
  size_t count = 1;

  // UDMF level?
  if (StringCaseCmp(read_directory[lump_index + 1].name, "TEXTMAP"))
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
  return ReadLump<T>(this->LevelLookupLump(level.level_header_lump_index, name));
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
