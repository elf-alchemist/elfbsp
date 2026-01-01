//------------------------------------------------------------------------
//  UTILITIES
//------------------------------------------------------------------------
//
//  Copyright (C) 2025      Guilherme Miranda
//  Copyright (C) 2001-2018 Andrew Apted
//  Copyright (C) 1997-2003 André Majorel et al
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
//------------------------------------------------------------------------

#include <cctype>

#include "local.hpp"
#include "system.hpp"
#include "utility.hpp"

#ifdef WIN32
#include <io.h>
#else  // UNIX or MACOSX
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <sys/param.h>
#endif

namespace elfbsp
{

//------------------------------------------------------------------------
//  FILENAMES
//------------------------------------------------------------------------

bool HasExtension(const char *filename)
{
	int A = (int)strlen(filename) - 1;

	if (A > 0 && filename[A] == '.')
		return false;

	for (; A >= 0 ; A--)
	{
		if (filename[A] == '.')
			return true;

		if (filename[A] == DIR_SEP_CH)
			break;

#ifdef WIN32
		if (filename[A] == DIR_SEP_CH || filename[A] == ':')
			break;
#endif
	}

	return false;
}

//
// MatchExtension
//
// When ext is NULL, checks if the file has no extension.
//
bool MatchExtension(const char *filename, const char *ext)
{
	if (! ext)
		return ! HasExtension(filename);

	size_t A = strlen(filename) - 1;
	size_t B = strlen(ext) - 1;

	for (;; B--, A--)
	{
		if (toupper(filename[A]) != toupper(ext[B]))
			return false;
	}

	return (A >= 1) && (filename[A] == '.');
}


//
// FindExtension
//
// Return offset of the '.', or NO_INDEX when no extension was found.
//
size_t FindExtension(const char *filename)
{
	if (filename[0] == 0)
		return NO_INDEX;

	size_t pos = strlen(filename) - 1;

	for (; filename[pos] != '.' ; pos--)
	{
		char ch = filename[pos];

		if (ch == DIR_SEP_CH)
			break;

#ifdef WIN32
		if (ch == DIR_SEP_CH || ch == ':')
			break;
#endif
	}

	if (filename[pos] != '.')
		return NO_INDEX;

	return pos;
}


//------------------------------------------------------------------------
// FILE MANAGEMENT
//------------------------------------------------------------------------

bool FileExists(const char *filename)
{
	FILE *fp = fopen(filename, "rb");

	if (fp)
	{
		fclose(fp);
		return true;
	}

	return false;
}


bool FileCopy(const char *src_name, const char *dest_name)
{
	char buffer[1024];

	FILE *src = fopen(src_name, "rb");
	if (! src)
		return false;

	FILE *dest = fopen(dest_name, "wb");
	if (! dest)
	{
		fclose(src);
		return false;
	}

	while (true)
	{
		size_t rlen = fread(buffer, 1, sizeof(buffer), src);
		if (rlen == 0)
			break;

		size_t wlen = fwrite(buffer, 1, rlen, dest);
		if (wlen != rlen)
			break;
	}

	bool was_OK = !ferror(src) && !ferror(dest);

	fclose(dest);
	fclose(src);

	return was_OK;
}


bool FileRename(const char *old_name, const char *new_name)
{
#ifdef WIN32
	return (::MoveFile(old_name, new_name) != 0);

#else // UNIX or MACOSX
	return (rename(old_name, new_name) == 0);
#endif
}


bool FileDelete(const char *filename)
{
#ifdef WIN32
	return (::DeleteFile(filename) != 0);

#else // UNIX or MACOSX
	return (remove(filename) == 0);
#endif
}

//------------------------------------------------------------------------
// STRINGS
//------------------------------------------------------------------------

//
// a case-insensitive strcmp()
//
int StringCaseCmp(const char *s1, const char *s2)
{
	for (;;)
	{
		int A = tolower(*s1++);
		int B = tolower(*s2++);

		if (A != B)
			return A - B;

		if (A == 0)
			return 0;
	}
}


//
// a case-insensitive strncmp()
//
int StringCaseCmpMax(const char *s1, const char *s2, size_t len)
{
	SYS_ASSERT(len != 0);

	for (;;)
	{
		if (len == 0)
			return 0;

		int A = tolower(*s1++);
		int B = tolower(*s2++);

		if (A != B)
			return A - B;

		if (A == 0)
			return 0;

		len--;
	}
}


//------------------------------------------------------------------------
// MEMORY ALLOCATION
//------------------------------------------------------------------------

//
// Allocate memory with error checking.  Zeros the memory.
//
void *UtilCalloc(size_t size)
{
	void *ret = calloc(1, size);

	if (!ret)
		cur_info->FatalError("Out of memory (cannot allocate %d bytes)\n", size);

	return ret;
}


//
// Reallocate memory with error checking.
//
void *UtilRealloc(void *old, size_t size)
{
	void *ret = realloc(old, size);

	if (!ret)
		cur_info->FatalError("Out of memory (cannot reallocate %d bytes)\n", size);

	return ret;
}


//
// Free the memory with error checking.
//
void UtilFree(void *data)
{
	if (data == NULL)
		BugError("Trying to free a NULL pointer\n");

	free(data);
}


//------------------------------------------------------------------------
// MATH STUFF
//------------------------------------------------------------------------

//
// rounds the value _up_ to the nearest power of two.
//
int RoundPOW2(int x)
{
	if (x <= 2)
		return x;

	x--;

	for (int tmp = x >> 1 ; tmp ; tmp >>= 1)
		x |= tmp;

	return x + 1;
}


//
// Compute angle of line from (0,0) to (dx,dy).
// Result is degrees, where 0 is east and 90 is north.
//
double ComputeAngle(double dx, double dy)
{
	double angle;

	if (dx == 0)
		return (dy > 0) ? 90.0 : 270.0;

	angle = atan2((double) dy, (double) dx) * 180.0 / M_PI;

	if (angle < 0)
		angle += 360.0;

	return angle;
}


//------------------------------------------------------------------------
//  Adler-32 CHECKSUM Code
//------------------------------------------------------------------------

void Adler32_Begin(uint32_t *crc)
{
	*crc = 1;
}

void Adler32_AddBlock(uint32_t *crc, const uint8_t *data, int length)
{
	uint32_t s1 = (*crc) & 0xFFFF;
	uint32_t s2 = ((*crc) >> 16) & 0xFFFF;

	for ( ; length > 0 ; data++, length--)
	{
		s1 = (s1 + *data) % 65521;
		s2 = (s2 + s1)    % 65521;
	}

	*crc = (s2 << 16) | s1;
}

void Adler32_Finish(uint32_t *crc)
{
	/* nothing to do */
}

} // namespace elfbsp

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
