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

#include <cstring>
#include <format>
#include <fstream>
#include <string>
#include <vector>

static bool opt_help = false;
static bool opt_version = false;

static std::string opt_output;

static std::vector<const char *> wad_list;

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
static std::vector<std::string> analysis_csv;

buildinfo_t config;

//------------------------------------------------------------------------
void AnalysisSetupFile(const char *filepath)
{
  auto csv_path = std::string(filepath);

  auto ext_pos = FindExtension(filepath);
  if (ext_pos > 0)
  {
    csv_path.resize(ext_pos);
  }

  csv_path += ".csv";
  FileClear(csv_path.c_str());

  std::string line = "map_name,is_fast,split_cost,num_segs,num_subsecs,num_nodes,size_left,size_right";
  analysis_csv.push_back(line);
}

void AnalysisPushLine(size_t level_index, bool is_fast, double split_cost, size_t segs, size_t subsecs, size_t nodes,
                      int32_t left_size, int32_t right_size)
{
  std::string line = std::format("{},{},{},{},{},{},{},{}", GetLevelName(level_index), is_fast, split_cost, segs, subsecs,
                                 nodes, left_size, right_size);
  analysis_csv.push_back(line);
}

// writes out for current file
// expects AnalysisPushLine to have been called with all 0-32 split costs during node-building
void WriteAnalysis(const char *filename)
{
  auto csv_path = std::string(filename);

  if (const auto ext_pos = FindExtension(filename); ext_pos > 0)
  {
    csv_path.resize(ext_pos);
  }

  csv_path += ".csv";

  // Append to fresh CSV
  auto csv_file = std::ofstream(csv_path.c_str(), std::ios::app);

  if (!csv_file.is_open())
  {
    PrintLine(LOG_WARN, "[%s] Couldn't open file %s for writing.", __func__, csv_path.c_str());
    return;
  }

  for (const auto &line : analysis_csv)
  {
    csv_file << line << '\n';
  }

  csv_file.close();

  // Flush out from memory
  analysis_csv.clear();

  PrintLine(LOG_NORMAL, "[%s] Successfully finished writing data to CSV file %s.", __func__, csv_path.c_str());
}

//------------------------------------------------------------------------

bool CheckMapInRange(const map_range_t *range, const char *name)
{
  if (strlen(name) != range->low.size())
  {
    return false;
  }

  if (strcmp(name, range->low.c_str()) < 0)
  {
    return false;
  }

  if (strcmp(name, range->high.c_str()) > 0)
  {
    return false;
  }

  return true;
}

bool CheckMapInMapList(const size_t lev_idx)
{
  // when --map is not used, allow everything
  if (map_list.empty())
  {
    return true;
  }

  const char *name = GetLevelName(lev_idx);

  for (auto &map : map_list)
  {
    if (CheckMapInRange(&map, name))
    {
      return true;
    }
  }

  return false;
}

static void BuildFile(const char *filename)
{
  config.total_warnings = 0;

  const size_t num_levels = LevelsInWad();

  if (num_levels == 0)
  {
    PrintLine(LOG_NORMAL, "No levels in wad");
    total_empty_files += 1;
    return;
  }

  size_t visited = 0;
  size_t failures = 0;

  build_result_e res = BUILD_OK;

  // loop over each level in the wad
  for (size_t n = 0; n < num_levels; n++)
  {
    if (!CheckMapInMapList(n))
    {
      continue;
    }

    visited += 1;

    res = BuildLevel(n, filename);

    // handle a failed map (due to lump overflow)
    if (res == BUILD_LumpOverflow)
    {
      res = BUILD_OK;
      failures += 1;
      continue;
    }

    if (res != BUILD_OK)
    {
      break;
    }

    total_built_maps += 1;
  }

  if (visited == 0)
  {
    PrintLine(LOG_NORMAL, "No matching levels");
    total_empty_files += 1;
    return;
  }

  total_failed_maps += failures;

  if (failures > 0)
  {
    PrintLine(LOG_NORMAL, "Failed maps: %zu (out of %zu)", failures, visited);

    // allow building other files
    total_failed_files += 1;
  }

  PrintLine(LOG_NORMAL, "Serious warnings: %zu", config.total_warnings);
}

void ValidateInputFilename(const char *filename)
{
  // NOTE: these checks are case-insensitive

  // files with ".bak" extension cannot be backed up, so refuse them
  if (MatchExtension(filename, "bak"))
  {
    PrintLine(LOG_ERROR, "cannot process a backup file: %s", filename);
  }

  // we do not support packages
  if (MatchExtension(filename, "pak") || MatchExtension(filename, "pk2") || MatchExtension(filename, "pk3")
      || MatchExtension(filename, "pk4") || MatchExtension(filename, "pk7") || MatchExtension(filename, "epk")
      || MatchExtension(filename, "pack") || MatchExtension(filename, "zip") || MatchExtension(filename, "rar"))
  {
    PrintLine(LOG_ERROR, "package files (like PK3) are not supported: %s", filename);
  }

  // reject some very common formats
  if (!MatchExtension(filename, "wad"))
  {
    PrintLine(LOG_ERROR, "not a wad file: %s", filename);
  }
}

void BackupFile(const char *filename)
{
  std::string dest_name = filename;

  // replace file extension (if any) with .bak

  size_t ext_pos = FindExtension(filename);
  if (ext_pos > 0)
  {
    dest_name.resize(ext_pos);
  }

  dest_name += ".bak";

  if (!FileCopy(filename, dest_name.c_str()))
  {
    PrintLine(LOG_ERROR, "failed to create backup: %s", dest_name.c_str());
  }

  PrintLine(LOG_NORMAL, "Created backup: %s", dest_name.c_str());
}

void VisitFile(const char *filename)
{
  // handle the -o option
  if (!opt_output.empty())
  {
    if (!FileCopy(filename, opt_output.c_str()))
    {
      PrintLine(LOG_ERROR, "failed to create output file: %s", opt_output.c_str());
    }

    PrintLine(LOG_NORMAL, "Copied input file: %s", filename);

    filename = opt_output.c_str();
  }

  if (config.backup)
  {
    BackupFile(filename);
  }

  if (config.analysis)
  {
    AnalysisSetupFile(filename);
  }

  PrintLine(LOG_NORMAL, "Building %s", filename);

  // this will fatal error if it fails
  OpenWad(filename);

  BuildFile(filename);

  CloseWad();
}

// ----- user information -----------------------------

bool ValidateMapName(char *name)
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

void ParseMapRange(char *tok)
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
    PrintLine(LOG_ERROR, "illegal map name: '%s'", low);
  }

  if (!ValidateMapName(high))
  {
    PrintLine(LOG_ERROR, "illegal map name: '%s'", high);
  }

  if (strlen(low) < strlen(high))
  {
    PrintLine(LOG_ERROR, "bad map range (%s shorter than %s)", low, high);
  }

  if (strlen(low) > strlen(high))
  {
    PrintLine(LOG_ERROR, "bad map range (%s longer than %s)", low, high);
  }

  if (low[0] != high[0])
  {
    PrintLine(LOG_ERROR, "bad map range (%s and %s start with different letters)", low, high);
  }

  if (strcmp(low, high) > 0)
  {
    PrintLine(LOG_ERROR, "bad map range (wrong order, %s > %s)", low, high);
  }

  // Ok

  map_range_t range;

  range.low = low;
  range.high = high;

  map_list.push_back(range);
}

void ParseMapList(const char *arg)
{
  while (*arg != 0)
  {
    if (*arg == ',')
    {
      PrintLine(LOG_ERROR, "bad map list (empty element)");
    }

    // copy characters up to next comma / end
    char buffer[64];
    size_t len = 0;

    while (!(*arg == 0 || *arg == ','))
    {
      if (len > sizeof(buffer) - 4)
      {
        PrintLine(LOG_ERROR, "bad map list (very long element)");
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

void ParseShortArgument(const char *arg)
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
    case 'b':
      config.backup = true;
      continue;

    case 'v':
      config.verbose = true;
      continue;
    case 'f':
      config.fast = true;
      continue;
    case 'x':
      config.force_xnod = true;
      continue;
    case 's':
      config.ssect_xgl3 = true;
      continue;
    case 'a':
      config.analysis = true;
      continue;

    case 'm':
    case 'o':
      PrintLine(LOG_ERROR, "cannot use option '-%c' like that", c);
      return;

    case 'c':
      if (*arg == 0 || !isdigit(*arg))
      {
        PrintLine(LOG_ERROR, "missing value for '-c' option");
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
        PrintLine(LOG_ERROR, "illegal value for '-c' option");
      }

      config.split_cost = val;
      continue;

    default:
      if (isprint(c) && !isspace(c))
      {
        PrintLine(LOG_ERROR, "unknown short option: '-%c'", c);
      }
      else
      {
        PrintLine(LOG_ERROR, "illegal short option (ascii code %d)", static_cast<unsigned char>(c));
      }
      return;
    }
  }
}

void ProcessDebugParam(const char *param, uint32_t &debug)
{
  if (strcmp(param, "--debug-blockmap") == 0)
  {
    debug |= DEBUG_BLOCKMAP;
  }
  else if (strcmp(param, "--debug-reject") == 0)
  {
    debug |= DEBUG_REJECT;
  }
  else if (strcmp(param, "--debug-load") == 0)
  {
    debug |= DEBUG_LOAD;
  }
  else if (strcmp(param, "--debug-bsp") == 0)
  {
    debug |= DEBUG_BSP;
  }
  else if (strcmp(param, "--debug-walltips") == 0)
  {
    debug |= DEBUG_WALLTIPS;
  }
  else if (strcmp(param, "--debug-polyobj") == 0)
  {
    debug |= DEBUG_POLYOBJ;
  }
  else if (strcmp(param, "--debug-overlaps") == 0)
  {
    debug |= DEBUG_OVERLAPS;
  }
  else if (strcmp(param, "--debug-picknode") == 0)
  {
    debug |= DEBUG_PICKNODE;
  }
  else if (strcmp(param, "--debug-split") == 0)
  {
    debug |= DEBUG_SPLIT;
  }
  else if (strcmp(param, "--debug-cutlist") == 0)
  {
    debug |= DEBUG_CUTLIST;
  }
  else if (strcmp(param, "--debug-builder") == 0)
  {
    debug |= DEBUG_BUILDER;
  }
  else if (strcmp(param, "--debug-sorter") == 0)
  {
    debug |= DEBUG_SORTER;
  }
  else if (strcmp(param, "--debug-subsec") == 0)
  {
    debug |= DEBUG_SUBSEC;
  }
  else if (strcmp(param, "--debug-wad") == 0)
  {
    debug |= DEBUG_WAD;
  }
}

int32_t ParseLongArgument(const char *name, const int32_t argc, const char *argv[])
{
  int32_t used = 0;

  if (strcmp(name, "--help") == 0)
  {
    opt_help = true;
  }
  else if (strcmp(name, "--version") == 0)
  {
    opt_version = true;
  }
  else if (strcmp(name, "--analysis") == 0)
  {
    config.analysis = true;
  }
  else if (strcmp(name, "--verbose") == 0)
  {
    config.verbose = true;
  }
  else if (strcmp(name, "--backup") == 0 || strcmp(name, "--backups") == 0)
  {
    config.backup = true;
  }
  else if (strcmp(name, "--fast") == 0)
  {
    config.fast = true;
  }
  else if (strcmp(name, "--map") == 0 || strcmp(name, "--maps") == 0)
  {
    if (argc < 1 || argv[0][0] == '-')
    {
      PrintLine(LOG_ERROR, "missing value for '--map' option");
    }

    ParseMapList(argv[0]);

    used = 1;
  }
  else if (strcmp(name, "--xnod") == 0)
  {
    config.force_xnod = true;
  }
  else if (strcmp(name, "--ssect") == 0)
  {
    config.ssect_xgl3 = true;
  }
  else if (strcmp(name, "--cost") == 0)
  {
    if (argc < 1 || !isdigit(argv[0][0]))
    {
      PrintLine(LOG_ERROR, "missing value for '--cost' option");
    }

    int32_t val = std::stoi(argv[0]);

    if (val < SPLIT_COST_MIN || val > SPLIT_COST_MAX)
    {
      PrintLine(LOG_ERROR, "illegal value for '--cost' option");
    }

    config.split_cost = val;
    used = 1;
  }
  else if (strcmp(name, "--output") == 0)
  {
    // this option is *only* for compatibility

    if (argc < 1 || argv[0][0] == '-')
    {
      PrintLine(LOG_ERROR, "missing value for '--output' option");
    }

    if (!opt_output.empty())
    {
      PrintLine(LOG_ERROR, "cannot use '--output' option twice");
    }

    opt_output = argv[0];
    used = 1;
  }
  else if (strncmp(name, "--debug-", 8) == 0)
  {
    ProcessDebugParam(name, config.debug);
  }
  else
  {
    PrintLine(LOG_ERROR, "unknown long option: '%s'", name);
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
    const char *arg = *argv++;
    argc--;

    if constexpr (MACOS)
    {
      // ignore Mac OS X garbage
      if (strncmp(arg, "-psn_", 5) == 0)
      {
        continue;
      }
    }

    if (strlen(arg) == 0)
    {
      continue;
    }

    // a normal filename?
    if (arg[0] != '-' || rest_are_files)
    {
      wad_list.push_back(arg);
      continue;
    }

    if (strcmp(arg, "-") == 0)
    {
      PrintLine(LOG_ERROR, "illegal option '-'");
    }

    if (strcmp(arg, "--") == 0)
    {
      rest_are_files = true;
      continue;
    }

    // handle short args which are isolate and require a value
    if (strcmp(arg, "-c") == 0)
    {
      arg = "--cost";
    }
    if (strcmp(arg, "-m") == 0)
    {
      arg = "--map";
    }
    if (strcmp(arg, "-o") == 0)
    {
      arg = "--output";
    }
    if (strcmp(arg, "-a") == 0)
    {
      arg = "--analysis";
    }

    if (arg[1] != '-')
    {
      ParseShortArgument(arg);
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
    PrintLine(LOG_NORMAL, PROJECT_STRING);
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
    PrintLine(LOG_ERROR, "no files to process");
    return 0;
  }

  if (!opt_output.empty())
  {
    if (config.backup)
    {
      PrintLine(LOG_ERROR, "cannot use --backup with --output");
    }

    if (total_files > 1)
    {
      PrintLine(LOG_ERROR, "cannot use multiple input files with --output");
    }

    if (StringCaseCmp(wad_list[0], opt_output.c_str()) == 0)
    {
      PrintLine(LOG_ERROR, "input and output files are the same");
    }
  }

  // validate all filenames before processing any of them
  for (const auto filename : wad_list)
  {
    ValidateInputFilename(filename);

    if (!FileExists(filename))
    {
      PrintLine(LOG_ERROR, "no such file: %s", filename);
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
