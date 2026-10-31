//------------------------------------------------------------------------------
//
//  ELFBSP -- WAD Reading / Writing
//
//------------------------------------------------------------------------------
//
//  Copyright 2025-2026 Guilherme Miranda
//  Copyright 2001-2018 Andrew Apted
//  Copyright 2002-2006 Marisa Heit
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

#pragma once

#include "core.hpp"
#include <vector>

//------------------------------------------------------------------------
// WAD STRUCTURES
//------------------------------------------------------------------------

// wad header
struct WadHeader
{
  char ident[4];
  int32_t num_entries;
  int32_t dir_start;
} PACKEDATTR;

// directory entry
struct WadLump
{
  int32_t pos;
  int32_t size;
  char name[8];
} PACKEDATTR;

static_size(WadHeader, 12);
static_size(WadLump, 16);

//
// File handling
//

struct WadIO
{
  // Read data
  FILE *read_file;
  WadHeader read_header;
  std::vector<WadLump> read_directory;

  // Write data
  FILE *write_file;
  WadHeader write_header;
  std::vector<WadLump> write_directory;
  bool zlib_init = false;
  zng_stream zout_stream;
  byte zout_buffer[8192];

  // Init/die
  WadIO(std::string in_file, std::string out_file);
  ~WadIO(void);

  // Read operations
  void SafeRead(void *buffer, size_t size);
  size_t NumLumps(void) const;
  const char *LumpName(size_t lump_index) const;
  bool IsLevelLump(size_t lump_index) const;
  bool IsLevelTextmapFormat(size_t lump_index) const;
  bool IsLevel(size_t lump_index) const;
  MapFormat LevelFormat(size_t lump_index) const;
  size_t LevelLastLump(size_t lump_index) const;
  size_t LevelLookupLump(size_t lump_index, std::string_view name) const;
  bool LevelHasLump(size_t lump_index, std::string_view name) const;
  template <typename T>
  std::vector<T> ReadLump(size_t lump_index);
  template <typename T>
  std::vector<T> ReadLevelLump(size_t lump_index, std::string_view name);

  // Write operations
  void SafeWrite(const void *buffer, size_t size);
  void CreateEmptyLump(std::string_view name);
  void WriteLump(std::string_view name, const void *data, size_t size);
  void CopyLump(size_t lump);
  void LevelCopyLump(size_t lump, std::string_view name);

  // Partial writing
  void StartWritingLump(std::string_view name);
  void AddToLump(const void *data, size_t len);
  WadIO &operator<<(uint8_t value);
  WadIO &operator<<(uint16_t value);
  WadIO &operator<<(uint32_t value);
  WadIO &operator<<(int16_t value);
  WadIO &operator<<(fixed_t value);

  // Zlib compression support
  void Begin_Zlib(void);
  void AddToLumpZ(const void *data, size_t length);
  void Finish_Zlib(void);
};

//
// fuck C++ start
//
template <typename T>
std::vector<T> WadIO::ReadLump(size_t lump_index)
{
  std::vector<T> lump;
  if (lump_index >= this->read_directory.size())
  {
    return lump;
  }
  if (fseek(this->read_file, this->read_directory[lump_index].pos, SEEK_SET))
  {
    throw RuntimeException("Failed to seek");
  }
  size_t size = this->read_directory[lump_index].size / sizeof(T);
  lump.resize(size);
  this->SafeRead(lump.data(), size * sizeof(T));
  return lump;
}

template <typename T>
std::vector<T> WadIO::ReadLevelLump(size_t lump_index, std::string_view name)
{
  return ReadLump<T>(this->LevelLookupLump(lump_index, name));
}

//
// fuck C++ end
//
