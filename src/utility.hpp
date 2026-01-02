//------------------------------------------------------------------------------
//
//  ELFBSP
//
//------------------------------------------------------------------------------
//
//  Copyright 2025-2026 Guilherme Miranda
//  Copyright 2001-2013 Andrew Apted
//  Copyright 1997-2003 André Majorel et al
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

#include <cstddef>
#include <cstdint>

#ifdef WIN32
  #define DIR_SEP_CH  '\\'
  #define DIR_SEP_STR "\\"
#else
  #define DIR_SEP_CH  '/'
  #define DIR_SEP_STR "/"
#endif

// filename functions
bool HasExtension(const char *filename);
bool MatchExtension(const char *filename, const char *ext);
size_t FindExtension(const char *filename);

// file utilities
bool FileExists(const char *filename);
bool FileCopy(const char *src_name, const char *dest_name);
bool FileRename(const char *old_name, const char *new_name);
bool FileDelete(const char *filename);

// memory allocation, guaranteed to not return nullptr.
void *UtilCalloc(size_t size);
void *UtilRealloc(void *old, size_t size);
void UtilFree(void *data);

// math stuff
int RoundPOW2(int x);
double ComputeAngle(double dx, double dy);

// string utilities
int StringCaseCmp(const char *s1, const char *s2);
int StringCaseCmpMax(const char *s1, const char *s2, size_t len);

// checksum functions
void Adler32_Begin(uint32_t *crc);
void Adler32_AddBlock(uint32_t *crc, const uint8_t *data, int length);
void Adler32_Finish(uint32_t *crc);
