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

//
// Node Build Information Structure
//

static constexpr int32_t SPLIT_COST_MIN = 1;
static constexpr int32_t SPLIT_COST_DEFAULT = 11;
static constexpr int32_t SPLIT_COST_MAX = 32;

class buildinfo_t
{
public:
  // use a faster method to pick nodes
  bool fast;
  // when these two are false, they create an empty lump
  bool do_blockmap;
  bool do_reject;

  bool force_xnod;
  bool ssect_xgl3;

  // the GUI can set this to tell the node builder to stop
  bool cancelled;

  int split_cost;

  // this affects how some messages are shown
  bool verbose;

  // from here on, various bits of internal state
  int total_warnings;
  int total_minor_issues;

public:
  buildinfo_t(void)
      : fast(false),

        do_blockmap(true), do_reject(true),

        force_xnod(false),

        cancelled(false),

        split_cost(SPLIT_COST_DEFAULT), verbose(false),

        total_warnings(0), total_minor_issues(0)
  {
  }

  ~buildinfo_t(void)
  {
  }

public:
  virtual void Print_Verbose(const char *fmt, ...) = 0;
  virtual void ShowMap(const char *name) = 0;
};

constexpr const char PRINT_USAGE[] = "\n";

constexpr const char PRINT_HELP[] = "\n"
                                    "Usage: elfbsp [options...] FILE...\n"
                                    "\n"
                                    "Available options are:\n"
                                    "    -d --doc           Documentation, shows all options\n"
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
                                    "and/or ranges can be specified: MAP01,MAP04-MAP07,MAP12\n"
                                    "\n";

constexpr const char PRINT_DOC[] = "\n"
                                   "`-v --verbose`\n"
                                   "Produces more verbose output to the terminal.\n"
                                   "Some warnings which are normally hidden (except for a final\n"
                                   "tally) will be shown when enabled.\n"
                                   "\n"
                                   "`-b --backup`\n"
                                   "Backs up each input file before processing it.\n"
                                   "The backup files will have the \".bak\" extension\n"
                                   "(replacing the \".wad\" extension).  If the backup\n"
                                   "file already exists, it will be silently overwritten.\n"
                                   "\n"
                                   "`-f --fast`  \n"
                                   "Enables a faster method for selecting partition lines.\n"
                                   "On large maps this can be significantly faster,\n"
                                   "however the BSP tree may not be as good.\n"
                                   "\n"
                                   "`-m --map  NAME(s)`\n"
                                   "Specifies one or more maps to process.\n"
                                   "All other maps will be skipped (not touched at all).\n"
                                   "The same set of maps applies to every given wad file.\n"
                                   "The default behavior is to process every map in the wad.\n"
                                   "\n"
                                   "Map names must be the lump name, like \"MAP01\" or \"E2M3\",\n"
                                   "and cannot be abbreviated.  A range of maps can be\n"
                                   "specified using a hyphen, such as \"MAP04-MAP07\".\n"
                                   "Several map names and/or ranges can be given, using\n"
                                   "commas to separate them, such as \"MAP01,MAP03,MAP05\".\n"
                                   "\n"
                                   "NOTE: spaces cannot be used to separate map names.\n"
                                   "\n"
                                   "`-x --xnod`\n"
                                   "Forces XNOD (ZDoom extended) format of normal nodes.\n"
                                   "Without this option, normal nodes will be built using\n"
                                   "the standard DOOM format, and only switch to XNOD format\n"
                                   "when the level is too large (e.g. has too many segs).\n"
                                   "\n"
                                   "Using XNOD format can be better for source ports which\n"
                                   "support it, since it provides higher accuracy for seg\n"
                                   "splits.  However, it cannot be used with the original\n"
                                   "DOOM.EXE or with Chocolate-Doom.\n"
                                   "\n"
                                   "`-s --ssect`\n"
                                   "Build XGL3 (extended Nodes) format in the SSECTORS lump.\n"
                                   "This option will disable the building of normal nodes, leaving\n"
                                   "the NODES and SEGS lumps empty.  Although it can be used with\n"
                                   "the `-x` option to store XNOD format nodes in the NODES lump\n"
                                   "as well.\n"
                                   "\n"
                                   "`-c --cost  ##`\n"
                                   "Sets the cost for making seg splits.\n"
                                   "The value is a number between 1 and 32.\n"
                                   "Larger values try to reduce the number of seg splits,\n"
                                   "whereas smaller values produce more balanced BSP trees.\n"
                                   "The default value is 11.\n"
                                   "\n"
                                   "NOTE: this option has little effect when the --fast\n"
                                   "option is enabled.\n"
                                   "\n"
                                   "`-o --output  FILE`\n"
                                   "This option is provided *only* for compatibility with\n"
                                   "existing node builders.  It causes the input file to be\n"
                                   "copied to the specified file, and that file is the one\n"
                                   "processed.  This option *cannot* be used with multiple\n"
                                   "input files, or with the --backup option.\n"
                                   "\n"
                                   "`-h --help`\n"
                                   "Displays a brief help screen, then exits.\n"
                                   "\n"
                                   "`-d --doc`\n"
                                   "Displays this documentaion screen, then exits.\n"
                                   "\n"
                                   "`--version`\n"
                                   "Displays the version of ELFBSP, then exits.\n"
                                   "\n";

typedef enum
{
  // everything went peachy keen
  BUILD_OK = 0,

  // building was cancelled
  BUILD_Cancelled,

  // when saving the map, one or more lumps overflowed
  BUILD_LumpOverflow
} build_result_e;

// set the build information.  must be done before anything else.
void SetInfo(buildinfo_t *info);

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
