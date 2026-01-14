
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

ELFBSP is a general purpose nodes builder for modern DOOM source ports.
It can build standard DOOM nodes and Extended ZDoom format nodes, as well as levels in the Doom, Hexen and UDMF formats.
The code is based on the BSP code in Eureka DOOM Editor, which was based on the code from glBSP but with significant changes. 

ELFBSP is a command-line tool.
It can handle multiple wad files, and while it modifies each file in-place, there is an option to backup each file first.
The output to the terminal is fairly terse, but greater verbosity can be enabled.
Generally all the maps in a wad will processed, but this can be limited to a specific set.

Legalese
--------

ELFBSP is Copyright &copy; 1997-2025 Guilherme Miranda, Andrew Apted, Colin Reed, and Lee Killough, et al.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
