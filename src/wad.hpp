//------------------------------------------------------------------------------
//
//  ELFBSP
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

#pragma once

#include "core.hpp"

#include <string>
#include <vector>

struct Lump_c;

struct Wad_file
{
  char mode; // mode value passed to ::Open()

  FILE *fp;

  char kind; // 'P' for PWAD, 'I' for IWAD

  // zero means "currently unknown", which only occurs after a
  // call to BeginWrite() and before any call to AddLump() or
  // the finalizing EndWrite().
  off_t total_size;

  std::vector<Lump_c *> directory;

  size_t dir_start;
  size_t dir_count;

  // these are lump indices (into 'directory' vector)
  std::vector<size_t> levels;
  std::vector<size_t> patches;
  std::vector<size_t> sprites;
  std::vector<size_t> flats;
  std::vector<size_t> tx_tex;

  bool begun_write;
  size_t begun_max_size;

  // when >= 0, the next added lump is placed _before_ this
  size_t insert_point;

  // constructor is private
  Wad_file(const char *_name, char _mode, FILE *_fp);
  ~Wad_file(void);

  // open a wad file.
  //
  // mode is similar to the fopen() function:
  //   'r' opens the wad for reading ONLY
  //   'a' opens the wad for appending (read and write)
  //   'w' opens the wad for writing (i.e. create it)
  //
  // Note: if 'a' is used and the file is read-only, it will be
  //       silently opened in 'r' mode instead.
  //
  static Wad_file *Open(const char *filename, char mode = 'a');

  bool IsReadOnly(void) const
  {
    return mode == 'r';
  }

  size_t NumLumps(void) const
  {
    return directory.size();
  }

  Lump_c *GetLump(size_t index);

  size_t LevelCount(void) const
  {
    return levels.size();
  }

  size_t LevelHeader(size_t lev_num);
  size_t LevelLastLump(size_t lev_num);

  // returns a lump index, -1 if not found
  size_t LevelLookupLump(size_t lev_num, const char *name);

  map_format_t LevelFormat(size_t lev_num);

  void SortLevels(void);

  // all changes to the wad must occur between calls to BeginWrite()
  // and EndWrite() methods.  the on-disk wad directory may be trashed
  // during this period, it will be re-written by EndWrite().
  void BeginWrite(void);
  void EndWrite(void);

  // remove the given lump(s)
  // this will change index numbers on existing lumps
  // (previous results of FindLumpNum or LevelHeader are invalidated).
  void RemoveLumps(size_t index, size_t count = 1);

  // removes any ZNODES lump from a UDMF level.
  void RemoveZNodes(size_t lev_num);

  // insert a new lump.
  // The second form is for a level marker.
  // The 'max_size' parameter (if >= 0) specifies the most data
  // you will write into the lump -- writing more will corrupt
  // something else in the WAD.
  Lump_c *AddLump(const char *name, size_t max_size = NO_INDEX);

  // setup lump to write new data to it.
  // the old contents are lost.
  void RecreateLump(Lump_c *lump, size_t max_size = NO_INDEX);

  // set the insertion point -- the next lump will be added *before*
  // this index, and it will be incremented so that a sequence of
  // AddLump() calls produces lumps in the same order.
  //
  // passing a negative value or invalid index will reset the
  // insertion point -- future lumps get added at the END.
  // RemoveLumps(), RemoveLevel() and EndWrite() also reset it.
  void InsertPoint(size_t index = NO_INDEX);

  static Wad_file *Create(const char *filename, char mode);

  // read the existing directory.
  void ReadDirectory(void);

  void DetectLevels(void);
  void ProcessNamespaces(void);

  // look at all the lumps and determine the lowest offset from
  // start of file where we can write new data.  The directory itself
  // is ignored for this.
  size_t HighWaterMark(void);

  // look at all lumps in directory and determine the lowest offset
  // where a lump of the given length will fit.  Returns same as
  // HighWaterMark() when no largest gaps exist.  The directory itself
  // is ignored since it will be re-written at EndWrite().
  size_t FindFreeSpace(size_t length);

  // find a place (possibly at end of WAD) where we can write some
  // data of max_size (-1 means unlimited), and seek to that spot
  // (possibly writing some padding zeros -- the difference should
  // be no more than a few bytes).  Returns new position.
  size_t PositionForWrite(size_t max_size = NO_INDEX);

  bool FinishLump(size_t final_size);
  size_t WritePadding(size_t count);

  // write the new directory, updating the dir_xxx variables
  void WriteDirectory(void);

  void FixGroup(std::vector<size_t> &group, size_t index, size_t num_added, size_t num_removed);
};

struct Lump_c
{
  struct Wad_file *parent;

  std::string lumpname;

  size_t l_start;
  size_t l_length;

  // constructor is private
  Lump_c(Wad_file *_par, const char *_name, size_t _start, size_t _len);
  Lump_c(Wad_file *_par, const raw_wad_entry_t *entry);

  void MakeEntry(raw_wad_entry_t *entry);

  ~Lump_c(void);

  inline const char *Name(void) const
  {
    return lumpname.c_str();
  }

  inline size_t Length(void) const
  {
    return l_length;
  }

  // case insensitive match on the lump name
  inline bool Match(const char *s) const
  {
    return (0 == StringCaseCmp(lumpname.c_str(), s));
  }

  // do not call this directly, use Wad_file::RenameLump()
  inline void Rename(const char *new_name)
  {
    // ensure lump name is uppercase
    lumpname.clear();

    for (const char *s = new_name; *s != 0; s++)
    {
      lumpname.push_back(static_cast<char>(toupper(*s)));
    }
  }

  // attempt to seek to a position within the lump (default is
  // the beginning).  Returns true if OK, false on error.
  inline bool Seek(size_t offset)
  {
    return (fseeko(parent->fp, static_cast<off_t>(l_start + offset), SEEK_SET) == 0);
  }

  // read some data from the lump, returning true if OK.
  inline bool Read(void *data, size_t len)
  {
    SYS_ASSERT(data && len > 0);
    return (fread(data, len, 1, parent->fp) == 1);
  }

  // write some data to the lump.  Only the lump which had just
  // been created with Wad_file::AddLump() or RecreateLump() can be
  // written to.
  inline bool Write(const void *data, size_t len)
  {
    SYS_ASSERT(data && len > 0);
    l_length += len;
    return (fwrite(data, len, 1, parent->fp) == 1);
  }

  // mark the lump as finished (after writing data to it).
  inline bool Finish(void)
  {
    if (l_length == 0)
    {
      l_start = 0;
    }

    return parent->FinishLump(l_length);
  }
};
