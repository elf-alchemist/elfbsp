
ELFBSP 1.0
==========

[![Top Language](https://img.shields.io/github/languages/top/elf-alchemist/elfbsp.svg)](https://github.com/elf-alchemist/elfbsp)
[![Code Size](https://img.shields.io/github/languages/code-size/elf-alchemist/elfbsp.svg)](https://github.com/elf-alchemist/elfbsp)
[![License](https://img.shields.io/github/license/elf-alchemist/elfbsp.svg?logo=gnu)](https://github.com/elf-alchemist/elfbsp/blob/main/LICENSE.txt)
[![Release](https://img.shields.io/github/release/elf-alchemist/elfbsp.svg)](https://github.com/elf-alchemist/elfbsp/releases/latest)
[![Release Date](https://img.shields.io/github/release-date/elf-alchemist/elfbsp.svg)](https://github.com/elf-alchemist/elfbsp/releases/latest)
[![Downloads (total)](https://img.shields.io/github/downloads/elf-alchemist/elfbsp/total)](https://github.com/elf-alchemist/elfbsp/releases/latest)
[![Downloads (latest)](https://img.shields.io/github/downloads/elf-alchemist/elfbsp/latest/total.svg)](https://github.com/elf-alchemist/elfbsp/releases/latest)
[![Commits](https://img.shields.io/github/commits-since/elf-alchemist/elfbsp/latest.svg)](https://github.com/elf-alchemist/elfbsp/commits/main)
[![Last Commit](https://img.shields.io/github/last-commit/elf-alchemist/elfbsp.svg)](https://github.com/elf-alchemist/elfbsp/commits/main)
[![Build Status](https://github.com/elf-alchemist/elfbsp/actions/workflows/main.yml/badge.svg)](https://github.com/elf-alchemist/elfbsp/actions/workflows/main.yml)

by Guilherme Miranda, 2025 -- based on AJBSP, by Andrew Apted, 2022.

About
-----

ELFBSP is a simple nodes builder for modern DOOM source ports.
It can build standard DOOM nodes and Extended ZDoom format nodes,
and since version 1.5 supports the UDMF map format.  The code is
based on the BSP code in Eureka DOOM Editor, which was based on the
code from glBSP but with significant changes.

ELFBSP is a command-line tool.  It can handle multiple wad files,
and modifies each file in-place.  There is an option to backup each
file first.  The output to the terminal is fairly terse, but greater
verbosity can be enabled.  Generally all the maps in a wad will
processed, but this can be limited to a specific set.


Exit Codes
----------

- 0 if OK.
- 1 if nothing was built (no matching maps).
- 2 if one or more maps failed to build properly.
- 3 if a fatal error occurred.


Usage
-----

The simplest possible operation will rebuild nodes in all of the maps in a provided WAD:
```bash
elfbsp example.wad
```

The following will rebuild all of the nodes in a seperate copy of the provided WAD:
```bash
elfbsp example1.wad --output example2.wad
```

To build only certain maps' nodes, the following option is available:
```bash
elfbsp example.wad --map MAP01
elfbsp example.wad --map MAP01,MAP03,MAP07 # multiple maps can be provided via comma-separation
elfbsp example.wad --map MAP10-MAP11       # or via a hyphen-separated range
elfbsp example.wad --map MAP04,MAP22-MAP25 # or you may combine both
```

For a basic explanation of the main options, type:
```bash
elfbsp --help
```

For a complete options list, and documentation for each one, type:
```bash
elfbsp --doc
```

Compiling
---------

The ELFBSP code is fairly portable C++, and does not depend
on any third-party libraries. It requires at least C++17.
Both GNU g++ and clang++ are known to work.

Building should be fairly straight-forward on any Unix-like
system, such as Linux, the BSDs, and even MacOS X. With the
main development dependency being CMake

On Debian Linux, you will need the following packages:

- g++
- binutils
- cmake
- make

To build the program, type the following:

```bash
cmake -B build && make all -C build
```

To install ELFBSP, for which you will need root priveliges, do:

```bash
cmake -B build && sudo make install -C build
```

Legalese
--------

ELFBSP is Copyright &copy; 2025 Guilherme Miranda, Andrew Apted,
Colin Reed, and Lee Killough, et al.

ELFBSP is Free Software, under the terms of the GNU General Public
License, version 2 or (at your option) any later version.
See the [LICENSE.txt](LICENSE.txt) file for the complete text.

ELFBSP comes with NO WARRANTY of any kind, express or implied.
Please read the license for full details.
