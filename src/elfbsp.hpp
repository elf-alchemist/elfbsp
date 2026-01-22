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

#pragma once

#include "core.hpp"
#include <cstddef>
#include <cstdint>

//
// Node Build Information Structure
//

static constexpr int32_t SPLIT_COST_MIN = 1;
static constexpr int32_t SPLIT_COST_DEFAULT = 11;
static constexpr int32_t SPLIT_COST_MAX = 32;

using buildinfo_t = struct buildinfo_s;

extern buildinfo_t config;

struct buildinfo_s
{
  // use a faster method to pick nodes
  bool fast = false;
  bool backup = false;

  bool force_xnod = false;
  bool ssect_xgl3 = false;

  size_t split_cost = SPLIT_COST_DEFAULT;

  // this affects how some messages are shown
  bool verbose = false;

  // from here on, various bits of internal state
  size_t total_warnings = 0;

  uint32_t debug = DEBUG_NONE;
};

constexpr const char PRINT_HELP[] = "\n"
                                    "Usage: elfbsp [options...] FILE...\n"
                                    "\n"
                                    "Available options are:\n"
                                    "    -v --verbose       Verbose output, show all warnings\n"
                                    "    -b --backup        Backup input files (.bak extension)\n"
                                    "    -f --fast          Faster partition selection\n"
                                    "    -m --map   XXXX    Control which map(s) are built\n"
                                    "    -c --cost  ##      Cost assigned to seg splits (1-32)\n"
                                    "\n"
                                    "    -x --xnod          Use XNOD format in NODES lump\n"
                                    "    -s --ssect         Use XGL3 format in SSECTORS lump\n"
                                    "\n"
                                    "Short options may be mixed, for example: -fbv\n"
                                    "Long options must always begin with a double hyphen\n"
                                    "\n"
                                    "Map names should be full, like E1M3 or MAP24, but a list\n"
                                    "and/or ranges can be specified: MAP01,MAP04-MAP07,MAP12\n";

using build_result_t = enum build_result_e
{
  // everything went peachy keen
  BUILD_OK = 0,

  // when saving the map, one or more lumps overflowed
  BUILD_LumpOverflow
};

// attempt to open a wad.  on failure, the FatalError method in the
// buildinfo_t interface is called.
void OpenWad(const char *filename);

// close a previously opened wad.
void CloseWad(void);

// give the number of levels detected in the wad.
size_t LevelsInWad(void);

// retrieve the name of a particular level.
const char *GetLevelName(size_t lev_idx);

// build the nodes of a particular level.  if cancelled, returns the
// BUILD_Cancelled result and the wad is unchanged.  otherwise the wad
// is updated to store the new lumps and returns either BUILD_OK or
// BUILD_LumpOverflow if some limits were exceeded.
build_result_e BuildLevel(size_t lev_idx);
