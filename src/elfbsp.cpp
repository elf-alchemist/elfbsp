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

#include "elfbsp.hpp"
#include "core.hpp"

#include <cstring>
#include <string>
#include <vector>

bool opt_help = false;
bool opt_doc = false;
bool opt_version = false;

std::string opt_output;

std::vector<const char *> wad_list;

size_t total_failed_files = 0;
size_t total_empty_files = 0;
size_t total_built_maps = 0;
size_t total_failed_maps = 0;

struct map_range_t
{
  std::string low;
  std::string high;
};

std::vector<map_range_t> map_list;

buildinfo_t config;

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

bool CheckMapInMaplist(size_t lev_idx)
{
  // when --map is not used, allow everything
  if (map_list.empty())
  {
    return true;
  }

  const char *name = GetLevelName(lev_idx);

  for (unsigned int i = 0; i < map_list.size(); i++)
  {
    if (CheckMapInRange(&map_list[i], name))
    {
      return true;
    }
  }

  return false;
}

static void BuildFile(void)
{
  config.total_warnings = 0;
  config.total_minor_issues = 0;

  size_t num_levels = LevelsInWad();

  if (num_levels == 0)
  {
    Print("  No levels in wad\n");
    total_empty_files += 1;
    return;
  }

  size_t visited = 0;
  size_t failures = 0;

  build_result_e res = BUILD_OK;

  // loop over each level in the wad
  for (size_t n = 0; n < num_levels; n++)
  {
    if (!CheckMapInMaplist(n))
    {
      continue;
    }

    visited += 1;

    if (n > 0)
    {
      config.Print_Verbose("\n");
    }

    res = BuildLevel(n);

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

  StopHanging();

  if (visited == 0)
  {
    Print("  No matching levels\n");
    total_empty_files += 1;
    return;
  }

  Print("\n");

  total_failed_maps += failures;

  if (failures > 0)
  {
    Print("  Failed maps: %zu (out of %zu)\n", failures, visited);

    // allow building other files
    total_failed_files += 1;
  }

  Print("  Serious warnings: %zu\n", config.total_warnings);

  if (config.verbose)
  {
    Print("  Minor issues: %zu\n", config.total_minor_issues);
  }
}

void ValidateInputFilename(const char *filename)
{
  // NOTE: these checks are case-insensitive

  // files with ".bak" extension cannot be backed up, so refuse them
  if (MatchExtension(filename, "bak"))
  {
    FatalError("cannot process a backup file: %s\n", filename);
  }

  // we do not support packages
  if (MatchExtension(filename, "pak") || MatchExtension(filename, "pk2") || MatchExtension(filename, "pk3")
      || MatchExtension(filename, "pk4") || MatchExtension(filename, "pk7") || MatchExtension(filename, "epk")
      || MatchExtension(filename, "pack") || MatchExtension(filename, "zip") || MatchExtension(filename, "rar"))
  {
    FatalError("package files (like PK3) are not supported: %s\n", filename);
  }

  // reject some very common formats
  if (MatchExtension(filename, "exe") || MatchExtension(filename, "dll") || MatchExtension(filename, "com")
      || MatchExtension(filename, "bat") || MatchExtension(filename, "txt") || MatchExtension(filename, "doc")
      || MatchExtension(filename, "deh") || MatchExtension(filename, "bex") || MatchExtension(filename, "lmp")
      || MatchExtension(filename, "cfg") || MatchExtension(filename, "gif") || MatchExtension(filename, "png")
      || MatchExtension(filename, "jpg") || MatchExtension(filename, "jpeg"))
  {
    FatalError("not a wad file: %s\n", filename);
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
    FatalError("failed to create backup: %s\n", dest_name.c_str());
  }

  Print("\nCreated backup: %s\n", dest_name.c_str());
}

void VisitFile(unsigned int idx, const char *filename)
{
  // handle the -o option
  if (opt_output.size() > 0)
  {
    if (!FileCopy(filename, opt_output.c_str()))
    {
      FatalError("failed to create output file: %s\n", opt_output.c_str());
    }

    Print("\nCopied input file: %s\n", filename);

    filename = opt_output.c_str();
  }

  if (config.backup)
  {
    BackupFile(filename);
  }

  Print("\n");
  Print("Building %s\n", filename);

  // this will fatal error if it fails
  OpenWad(filename);

  BuildFile();

  CloseWad();
}

// ----- user information -----------------------------

void ShowDoc(void)
{
  printf(PRINT_DOC);

  fflush(stdout);
}

void ShowHelp(void)
{
  printf(PRINT_HELP);

  fflush(stdout);
}

void ShowVersion(void)
{
  printf("%s\n", PROJECT_STRING);

  fflush(stdout);
}

void ShowBanner(void)
{
  printf("+---------------------------------------------------+\n");
  printf("|   ELFBSP %s (C) 2025 Guilherme Miranda, et al   |\n", PROJECT_VERSION);
  printf("+---------------------------------------------------+\n");

  fflush(stdout);
}

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
    FatalError("illegal map name: '%s'\n", low);
  }

  if (!ValidateMapName(high))
  {
    FatalError("illegal map name: '%s'\n", high);
  }

  if (strlen(low) < strlen(high))
  {
    FatalError("bad map range (%s shorter than %s)\n", low, high);
  }

  if (strlen(low) > strlen(high))
  {
    FatalError("bad map range (%s longer than %s)\n", low, high);
  }

  if (low[0] != high[0])
  {
    FatalError("bad map range (%s and %s start with different letters)\n", low, high);
  }

  if (strcmp(low, high) > 0)
  {
    FatalError("bad map range (wrong order, %s > %s)\n", low, high);
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
      FatalError("bad map list (empty element)\n");
    }

    // copy characters up to next comma / end
    char buffer[64];
    size_t len = 0;

    while (!(*arg == 0 || *arg == ','))
    {
      if (len > sizeof(buffer) - 4)
      {
        FatalError("bad map list (very long element)\n");
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
    int val = 0;

    switch (c)
    {
      case 'h':
        opt_help = true;
        continue;
      case 'd':
        opt_doc = true;
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

      case 'm':
      case 'o':
      case 't':
        FatalError("cannot use option '-%c' like that\n", c);
        return;

      case 'c':
        if (*arg == 0 || !isdigit(*arg))
        {
          FatalError("missing value for '-c' option\n");
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
          FatalError("illegal value for '-c' option\n");
        }

        config.split_cost = static_cast<size_t>(val);
        continue;

      default:
        if (isprint(c) && !isspace(c))
        {
          FatalError("unknown short option: '-%c'\n", c);
        }
        else
        {
          FatalError("illegal short option (ascii code %d)\n", (int)(unsigned char)c);
        }
        return;
    }
  }
}

int ParseLongArgument(const char *name, int argc, char *argv[])
{
  int used = 0;

  if (strcmp(name, "--help") == 0)
  {
    opt_help = true;
  }
  else if (strcmp(name, "--doc") == 0)
  {
    opt_doc = true;
  }
  else if (strcmp(name, "--version") == 0)
  {
    opt_version = true;
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
      FatalError("missing value for '--map' option\n");
    }

    ParseMapList(argv[0]);

    used = 1;
  }
  else if (strcmp(name, "--no-reject") == 0)
  {
    config.no_reject = true;
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
      FatalError("missing value for '--cost' option\n");
    }

    int32_t val = std::stoi(argv[0]);

    if (val < SPLIT_COST_MIN || val > SPLIT_COST_MAX)
    {
      FatalError("illegal value for '--cost' option\n");
    }

    config.split_cost = static_cast<size_t>(val);
    used = 1;
  }
  else if (strcmp(name, "--output") == 0)
  {
    // this option is *only* for compatibility

    if (argc < 1 || argv[0][0] == '-')
    {
      FatalError("missing value for '--output' option\n");
    }

    if (opt_output.size() > 0)
    {
      FatalError("cannot use '--output' option twice\n");
    }

    opt_output = argv[0];
    used = 1;
  }
  else
  {
    FatalError("unknown long option: '%s'\n", name);
  }

  return used;
}

void ParseCommandLine(int argc, char *argv[])
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
      // ignore MacOS X rubbish
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
      FatalError("illegal option '-'\n");
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

    if (arg[1] != '-')
    {
      ParseShortArgument(arg);
      continue;
    }

    int count = ParseLongArgument(arg, argc, argv);

    if (count > 0)
    {
      argc -= count;
      argv += count;
    }
  }
}

int main(int argc, char *argv[])
{
  ParseCommandLine(argc, argv);

  if (opt_version)
  {
    ShowVersion();
    return 0;
  }

  if (opt_help || argc <= 1)
  {
    ShowBanner();
    ShowHelp();
    return 0;
  }

  if (opt_doc)
  {
    ShowBanner();
    ShowDoc();
    return 0;
  }

  size_t total_files = wad_list.size();

  if (total_files == 0)
  {
    FatalError("no files to process\n");
    return 0;
  }

  if (opt_output.size() > 0)
  {
    if (config.backup)
    {
      FatalError("cannot use --backup with --output\n");
    }

    if (total_files > 1)
    {
      FatalError("cannot use multiple input files with --output\n");
    }

    if (StringCaseCmp(wad_list[0], opt_output.c_str()) == 0)
    {
      FatalError("input and output files are the same\n");
    }
  }

  ShowBanner();

  // validate all filenames before processing any of them
  for (unsigned int i = 0; i < wad_list.size(); i++)
  {
    const char *filename = wad_list[i];

    ValidateInputFilename(filename);

    if (!FileExists(filename))
    {
      FatalError("no such file: %s\n", filename);
    }
  }

  for (unsigned int i = 0; i < wad_list.size(); i++)
  {
    VisitFile(i, wad_list[i]);
  }

  Print("\n");

  if (total_failed_files > 0)
  {
    Print("FAILURES occurred on %zu map%s in %zu file%s.\n", total_failed_maps, total_failed_maps == 1 ? "" : "s",
          total_failed_files, total_failed_files == 1 ? "" : "s");

    if (!config.verbose)
    {
      Print("Rerun with --verbose to see more details.\n");
    }

    return 2;
  }
  else if (total_built_maps == 0)
  {
    Print("NOTHING was built!\n");

    return 1;
  }
  else if (total_empty_files == 0)
  {
    Print("Ok, built all files.\n");
  }
  else
  {
    size_t built = total_files - total_empty_files;
    size_t empty = total_empty_files;

    Print("Ok, built %zu file%s, %zu file%s empty.\n", built, (built == 1 ? "" : "s"), empty, (empty == 1 ? " was" : "s were"));
  }

  // that's all folks!
  return 0;
}
