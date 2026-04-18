# Command Line Interface

ELFBSP is a command-line tool.
It can handle multiple wad files, and while it modifies each file in-place, there is an option to backup each file first.
The output to the terminal is fairly terse, but greater verbosity can be enabled.
Generally all the maps in a wad will processed, but this can be limited to a specific set.

## Basic Usage

The simplest possible operation will rebuild nodes in all of the maps in a provided WAD:
```bash
elfbsp example.wad
```

## Output separate file
The following will rebuild all of the nodes in a separate copy of the provided WAD:
```bash
elfbsp example1.wad --output example2.wad
```

## Specific maps
To build only certain maps' nodes, the following option is available:
```bash
elfbsp example.wad --map MAP01
elfbsp example.wad --map MAP01,MAP03,MAP07 # multiple maps can be provided via comma-separation
elfbsp example.wad --map MAP10-MAP11       # or via a hyphen-separated range
elfbsp example.wad --map MAP04,MAP22-MAP25 # or you may combine both
```

## Exit Codes

- 0 if OK.
- 1 if nothing was built (no matching maps).
- 2 if one or more maps failed to build properly.
- 3 if a fatal error occurred.

## Documentation

#### `-v --verbose`
Produces more verbose output to the terminal.
Some warnings which are normally hidden (except for a final tally) will be shown when enabled.

#### `-b --backup`
Backs up each input file before processing it.
The backup files will have the ".bak" extension (replacing the ".wad" extension).
If the backup file already exists, it will be silently overwritten.

#### `-f --fast`
Enables a faster method for selecting partition lines.
On large maps this can be significantly faster, however the BSP tree may not be as good.

#### `-m --map  NAME(s)`
Specifies one or more maps to process. All other maps will be skipped (not touched at all).
The same set of maps applies to every given wad file. The default behavior is to process every map in the wad.

Map names must be the lump name, like "MAP01" or "E2M3", and cannot be abbreviated.
A range of maps can be specified using a hyphen, such as "MAP04-MAP07".
Several map names and/or ranges can be given, using commas to separate them, such as "MAP01,MAP03,MAP05".

NOTE: spaces cannot be used to separate map names.

#### `-t --type <0...5>`
Forces a specific BSP tree lump format at build time for the Doom & Hexen map formats.
Only DoomBSP and DeePBSPV4 are support in Doom 64 map format.
Only XGL3 is supported on UDMF map format.
In case of overflow from the DoomBSP format, ELFBSP will automatically promote to DeePBSPV4.
The default is 2, using ZDBSP's XNOD.
Supported formats are as follows:
* 0 -> DoomBSP
* 1 -> DeePBSPV4
* 2 -> XNOD (default)
* 3 -> XGLN
* 4 -> XGL2
* 5 -> XGL3

#### `--bmap <0...1>`
Forces the use of a specific blockmap lump at build time, for all supported map formats.
In case of overflow from the DoomBSP format, ELFBSP will automatically promote to XBM1.
The default is 0, using the vanilla format.
* 0 -> DoomBSP (default)
* 1 -> XBM1

#### `--no-effects`
Prevents the application of built time [special effects](./special_effects.md), causing them to be ignored.

#### `-p, --polyobj`
Use Hexen's original polyobject editor numbers (3000, 3001, 3002), instead of ZDoom's polyobject editor numbers (9300, 9301, 9302, 9303).

#### `-c --cost  <1...32>`
Sets the cost for making seg splits. The value is a number between 1 and 32. The default value is 11.
Larger values try to reduce the number of seg splits, whereas smaller values produce more balanced BSP trees.

NOTE: this option has little effect when the --fast option is enabled.

#### `-a --analysis`
Generates CSV files containing multiple builds of the input maps, used for data visualization purposes.
"Multiple builds" refers to re-building each map across every valid "split cost" value, from 1 to 32.

#### `-o --output  FILE`
This option is provided *only* for compatibility with existing node builders.
It causes the input file to be copied to the specified file, and that file is the one processed.
This option *cannot* be used with multiple input files, or with the --backup option.

#### `-h --help`
Displays a brief help screen, then exits.

#### `-d --doc`
Displays this documentation screen, then exits.

#### `--version`
Displays the version of ELFBSP, then exits.

#### `--debug-blockmap` `--debug-reject` `--debug-load` `--debug-bsp` `--debug-walltips` `--debug-polyobj` `--debug-overlaps` `--debug-picknode` `--debug-split` `--debug-cutlist` `--debug-builder` `--debug-sorter` `--debug-subsec` `--debug-wad`
Debugging utilities to display runtime information.
Any permutation of the above list is valid, though the output will become hard and harder to read.
It is recommended to use as few as needed at once.
