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

constexpr uint32_t HAS_THINGS = BIT(0);
constexpr uint32_t HAS_LINEDEFS = BIT(1);
constexpr uint32_t HAS_SIDEDEFS = BIT(2);
constexpr uint32_t HAS_VERTEXES = BIT(3);
constexpr uint32_t HAS_SECTORS = BIT(4);

constexpr uint32_t HAS_REQUIRED_LUMPS = HAS_THINGS | HAS_LINEDEFS | HAS_SIDEDEFS | HAS_VERTEXES | HAS_SECTORS;

static uint32_t WhatLevelPart(const std::string_view name)
{
  if (name == "THINGS") return HAS_THINGS;
  if (name == "LINEDEFS") return HAS_LINEDEFS;
  if (name == "SIDEDEFS") return HAS_SIDEDEFS;
  if (name == "VERTEXES") return HAS_VERTEXES;
  if (name == "SECTORS") return HAS_SECTORS;

  return 0;
}

static const std::string_view TEXTMAP = "TEXTMAP";
static const std::string_view ENDMAP = "ENDMAP";

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
  this->SafeRead(this->read_directory.data(), this->NumLumps() * sizeof(WadLump));

  for (size_t i = 0; i < this->NumLumps(); ++i)
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
  uint32_t num_entries = GetLittleEndian(IndexToInt(this->write_directory.size()));
  uint32_t dir_start = GetLittleEndian(static_cast<uint32_t>(ftell(this->write_file)));

  this->SafeWrite(this->write_directory.data(), sizeof(WadLump) * this->write_directory.size());

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
  SYS_ASSERT(lump_index < this->NumLumps());
  static char name[9];
  strncpy(name, this->read_directory[lump_index].name, 8);
  name[8] = 0;
  return name;
}

bool WadIO::IsLevelLump(size_t lump_index) const
{
  SYS_ASSERT(lump_index < this->NumLumps());
  const std::string_view name = this->LumpName(lump_index);
  if (name == "SEGS") return true;
  if (name == "SSECTORS") return true;
  if (name == "NODES") return true;
  if (name == "REJECT") return true;
  if (name == "BLOCKMAP") return true;
  if (name == "BEHAVIOR") return true;
  if (name == "SCRIPTS") return true;
  if (name == "LEAFS") return true;
  if (name == "LIGHTS") return true;
  if (name == "MACROS") return true;

  return WhatLevelPart(name) != 0;
}

bool WadIO::IsLevelTextmapFormat(size_t lump_index) const
{
  if (lump_index + LL_TEXTMAP >= this->NumLumps())
  {
    return false;
  }

  return (TEXTMAP == this->LumpName(lump_index + LL_TEXTMAP));
}

bool WadIO::IsLevel(size_t lump_index) const
{
  SYS_ASSERT(lump_index < this->NumLumps());

  // check for UDMF levels
  if (this->IsLevelTextmapFormat(lump_index))
  {
    if (HAS_BIT(config.debug, DEBUG_WAD))
    {
      PrintLine(LOG_DEBUG, "[%s] Detected level: %s (UDMF) [Index: %zu]", __func__, this->LumpName(lump_index), lump_index);
    }
    return true;
  }

  uint32_t required_lumps = 0;
  for (size_t i = 1; i < MAX_LUMPS_IN_A_LEVEL; ++i)
  {
    // Haven't found all required lumps yet? Out
    if (lump_index + i >= this->NumLumps()) break;
    // Not map lump? Also out
    if (!this->IsLevelLump(lump_index + i)) break;
    // Check if this lump is a required level part
    required_lumps |= WhatLevelPart(this->LumpName(lump_index + i));
  }

  // Have found all required lumps?
  if (HAS_ALL(required_lumps, HAS_REQUIRED_LUMPS))
  {
    if (HAS_BIT(config.debug, DEBUG_WAD))
    {
      map_format_t format = this->LevelFormat(lump_index);
      PrintLine(LOG_DEBUG, "[%s] Detected level: %s (%s) [Index: %zu]", __func__, this->LumpName(lump_index),
                format_name[format].data(), lump_index);
    }
    return true;
  }
  else
  {
    return false;
  }
}

map_format_e WadIO::LevelFormat(size_t lump_index) const
{
  // UDMF maps can contain BEHAVIOR or MACROS
  // check exclusively TEXTMAP
  if (this->IsLevelTextmapFormat(lump_index))
  {
    return MapFormat_UDMF;
  }

  if (this->LevelHasLump(lump_index, "BEHAVIOR"))
  {
    return MapFormat_Hexen;
  }

  if (this->LevelHasLump(lump_index, "LIGHTS") && this->LevelHasLump(lump_index, "MACROS"))
  {
    return MapFormat_Doom64;
  }

  return MapFormat_Doom;
}

size_t WadIO::LevelLastLump(size_t lump_index) const
{
  size_t count = 1;

  // UDMF level?
  if (this->IsLevelTextmapFormat(lump_index))
  {
    while (count < MAX_LUMPS_IN_A_LEVEL              //
           && lump_index + count < this->NumLumps()) //
    {
      if (ENDMAP == this->LumpName(lump_index + count))
      {
        count++;
        break;
      }

      count++;
    }
  }
  else // all other binary formats
  {
    while (count < MAX_LUMPS_IN_A_LEVEL             //
           && lump_index + count < this->NumLumps() //
           && IsLevelLump(lump_index + count))      //
    {
      count++;
    }
  }

  return lump_index + count - 1;
}

// returns a lump index, NO_INDEX if not found
size_t WadIO::LevelLookupLump(size_t lump_index, std::string_view name) const
{
  size_t finish = this->LevelLastLump(lump_index);

  for (size_t k = lump_index + 1; k <= finish; k++)
  {
    SYS_ASSERT(k < this->NumLumps());

    if (name == this->LumpName(k))
    {
      return k;
    }
  }

  return NO_INDEX; // not found
}

bool WadIO::LevelHasLump(size_t lump_index, std::string_view name) const
{
  return this->LevelLookupLump(lump_index, name) != NO_INDEX;
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

void WadIO::CreateEmptyLump(std::string_view name)
{
  WadLump lump;
  strncpy(lump.name, name.data(), 8);
  lump.pos = GetLittleEndian(static_cast<uint32_t>(ftell(this->write_file)));
  lump.size = 0;
  this->write_directory.push_back(lump);
}

void WadIO::WriteLump(std::string_view name, const void *data, size_t size)
{
  WadLump lump;
  strncpy(lump.name, name.data(), 8);
  lump.pos = GetLittleEndian(static_cast<uint32_t>(ftell(this->write_file)));
  lump.size = IndexToInt(size);
  this->write_directory.push_back(lump);
  this->SafeWrite(data, size);
}

void WadIO::CopyLump(size_t index)
{
  auto lump = this->ReadLump<byte>(index);
  if (index < this->NumLumps())
  {
    WriteLump(this->LumpName(index), lump.data(), lump.size());
  }
}

//
// Partial writing
//

void WadIO::StartWritingLump(std::string_view name)
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
