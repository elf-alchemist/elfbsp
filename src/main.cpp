//------------------------------------------------------------------------------
//
//  ELFBSP
//
//------------------------------------------------------------------------------
//
//  Copyright 2025-2026 Guilherme Miranda
//  Copyright 2001-2022 Andrew Apted
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

#include "core.hpp"
#include "info.hpp"
#include "local.hpp"
#include "wad.hpp"

#include <cstring>
#include <string>
#include <string_view>
#include <vector>

static bool opt_help = false;
static bool opt_version = false;

static std::string_view opt_output;

static std::vector<std::string> wad_list;

static size_t total_failed_files = 0;
static size_t total_empty_files = 0;
static size_t total_built_maps = 0;
static size_t total_failed_maps = 0;

struct map_range_t
{
  std::string low;
  std::string high;
};

static std::vector<map_range_t> map_list;

buildinfo_t config;

//------------------------------------------------------------------------

//------------------------------------------------------------------------
// FILE MANAGEMENT
//------------------------------------------------------------------------

static bool FileExists(std::string filename)
{

  if (FILE *fp = fopen(filename.c_str(), "rb"))
  {
    fclose(fp);
    return true;
  }

  return false;
}

static bool CheckMapInRange(const map_range_t *range, std::string name)
{
  if (name.size() != range->low.size())
  {
    return false;
  }

  if (name < range->low)
  {
    return false;
  }

  if (name > range->high)
  {
    return false;
  }

  return true;
}

static bool CheckMapInMapList(std::string name)
{
  // when --map is not used, allow everything
  if (map_list.empty())
  {
    return true;
  }

  for (auto &map : map_list)
  {
    if (CheckMapInRange(&map, name))
    {
      return true;
    }
  }

  return false;
}

static void VisitFile(std::string_view filename)
{
  auto mark = Benchmarker(__func__);
  auto in_file = std::string(filename);
  auto out_file = std::string();

  // handle the -o option
  // by this point, we are guaranteed to have an opt_output that is different than wad_list[0]
  bool is_same_file = false;
  if (opt_output.empty())
  {
    is_same_file = true;
    out_file = in_file + ".x";
  }
  else
  {
    is_same_file = false;
    out_file = opt_output;
  }

  if (config.analysis)
  {
    SetupAnalysisFile(in_file);
  }

  PrintLine(LOG_NORMAL, "Building %s", in_file.c_str());
  {
    config.total_warnings = 0;
    auto wad = WadIO(in_file, out_file);
    size_t current = 0;
    size_t max = wad.NumLumps();

    size_t levels_visited = 0;
    size_t levels_failed = 0;

    while (current < max)
    {
      if (wad.IsLevel(current) && CheckMapInMapList(wad.LumpName(current)))
      {
        level_t level;
        level.lump = current;
        level.map_format = wad.LevelFormat(current);

        levels_visited += 1;
        auto result = BuildLevel(wad, level, in_file);

        // handle a failed map (due to lump overflow)
        if (result == BUILD_LumpOverflow)
        {
          levels_failed += 1;
        }
        else
        {
          total_built_maps += 1;
        }

        current = wad.LevelLastLump(current) + 1;
      }
      // TODO: process glbsp
      else
      {
        wad.CopyLump(current);
        current++;
      }
    }

    if (levels_visited == 0)
    {
      PrintLine(LOG_NORMAL, "No matching levels found");
      total_empty_files += 1;
    }

    total_failed_maps += levels_failed;

    if (levels_failed > 0)
    {
      PrintLine(LOG_NORMAL, "Failed maps: %zu (out of %zu)", levels_failed, levels_visited);

      // allow building other files
      total_failed_files += 1;
    }

    PrintLine(LOG_NORMAL, "Serious warnings: %zu", config.total_warnings);
  }

  if (is_same_file)
  {
    remove(in_file.c_str());
    if (rename(out_file.c_str(), in_file.c_str()))
    {
      throw std::runtime_error("ERROR: Failed to rename output file (" + out_file + ") to input file (" + in_file + ")");
    }
  }
}

// ----- user information -----------------------------

static bool ValidateMapName(char *name)
{
  if (strlen(name) < 2 || strlen(name) > 8)
  {
    return false;
  }

  if (!isalpha(name[0]))
  {
    return false;
  }

  for (const char *p = name; *p; p++)
  {
    if (!(isalnum(*p) || *p == '_'))
    {
      return false;
    }
  }

  // Ok, convert to upper case
  for (char *s = name; *s; s++)
  {
    *s = static_cast<char>(toupper(*s));
  }

  return true;
}

static void ParseMapRange(char *tok)
{
  char *low = tok;
  char *high = tok;

  // look for '-' separator
  char *p = strchr(tok, '-');

  if (p)
  {
    *p++ = 0;

    high = p;
  }

  if (!ValidateMapName(low))
  {
    PrintLine(LOG_ERROR, "ERROR: illegal map name: '%s'", low);
  }

  if (!ValidateMapName(high))
  {
    PrintLine(LOG_ERROR, "ERROR: illegal map name: '%s'", high);
  }

  if (strlen(low) < strlen(high))
  {
    PrintLine(LOG_ERROR, "ERROR: bad map range (%s shorter than %s)", low, high);
  }

  if (strlen(low) > strlen(high))
  {
    PrintLine(LOG_ERROR, "ERROR: bad map range (%s longer than %s)", low, high);
  }

  if (low[0] != high[0])
  {
    PrintLine(LOG_ERROR, "ERROR: bad map range (%s and %s start with different letters)", low, high);
  }

  if (strcmp(low, high) > 0)
  {
    PrintLine(LOG_ERROR, "ERROR: bad map range (wrong order, %s > %s)", low, high);
  }

  // Ok

  map_range_t range;

  range.low = low;
  range.high = high;

  map_list.push_back(range);
}

static void ParseMapList(const char *arg)
{
  while (*arg != 0)
  {
    if (*arg == ',')
    {
      PrintLine(LOG_ERROR, "ERROR: bad map list (empty element)");
    }

    // copy characters up to next comma / end
    char buffer[64];
    size_t len = 0;

    while (!(*arg == 0 || *arg == ','))
    {
      if (len > sizeof(buffer) - 4)
      {
        PrintLine(LOG_ERROR, "ERROR: bad map list (very long element)");
      }

      buffer[len++] = *arg++;
    }

    buffer[len] = 0;

    ParseMapRange(buffer);

    if (*arg == ',')
    {
      arg++;
    }
  }
}

static void ParseShortArgument(const char *arg)
{
  // skip the leading '-'
  arg++;

  while (*arg)
  {
    char c = *arg++;
    int32_t val = 0;

    switch (c)
    {
    case 'h':
      opt_help = true;
      continue;
    case 'a':
      config.analysis = true;
      continue;
    case 'v':
      config.verbose = true;
      continue;
    case 'f':
      config.fast = true;
      continue;

    case 'm':
    case 'o':
    case 't':
      PrintLine(LOG_ERROR, "ERROR: cannot use option '-%c' like that", c);
      return;

    case 'c':
      if (*arg == 0 || !isdigit(*arg))
      {
        PrintLine(LOG_ERROR, "ERROR: missing value for '-c' option");
      }

      // we only accept one or two digits here
      val = *arg - '0';
      arg++;

      if (*arg && isdigit(*arg))
      {
        val = (val * 10) + (*arg - '0');
        arg++;
      }

      if (val < SPLIT_COST_MIN || val > SPLIT_COST_MAX)
      {
        PrintLine(LOG_ERROR, "ERROR: illegal value for '-c' option");
      }

      config.split_cost = val;
      continue;

    default:
      if (isprint(c) && !isspace(c))
      {
        PrintLine(LOG_ERROR, "ERROR: unknown short option: '-%c'", c);
      }
      else
      {
        PrintLine(LOG_ERROR, "ERROR: illegal short option (ascii code %d)", static_cast<unsigned char>(c));
      }
      return;
    }
  }
}

static bool ProcessDebugParam(std::string param, uint32_t &debug)
{
  if (param == "--debug-blockmap")
  {
    debug |= DEBUG_BLOCKMAP;
  }
  else if (param == "--debug-reject")
  {
    debug |= DEBUG_REJECT;
  }
  else if (param == "--debug-load")
  {
    debug |= DEBUG_LOAD;
  }
  else if (param == "--debug-bsp")
  {
    debug |= DEBUG_BSP;
  }
  else if (param == "--debug-walltips")
  {
    debug |= DEBUG_WALLTIPS;
  }
  else if (param == "--debug-polyobj")
  {
    debug |= DEBUG_POLYOBJ;
  }
  else if (param == "--debug-overlaps")
  {
    debug |= DEBUG_OVERLAPS;
  }
  else if (param == "--debug-picknode")
  {
    debug |= DEBUG_PICKNODE;
  }
  else if (param == "--debug-split")
  {
    debug |= DEBUG_SPLIT;
  }
  else if (param == "--debug-cutlist")
  {
    debug |= DEBUG_CUTLIST;
  }
  else if (param == "--debug-builder")
  {
    debug |= DEBUG_BUILDER;
  }
  else if (param == "--debug-sorter")
  {
    debug |= DEBUG_SORTER;
  }
  else if (param == "--debug-subsec")
  {
    debug |= DEBUG_SUBSEC;
  }
  else if (param == "--debug-wad")
  {
    debug |= DEBUG_WAD;
  }

  return debug != 0;
}

static int32_t ParseLongArgument(std::string name, const int32_t argc, const char *argv[])
{
  int32_t used = 0;

  if (name == "--help")
  {
    opt_help = true;
  }
  else if (name == "--version")
  {
    opt_version = true;
  }
  else if (name == "--analysis")
  {
    config.analysis = true;
  }
  else if (name == "--verbose")
  {
    config.verbose = true;
  }
  else if (name == "--fast")
  {
    config.fast = true;
  }
  else if (name == "--no-effects")
  {
    config.effects = false;
  }
  else if (name == "--compress")
  {
    config.compress = true;
  }
  else if (name == "--map" || name == "--maps")
  {
    if (argc < 1 || argv[0][0] == '-')
    {
      PrintLine(LOG_ERROR, "ERROR: missing value for '--map' option");
    }

    ParseMapList(argv[0]);

    used = 1;
  }
  else if (name == "--type")
  {
    if (argc < 1 || !isdigit(argv[0][0]))
    {
      PrintLine(LOG_ERROR, "ERROR: missing value for '--type' option");
    }

    int32_t val = std::stoi(argv[0]);

    if (val < BSP_MIN || val > BSP_MAX)
    {
      PrintLine(LOG_ERROR, "ERROR: illegal value for '--type' option");
    }

    config.bsp_format = static_cast<bsp_format_t>(val);
    used = 1;
  }
  else if (name == "--bmap")
  {
    if (argc < 1 || !isdigit(argv[0][0]))
    {
      PrintLine(LOG_ERROR, "ERROR: missing value for '--bmap' option");
    }

    int32_t val = std::stoi(argv[0]);

    if (val < BMAP_MIN || val > BMAP_MAX)
    {
      PrintLine(LOG_ERROR, "ERROR: illegal value for '--bmap' option");
    }

    config.bmap_format = static_cast<bmap_format_t>(val);
    used = 1;
  }
  else if (name == "--cost")
  {
    if (argc < 1 || !isdigit(argv[0][0]))
    {
      PrintLine(LOG_ERROR, "ERROR: missing value for '--cost' option");
    }

    int32_t val = std::stoi(argv[0]);

    if (val < SPLIT_COST_MIN || val > SPLIT_COST_MAX)
    {
      PrintLine(LOG_ERROR, "ERROR: illegal value for '--cost' option");
    }

    config.split_cost = val;
    used = 1;
  }
  else if (name == "--polyobj")
  {
    config.polyobj.anchor = Hexen_PolyObj_Anchor;
    config.polyobj.spawn = Hexen_PolyObj_Spawn;
    config.polyobj.spawn_crush = Hexen_PolyObj_SpawnCrush;
    config.polyobj.spawn_hurt = Hexen_PolyObj_SpawnHurt;
  }
  else if (name == "--output")
  {
    // this option is *only* for compatibility

    if (argc < 1 || argv[0][0] == '-')
    {
      PrintLine(LOG_ERROR, "ERROR: missing value for '--output' option");
    }

    if (!opt_output.empty())
    {
      PrintLine(LOG_ERROR, "ERROR: cannot use '--output' option twice");
    }

    opt_output = argv[0];
    used = 1;
  }
  else if (name.starts_with("--debug-"))
  {
    if (!ProcessDebugParam(name.c_str(), config.debug))
    {
      PrintLine(LOG_ERROR, "ERROR: unknown debug parameter '%s'", name.c_str());
    }
  }
  else
  {
    PrintLine(LOG_ERROR, "ERROR: unknown long option: '%s'", name.c_str());
  }

  return used;
}

void ParseCommandLine(int32_t argc, const char *argv[])
{
  // skip program name
  argc--;
  argv++;

  bool rest_are_files = false;

  while (argc > 0)
  {
    std::string arg = *argv++;
    argc--;

    if constexpr (MACOS)
    {
      // ignore Mac OS X garbage
      if (strncmp(arg.data(), "-psn_", 5) == 0)
      {
        continue;
      }
    }

    if (arg.empty())
    {
      continue;
    }

    // a normal filename?
    if (arg[0] != '-' || rest_are_files)
    {
      wad_list.push_back(arg);
      continue;
    }

    if (arg == "-")
    {
      PrintLine(LOG_ERROR, "ERROR: illegal option '-'");
    }

    if (arg == "--")
    {
      rest_are_files = true;
      continue;
    }

    // handle short args which are isolate and require a value
    if (arg == "-t")
    {
      arg = "--type";
    }
    if (arg == "-c")
    {
      arg = "--cost";
    }
    if (arg == "-m")
    {
      arg = "--map";
    }
    if (arg == "-o")
    {
      arg = "--output";
    }

    if (arg[1] != '-')
    {
      ParseShortArgument(arg.c_str());
      continue;
    }

    int32_t count = ParseLongArgument(arg, argc, argv);

    if (count > 0)
    {
      argc -= count;
      argv += count;
    }
  }
}

int32_t main(const int32_t argc, const char *argv[])
{
  ParseCommandLine(argc, argv);

  if (opt_version)
  {
    PrintLine(LOG_NORMAL, VERSION_INFO);
    return 0;
  }

  if (opt_help || argc <= 1)
  {
    PrintLine(LOG_NORMAL, PRINT_HELP);
    return 0;
  }

  size_t total_files = wad_list.size();

  if (total_files == 0)
  {
    PrintLine(LOG_ERROR, "ERROR: no files to process");
    return 0;
  }

  if (!opt_output.empty())
  {
    if (total_files > 1)
    {
      PrintLine(LOG_ERROR, "ERROR: cannot use multiple input files with --output");
    }

    if (wad_list[0] == opt_output)
    {
      PrintLine(LOG_ERROR, "ERROR: input and output files are the same");
    }
  }

  // validate all filenames before processing any of them
  for (const auto &filename : wad_list)
  {
    if (!FileExists(filename))
    {
      throw std::runtime_error("ERROR: no such file: " + filename);
    }
  }

  for (const auto &wad : wad_list)
  {
    VisitFile(wad);
  }

  if (total_failed_files > 0)
  {
    PrintLine(LOG_NORMAL, "FAILURES occurred on %zu map%s in %zu file%s.", total_failed_maps, total_failed_maps == 1 ? "" : "s",
              total_failed_files, total_failed_files == 1 ? "" : "s");

    if (!config.verbose)
    {
      PrintLine(LOG_NORMAL, "Rerun with --verbose to see more details.");
    }

    return 2;
  }
  else if (total_built_maps == 0)
  {
    PrintLine(LOG_NORMAL, "NOTHING was built!");

    return 1;
  }
  else if (total_empty_files == 0)
  {
    PrintLine(LOG_NORMAL, "Ok, built all files.");
  }
  else
  {
    size_t built = total_files - total_empty_files;

    PrintLine(LOG_NORMAL, "Ok, built %zu file%s, %zu file%s empty.", built, (built == 1 ? "" : "s"), total_empty_files,
              (total_empty_files == 1 ? " was" : "s were"));
  }

  // that's all folks!
  return 0;
}
