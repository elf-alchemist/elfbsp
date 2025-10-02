//------------------------------------------------------------------------
//
//  ELFBSP  Copyright (C) 2025       Guilherme Miranda
//          Copyright (C) 2000-2018  Andrew Apted, et al
//          Copyright (C) 1994-1998  Colin Reed
//          Copyright (C) 1997-1998  Lee Killough
//
//  Originally based on the program 'BSP', version 2.3.
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

#include "elfbsp.hpp"
#include "local.hpp"
#include "parse.hpp"
#include "raw_def.hpp"
#include "system.hpp"
#include "utility.hpp"
#include "wad.hpp"

#define DEBUG_BLOCKMAP  0
#define DEBUG_REJECT    0

#define DEBUG_LOAD      0
#define DEBUG_BSP       0


namespace elfbsp
{

Wad_file * cur_wad;

static int block_x, block_y;
static int block_w, block_h;
static int block_count;

static int block_mid_x = 0;
static int block_mid_y = 0;

static uint16_t ** block_lines;

static uint16_t *block_ptrs;
static uint16_t *block_dups;

static int block_compression;
static int block_overflowed;

#define BLOCK_LIMIT  16000

#define DUMMY_DUP  0xFFFF


void GetBlockmapBounds(int *x, int *y, int *w, int *h)
{
	*x = block_x; *y = block_y;
	*w = block_w; *h = block_h;
}


int CheckLinedefInsideBox(int xmin, int ymin, int xmax, int ymax,
		int x1, int y1, int x2, int y2)
{
	int count = 2;
	int tmp;

	for (;;)
	{
		if (y1 > ymax)
		{
			if (y2 > ymax)
				return false;

			x1 = x1 + (int) ((x2-x1) * (double)(ymax-y1) / (double)(y2-y1));
			y1 = ymax;

			count = 2;
			continue;
		}

		if (y1 < ymin)
		{
			if (y2 < ymin)
				return false;

			x1 = x1 + (int) ((x2-x1) * (double)(ymin-y1) / (double)(y2-y1));
			y1 = ymin;

			count = 2;
			continue;
		}

		if (x1 > xmax)
		{
			if (x2 > xmax)
				return false;

			y1 = y1 + (int) ((y2-y1) * (double)(xmax-x1) / (double)(x2-x1));
			x1 = xmax;

			count = 2;
			continue;
		}

		if (x1 < xmin)
		{
			if (x2 < xmin)
				return false;

			y1 = y1 + (int) ((y2-y1) * (double)(xmin-x1) / (double)(x2-x1));
			x1 = xmin;

			count = 2;
			continue;
		}

		count--;

		if (count == 0)
			break;

		// swap end points
		tmp=x1;  x1=x2;  x2=tmp;
		tmp=y1;  y1=y2;  y2=tmp;
	}

	// linedef touches block
	return true;
}


/* ----- create blockmap ------------------------------------ */

#define BK_NUM    0
#define BK_MAX    1
#define BK_XOR    2
#define BK_FIRST  3

#define BK_QUANTUM  32

static void BlockAdd(int blk_num, int line_index)
{
	uint16_t *cur = block_lines[blk_num];

#if DEBUG_BLOCKMAP
	cur_info->Debug("Block %d has line %d\n", blk_num, line_index);
#endif

	if (blk_num < 0 || blk_num >= block_count)
		BugError("BlockAdd: bad block number %d\n", blk_num);

	if (! cur)
	{
		// create empty block
		block_lines[blk_num] = cur = (uint16_t *)UtilCalloc(BK_QUANTUM * sizeof(uint16_t));
		cur[BK_NUM] = 0;
		cur[BK_MAX] = BK_QUANTUM;
		cur[BK_XOR] = 0x1234;
	}

	if (BK_FIRST + cur[BK_NUM] == cur[BK_MAX])
	{
		// no more room, so allocate some more...
		cur[BK_MAX] += BK_QUANTUM;

		block_lines[blk_num] = cur = (uint16_t *)UtilRealloc(cur, cur[BK_MAX] * sizeof(uint16_t));
	}

	// compute new checksum
	cur[BK_XOR] = (uint16_t) (((cur[BK_XOR] << 4) | (cur[BK_XOR] >> 12)) ^ line_index);

	cur[BK_FIRST + cur[BK_NUM]] = LE_U16(line_index);
	cur[BK_NUM]++;
}


static void BlockAddLine(const linedef_t *L)
{
	int x1 = (int) L->start->x;
	int y1 = (int) L->start->y;
	int x2 = (int) L->end->x;
	int y2 = (int) L->end->y;

	int bx1 = (std::min(x1,x2) - block_x) / 128;
	int by1 = (std::min(y1,y2) - block_y) / 128;
	int bx2 = (std::max(x1,x2) - block_x) / 128;
	int by2 = (std::max(y1,y2) - block_y) / 128;

	int bx, by;
	int line_index = L->index;

#if DEBUG_BLOCKMAP
	cur_info->Debug("BlockAddLine: %d (%d,%d) -> (%d,%d)\n", line_index,
			x1, y1, x2, y2);
#endif

	// handle truncated blockmaps
	if (bx1 < 0) bx1 = 0;
	if (by1 < 0) by1 = 0;
	if (bx2 >= block_w) bx2 = block_w - 1;
	if (by2 >= block_h) by2 = block_h - 1;

	if (bx2 < bx1 || by2 < by1)
		return;

	// handle simple case #1: completely horizontal
	if (by1 == by2)
	{
		for (bx=bx1 ; bx <= bx2 ; bx++)
		{
			int blk_num = by1 * block_w + bx;
			BlockAdd(blk_num, line_index);
		}
		return;
	}

	// handle simple case #2: completely vertical
	if (bx1 == bx2)
	{
		for (by=by1 ; by <= by2 ; by++)
		{
			int blk_num = by * block_w + bx1;
			BlockAdd(blk_num, line_index);
		}
		return;
	}

	// handle the rest (diagonals)

	for (by=by1 ; by <= by2 ; by++)
	for (bx=bx1 ; bx <= bx2 ; bx++)
	{
		int blk_num = by * block_w + bx;

		int minx = block_x + bx * 128;
		int miny = block_y + by * 128;
		int maxx = minx + 127;
		int maxy = miny + 127;

		if (CheckLinedefInsideBox(minx, miny, maxx, maxy, x1, y1, x2, y2))
		{
			BlockAdd(blk_num, line_index);
		}
	}
}


static void CreateBlockmap(void)
{
	block_lines = (uint16_t **) UtilCalloc(block_count * sizeof(uint16_t *));

	for (int i=0 ; i < num_linedefs ; i++)
	{
		const linedef_t *L = lev_linedefs[i];

		// ignore zero-length lines
		if (L->zero_len)
			continue;

		BlockAddLine(L);
	}
}


static int BlockCompare(const void *p1, const void *p2)
{
	int blk_num1 = ((const uint16_t *) p1)[0];
	int blk_num2 = ((const uint16_t *) p2)[0];

	const uint16_t *A = block_lines[blk_num1];
	const uint16_t *B = block_lines[blk_num2];

	if (A == B)
		return 0;

	if (A == NULL) return -1;
	if (B == NULL) return +1;

	if (A[BK_NUM] != B[BK_NUM])
	{
		return A[BK_NUM] - B[BK_NUM];
	}

	if (A[BK_XOR] != B[BK_XOR])
	{
		return A[BK_XOR] - B[BK_XOR];
	}

	return memcmp(A+BK_FIRST, B+BK_FIRST, A[BK_NUM] * sizeof(uint16_t));
}


static void CompressBlockmap(void)
{
	int i;
	int cur_offset;
#if DEBUG_BLOCKMAP
	int dup_count=0;
#endif

	int orig_size, new_size;

	block_ptrs = (uint16_t *)UtilCalloc(block_count * sizeof(uint16_t));
	block_dups = (uint16_t *)UtilCalloc(block_count * sizeof(uint16_t));

	// sort duplicate-detecting array.  After the sort, all duplicates
	// will be next to each other.  The duplicate array gives the order
	// of the blocklists in the BLOCKMAP lump.

	for (i=0 ; i < block_count ; i++)
		block_dups[i] = (uint16_t) i;

	qsort(block_dups, block_count, sizeof(uint16_t), BlockCompare);

	// scan duplicate array and build up offset array

	cur_offset = 4 + block_count + 2;

	orig_size = 4 + block_count;
	new_size  = cur_offset;

	for (i=0 ; i < block_count ; i++)
	{
		int blk_num = block_dups[i];
		int count;

		// empty block ?
		if (block_lines[blk_num] == NULL)
		{
			block_ptrs[blk_num] = (uint16_t) (4 + block_count);
			block_dups[i] = DUMMY_DUP;

			orig_size += 2;
			continue;
		}

		count = 2 + block_lines[blk_num][BK_NUM];

		// duplicate ?  Only the very last one of a sequence of duplicates
		// will update the current offset value.

		if (i+1 < block_count && BlockCompare(block_dups + i, block_dups + i+1) == 0)
		{
			block_ptrs[blk_num] = (uint16_t) cur_offset;
			block_dups[i] = DUMMY_DUP;

			// free the memory of the duplicated block
			UtilFree(block_lines[blk_num]);
			block_lines[blk_num] = NULL;

#if DEBUG_BLOCKMAP
			dup_count++;
#endif

			orig_size += count;
			continue;
		}

		// OK, this block is either the last of a series of duplicates, or
		// just a singleton.

		block_ptrs[blk_num] = (uint16_t) cur_offset;

		cur_offset += count;

		orig_size += count;
		new_size  += count;
	}

	if (cur_offset > 65535)
	{
		block_overflowed = true;
		return;
	}

#if DEBUG_BLOCKMAP
	cur_info->Debug("Blockmap: Last ptr = %d  duplicates = %d\n",
			cur_offset, dup_count);
#endif

	block_compression = (orig_size - new_size) * 100 / orig_size;

	// there's a tiny chance of new_size > orig_size
	if (block_compression < 0)
		block_compression = 0;
}


static int CalcBlockmapSize()
{
	// compute size of final BLOCKMAP lump.
	// it does not need to be exact, but it *does* need to be bigger
	// (or equal) to the actual size of the lump.

	// header + null_block + a bit extra
	int size = 20;

	// the pointers (offsets to the line lists)
	size = size + block_count * 2;

	// add size of each block
	for (int i=0 ; i < block_count ; i++)
	{
		int blk_num = block_dups[i];

		// ignore duplicate or empty blocks
		if (blk_num == DUMMY_DUP)
			continue;

		uint16_t *blk = block_lines[blk_num];
		SYS_ASSERT(blk);

		size += (1 + (int)(blk[BK_NUM]) + 1) * 2;
	}

	return size;
}


static void WriteBlockmap(void)
{
	int i;

	int max_size = CalcBlockmapSize();

	Lump_c *lump = CreateLevelLump("BLOCKMAP", max_size);

	uint16_t null_block[2] = { 0x0000, 0xFFFF };
	uint16_t m_zero = 0x0000;
	uint16_t m_neg1 = 0xFFFF;

	// fill in header
	raw_blockmap_header_t header;

	header.x_origin = LE_U16(block_x);
	header.y_origin = LE_U16(block_y);
	header.x_blocks = LE_U16(block_w);
	header.y_blocks = LE_U16(block_h);

	lump->Write(&header, sizeof(header));

	// handle pointers
	for (i=0 ; i < block_count ; i++)
	{
		uint16_t ptr = LE_U16(block_ptrs[i]);

		if (ptr == 0)
			BugError("WriteBlockmap: offset %d not set.\n", i);

		lump->Write(&ptr, sizeof(uint16_t));
	}

	// add the null block which *all* empty blocks will use
	lump->Write(null_block, sizeof(null_block));

	// handle each block list
	for (i=0 ; i < block_count ; i++)
	{
		int blk_num = block_dups[i];

		// ignore duplicate or empty blocks
		if (blk_num == DUMMY_DUP)
			continue;

		uint16_t *blk = block_lines[blk_num];
		SYS_ASSERT(blk);

		lump->Write(&m_zero, sizeof(uint16_t));
		lump->Write(blk + BK_FIRST, blk[BK_NUM] * sizeof(uint16_t));
		lump->Write(&m_neg1, sizeof(uint16_t));
	}

	lump->Finish();
}


static void FreeBlockmap(void)
{
	for (int i=0 ; i < block_count ; i++)
	{
		if (block_lines[i])
			UtilFree(block_lines[i]);
	}

	UtilFree(block_lines);
	UtilFree(block_ptrs);
	UtilFree(block_dups);
}


static void FindBlockmapLimits(bbox_t *bbox)
{
	double mid_x = 0;
	double mid_y = 0;

	bbox->minx = bbox->miny = SHRT_MAX;
	bbox->maxx = bbox->maxy = SHRT_MIN;

	for (int i=0 ; i < num_linedefs ; i++)
	{
		const linedef_t *L = lev_linedefs[i];

		if (! L->zero_len)
		{
			double x1 = L->start->x;
			double y1 = L->start->y;
			double x2 = L->end->x;
			double y2 = L->end->y;

			int lx = (int)floor(std::min(x1, x2));
			int ly = (int)floor(std::min(y1, y2));
			int hx = (int)ceil (std::max(x1, x2));
			int hy = (int)ceil (std::max(y1, y2));

			if (lx < bbox->minx) bbox->minx = lx;
			if (ly < bbox->miny) bbox->miny = ly;
			if (hx > bbox->maxx) bbox->maxx = hx;
			if (hy > bbox->maxy) bbox->maxy = hy;

			// compute middle of cluster
			mid_x += (lx + hx) >> 1;
			mid_y += (ly + hy) >> 1;
		}
	}

	if (num_linedefs > 0)
	{
		block_mid_x = I_ROUND(mid_x / (double)num_linedefs);
		block_mid_y = I_ROUND(mid_y / (double)num_linedefs);
	}

#if DEBUG_BLOCKMAP
	cur_info->Debug("Blockmap lines centered at (%d,%d)\n", block_mid_x, block_mid_y);
#endif
}


void InitBlockmap()
{
	bbox_t map_bbox;

	// find limits of linedefs, and store as map limits
	FindBlockmapLimits(&map_bbox);

	cur_info->Print_Verbose("    Map limits: (%d,%d) to (%d,%d)\n",
			map_bbox.minx, map_bbox.miny,
			map_bbox.maxx, map_bbox.maxy);

	block_x = map_bbox.minx - (map_bbox.minx & 0x7);
	block_y = map_bbox.miny - (map_bbox.miny & 0x7);

	block_w = ((map_bbox.maxx - block_x) / 128) + 1;
	block_h = ((map_bbox.maxy - block_y) / 128) + 1;

	block_count = block_w * block_h;
}


void PutBlockmap()
{
	if (! cur_info->do_blockmap || num_linedefs == 0)
	{
		// just create an empty blockmap lump
		CreateLevelLump("BLOCKMAP")->Finish();
		return;
	}

	block_overflowed = false;

	// initial phase: create internal blockmap containing the index of
	// all lines in each block.

	CreateBlockmap();

	// -AJA- second phase: compress the blockmap.  We do this by sorting
	//       the blocks, which is a typical way to detect duplicates in
	//       a large list.  This also detects BLOCKMAP overflow.

	CompressBlockmap();

	// final phase: write it out in the correct format

	if (block_overflowed)
	{
		// leave an empty blockmap lump
		CreateLevelLump("BLOCKMAP")->Finish();

		Warning("Blockmap overflowed (lump will be empty)\n");
	}
	else
	{
		WriteBlockmap();

		cur_info->Print_Verbose("    Blockmap size: %dx%d (compression: %d%%)\n",
				block_w, block_h, block_compression);
	}

	FreeBlockmap();
}


//------------------------------------------------------------------------
// REJECT : Generate the reject table
//------------------------------------------------------------------------


static uint8_t *rej_matrix;
static int      rej_total_size;	// in bytes


//
// Allocate the matrix, init sectors into individual groups.
//
static void Reject_Init()
{
	rej_total_size = (num_sectors * num_sectors + 7) / 8;

	rej_matrix = new uint8_t[rej_total_size];
	memset(rej_matrix, 0, rej_total_size);

	for (int i=0 ; i < num_sectors ; i++)
	{
		sector_t *sec = lev_sectors[i];

		sec->rej_group = i;
		sec->rej_next = sec->rej_prev = sec;
	}
}


static void Reject_Free()
{
	delete[] rej_matrix;
	rej_matrix = NULL;
}


//
// Algorithm: Initially all sectors are in individual groups.
// Now we scan the linedef list.  For each two-sectored line,
// merge the two sector groups into one.  That's it !
//
static void Reject_GroupSectors()
{
	for (int i=0 ; i < num_linedefs ; i++)
	{
		const linedef_t *line = lev_linedefs[i];

		if (! line->right || ! line->left)
			continue;

		sector_t *sec1 = line->right->sector;
		sector_t *sec2 = line->left->sector;
		sector_t *tmp;

		if (! sec1 || ! sec2 || sec1 == sec2)
			continue;

		// already in the same group ?
		if (sec1->rej_group == sec2->rej_group)
			continue;

		// swap sectors so that the smallest group is added to the biggest
		// group.  This is based on the assumption that sector numbers in
		// wads will generally increase over the set of linedefs, and so
		// (by swapping) we'll tend to add small groups into larger groups,
		// thereby minimising the updates to 'rej_group' fields when merging.

		if (sec1->rej_group > sec2->rej_group)
		{
			tmp = sec1; sec1 = sec2; sec2 = tmp;
		}

		// update the group numbers in the second group

		sec2->rej_group = sec1->rej_group;

		for (tmp=sec2->rej_next ; tmp != sec2 ; tmp=tmp->rej_next)
			tmp->rej_group = sec1->rej_group;

		// merge 'em baby...

		sec1->rej_next->rej_prev = sec2;
		sec2->rej_next->rej_prev = sec1;

		tmp = sec1->rej_next;
		sec1->rej_next = sec2->rej_next;
		sec2->rej_next = tmp;
	}
}


#if DEBUG_REJECT
static void Reject_DebugGroups()
{
	// Note: this routine is destructive to the group numbers

	for (int i=0 ; i < num_sectors ; i++)
	{
		sector_t *sec = lev_sectors[i];
		sector_t *tmp;

		int group = sec->rej_group;
		int num = 0;

		if (group < 0)
			continue;

		sec->rej_group = -1;
		num++;

		for (tmp=sec->rej_next ; tmp != sec ; tmp=tmp->rej_next)
		{
			tmp->rej_group = -1;
			num++;
		}

		cur_info->Debug("Group %d  Sectors %d\n", group, num);
	}
}
#endif


static void Reject_ProcessSectors()
{
	for (int view=0 ; view < num_sectors ; view++)
	{
		for (int target=0 ; target < view ; target++)
		{
			sector_t *view_sec = lev_sectors[view];
			sector_t *targ_sec = lev_sectors[target];

			if (view_sec->rej_group == targ_sec->rej_group)
				continue;

			// for symmetry, do both sides at same time

			int p1 = view * num_sectors + target;
			int p2 = target * num_sectors + view;

			rej_matrix[p1 >> 3] |= (1 << (p1 & 7));
			rej_matrix[p2 >> 3] |= (1 << (p2 & 7));
		}
	}
}


static void Reject_WriteLump()
{
	Lump_c *lump = CreateLevelLump("REJECT", rej_total_size);

	lump->Write(rej_matrix, rej_total_size);
	lump->Finish();
}


//
// For now we only do very basic reject processing, limited to
// determining all isolated groups of sectors (islands that are
// surrounded by void space).
//
void PutReject()
{
	if (! cur_info->do_reject || num_sectors == 0)
	{
		// just create an empty reject lump
		CreateLevelLump("REJECT")->Finish();
		return;
	}

	Reject_Init();
	Reject_GroupSectors();
	Reject_ProcessSectors();

#if DEBUG_REJECT
	Reject_DebugGroups();
#endif

	Reject_WriteLump();
	Reject_Free();

	cur_info->Print_Verbose("    Reject size: %d\n", rej_total_size);
}


//------------------------------------------------------------------------
// LEVEL : Level structure read/write functions.
//------------------------------------------------------------------------


// Note: ZDoom format support based on code (C) 2002,2003 Randy Heit


// per-level variables

const char *lev_current_name;

int lev_current_idx;
int lev_current_start;

map_format_e lev_format;
bool lev_force_xnod;

bool lev_long_name;
bool lev_overflows;


// objects of loaded level, and stuff we've built
std::vector<vertex_t *>  lev_vertices;
std::vector<linedef_t *> lev_linedefs;
std::vector<sidedef_t *> lev_sidedefs;
std::vector<sector_t *>  lev_sectors;
std::vector<thing_t *>   lev_things;

std::vector<seg_t *>     lev_segs;
std::vector<subsec_t *>  lev_subsecs;
std::vector<node_t *>    lev_nodes;
std::vector<walltip_t *> lev_walltips;

int num_old_vert = 0;
int num_new_vert = 0;
int num_real_lines = 0;


/* ----- allocation routines ---------------------------- */

vertex_t *NewVertex()
{
	vertex_t *V = (vertex_t *) UtilCalloc(sizeof(vertex_t));
	V->index = num_vertices;
	lev_vertices.push_back(V);
	return V;
}

linedef_t *NewLinedef()
{
	linedef_t *L = (linedef_t *) UtilCalloc(sizeof(linedef_t));
	L->index = num_linedefs;
	lev_linedefs.push_back(L);
	return L;
}

sidedef_t *NewSidedef()
{
	sidedef_t *S = (sidedef_t *) UtilCalloc(sizeof(sidedef_t));
	S->index = num_sidedefs;
	lev_sidedefs.push_back(S);
	return S;
}

sector_t *NewSector()
{
	sector_t *S = (sector_t *) UtilCalloc(sizeof(sector_t));
	S->index = num_sectors;
	lev_sectors.push_back(S);
	return S;
}

thing_t *NewThing()
{
	thing_t *T = (thing_t *) UtilCalloc(sizeof(thing_t));
	T->index = num_things;
	lev_things.push_back(T);
	return T;
}

seg_t *NewSeg()
{
	seg_t *S = (seg_t *) UtilCalloc(sizeof(seg_t));
	lev_segs.push_back(S);
	return S;
}

subsec_t *NewSubsec()
{
	subsec_t *S = (subsec_t *) UtilCalloc(sizeof(subsec_t));
	lev_subsecs.push_back(S);
	return S;
}

node_t *NewNode()
{
	node_t *N = (node_t *) UtilCalloc(sizeof(node_t));
	lev_nodes.push_back(N);
	return N;
}

walltip_t *NewWallTip()
{
	walltip_t *WT = (walltip_t *) UtilCalloc(sizeof(walltip_t));
	lev_walltips.push_back(WT);
	return WT;
}


/* ----- free routines ---------------------------- */

void FreeVertices()
{
	for (unsigned int i = 0 ; i < lev_vertices.size() ; i++)
		UtilFree((void *) lev_vertices[i]);

	lev_vertices.clear();
}

void FreeLinedefs()
{
	for (unsigned int i = 0 ; i < lev_linedefs.size() ; i++)
		UtilFree((void *) lev_linedefs[i]);

	lev_linedefs.clear();
}

void FreeSidedefs()
{
	for (unsigned int i = 0 ; i < lev_sidedefs.size() ; i++)
		UtilFree((void *) lev_sidedefs[i]);

	lev_sidedefs.clear();
}

void FreeSectors()
{
	for (unsigned int i = 0 ; i < lev_sectors.size() ; i++)
		UtilFree((void *) lev_sectors[i]);

	lev_sectors.clear();
}

void FreeThings()
{
	for (unsigned int i = 0 ; i < lev_things.size() ; i++)
		UtilFree((void *) lev_things[i]);

	lev_things.clear();
}

void FreeSegs()
{
	for (unsigned int i = 0 ; i < lev_segs.size() ; i++)
		UtilFree((void *) lev_segs[i]);

	lev_segs.clear();
}

void FreeSubsecs()
{
	for (unsigned int i = 0 ; i < lev_subsecs.size() ; i++)
		UtilFree((void *) lev_subsecs[i]);

	lev_subsecs.clear();
}

void FreeNodes()
{
	for (unsigned int i = 0 ; i < lev_nodes.size() ; i++)
		UtilFree((void *) lev_nodes[i]);

	lev_nodes.clear();
}

void FreeWallTips()
{
	for (unsigned int i = 0 ; i < lev_walltips.size() ; i++)
		UtilFree((void *) lev_walltips[i]);

	lev_walltips.clear();
}


/* ----- reading routines ------------------------------ */

static vertex_t *SafeLookupVertex(int num)
{
	if (num >= num_vertices)
		cur_info->FatalError("illegal vertex number #%d\n", num);

	return lev_vertices[num];
}

static sector_t *SafeLookupSector(uint16_t num)
{
	if (num == 0xFFFF)
		return NULL;

	if (num >= num_sectors)
		cur_info->FatalError("illegal sector number #%d\n", (int)num);

	return lev_sectors[num];
}

static inline sidedef_t *SafeLookupSidedef(uint16_t num)
{
	if (num == 0xFFFF)
		return NULL;

	// silently ignore illegal sidedef numbers
	if (num >= (unsigned int)num_sidedefs)
		return NULL;

	return lev_sidedefs[num];
}


void GetVertices()
{
	int count = 0;

	Lump_c *lump = FindLevelLump("VERTEXES");

	if (lump)
		count = lump->Length() / (int)sizeof(raw_vertex_t);

#if DEBUG_LOAD
	cur_info->Debug("GetVertices: num = %d\n", count);
#endif

	if (lump == NULL || count == 0)
		return;

	if (! lump->Seek(0))
		cur_info->FatalError("Error seeking to vertices.\n");

	for (int i = 0 ; i < count ; i++)
	{
		raw_vertex_t raw;

		if (! lump->Read(&raw, sizeof(raw)))
			cur_info->FatalError("Error reading vertices.\n");

		vertex_t *vert = NewVertex();

		vert->x = (double) LE_S16(raw.x);
		vert->y = (double) LE_S16(raw.y);
	}

	num_old_vert = num_vertices;
}


void GetSectors()
{
	int count = 0;

	Lump_c *lump = FindLevelLump("SECTORS");

	if (lump)
		count = lump->Length() / (int)sizeof(raw_sector_t);

	if (lump == NULL || count == 0)
		return;

	if (! lump->Seek(0))
		cur_info->FatalError("Error seeking to sectors.\n");

#if DEBUG_LOAD
	cur_info->Debug("GetSectors: num = %d\n", count);
#endif

	for (int i = 0 ; i < count ; i++)
	{
		raw_sector_t raw;

		if (! lump->Read(&raw, sizeof(raw)))
			cur_info->FatalError("Error reading sectors.\n");

		sector_t *sector = NewSector();

		(void) sector;
	}
}


void GetThings()
{
	int count = 0;

	Lump_c *lump = FindLevelLump("THINGS");

	if (lump)
		count = lump->Length() / (int)sizeof(raw_thing_t);

	if (lump == NULL || count == 0)
		return;

	if (! lump->Seek(0))
		cur_info->FatalError("Error seeking to things.\n");

#if DEBUG_LOAD
	cur_info->Debug("GetThings: num = %d\n", count);
#endif

	for (int i = 0 ; i < count ; i++)
	{
		raw_thing_t raw;

		if (! lump->Read(&raw, sizeof(raw)))
			cur_info->FatalError("Error reading things.\n");

		thing_t *thing = NewThing();

		thing->x    = LE_S16(raw.x);
		thing->y    = LE_S16(raw.y);
		thing->type = LE_U16(raw.type);
	}
}


void GetThingsHexen()
{
	int count = 0;

	Lump_c *lump = FindLevelLump("THINGS");

	if (lump)
		count = lump->Length() / (int)sizeof(raw_hexen_thing_t);

	if (lump == NULL || count == 0)
		return;

	if (! lump->Seek(0))
		cur_info->FatalError("Error seeking to things.\n");

#if DEBUG_LOAD
	cur_info->Debug("GetThingsHexen: num = %d\n", count);
#endif

	for (int i = 0 ; i < count ; i++)
	{
		raw_hexen_thing_t raw;

		if (! lump->Read(&raw, sizeof(raw)))
			cur_info->FatalError("Error reading things.\n");

		thing_t *thing = NewThing();

		thing->x    = LE_S16(raw.x);
		thing->y    = LE_S16(raw.y);
		thing->type = LE_U16(raw.type);
	}
}


void GetSidedefs()
{
	int count = 0;

	Lump_c *lump = FindLevelLump("SIDEDEFS");

	if (lump)
		count = lump->Length() / (int)sizeof(raw_sidedef_t);

	if (lump == NULL || count == 0)
		return;

	if (! lump->Seek(0))
		cur_info->FatalError("Error seeking to sidedefs.\n");

#if DEBUG_LOAD
	cur_info->Debug("GetSidedefs: num = %d\n", count);
#endif

	for (int i = 0 ; i < count ; i++)
	{
		raw_sidedef_t raw;

		if (! lump->Read(&raw, sizeof(raw)))
			cur_info->FatalError("Error reading sidedefs.\n");

		sidedef_t *side = NewSidedef();

		side->sector = SafeLookupSector(LE_S16(raw.sector));
	}
}


void GetLinedefs()
{
	int count = 0;

	Lump_c *lump = FindLevelLump("LINEDEFS");

	if (lump)
		count = lump->Length() / (int)sizeof(raw_linedef_t);

	if (lump == NULL || count == 0)
		return;

	if (! lump->Seek(0))
		cur_info->FatalError("Error seeking to linedefs.\n");

#if DEBUG_LOAD
	cur_info->Debug("GetLinedefs: num = %d\n", count);
#endif

	for (int i = 0 ; i < count ; i++)
	{
		raw_linedef_t raw;

		if (! lump->Read(&raw, sizeof(raw)))
			cur_info->FatalError("Error reading linedefs.\n");

		linedef_t *line;

		vertex_t *start = SafeLookupVertex(LE_U16(raw.start));
		vertex_t *end   = SafeLookupVertex(LE_U16(raw.end));

		start->is_used = true;
		  end->is_used = true;

		line = NewLinedef();

		line->start = start;
		line->end   = end;

		// check for zero-length line
		line->zero_len =
			(fabs(start->x - end->x) < DIST_EPSILON) &&
			(fabs(start->y - end->y) < DIST_EPSILON);

		line->type     = LE_U16(raw.type);
		uint16_t flags = LE_U16(raw.flags);
		int16_t tag    = LE_S16(raw.tag);

		line->two_sided   = (flags & MLF_TwoSided) != 0;
		line->is_precious = (tag >= 900 && tag < 1000);

		line->right = SafeLookupSidedef(LE_U16(raw.right));
		line->left  = SafeLookupSidedef(LE_U16(raw.left));

		if (line->right || line->left)
			num_real_lines++;

		line->self_ref = (line->left && line->right &&
				(line->left->sector == line->right->sector));
	}
}


void GetLinedefsHexen()
{
	int count = 0;

	Lump_c *lump = FindLevelLump("LINEDEFS");

	if (lump)
		count = lump->Length() / (int)sizeof(raw_hexen_linedef_t);

	if (lump == NULL || count == 0)
		return;

	if (! lump->Seek(0))
		cur_info->FatalError("Error seeking to linedefs.\n");

#if DEBUG_LOAD
	cur_info->Debug("GetLinedefsHexen: num = %d\n", count);
#endif

	for (int i = 0 ; i < count ; i++)
	{
		raw_hexen_linedef_t raw;

		if (! lump->Read(&raw, sizeof(raw)))
			cur_info->FatalError("Error reading linedefs.\n");

		linedef_t *line;

		vertex_t *start = SafeLookupVertex(LE_U16(raw.start));
		vertex_t *end   = SafeLookupVertex(LE_U16(raw.end));

		start->is_used = true;
		  end->is_used = true;

		line = NewLinedef();

		line->start = start;
		line->end   = end;

		// check for zero-length line
		line->zero_len =
			(fabs(start->x - end->x) < DIST_EPSILON) &&
			(fabs(start->y - end->y) < DIST_EPSILON);

		line->type     = (uint8_t) raw.type;
		uint16_t flags = LE_U16(raw.flags);

		// -JL- Added missing twosided flag handling that caused a broken reject
		line->two_sided = (flags & MLF_TwoSided) != 0;

		line->right = SafeLookupSidedef(LE_U16(raw.right));
		line->left  = SafeLookupSidedef(LE_U16(raw.left));

		if (line->right || line->left)
			num_real_lines++;

		line->self_ref = (line->left && line->right &&
				(line->left->sector == line->right->sector));
	}
}


static inline int VanillaSegDist(const seg_t *seg)
{
	double lx = seg->side ? seg->linedef->end->x : seg->linedef->start->x;
	double ly = seg->side ? seg->linedef->end->y : seg->linedef->start->y;

	// use the "true" starting coord (as stored in the wad)
	double sx = round(seg->start->x);
	double sy = round(seg->start->y);

	return (int) floor(hypot(sx - lx, sy - ly) + 0.5);
}

static inline int VanillaSegAngle(const seg_t *seg)
{
	// compute the "true" delta
	double dx = round(seg->end->x) - round(seg->start->x);
	double dy = round(seg->end->y) - round(seg->start->y);

	double angle = ComputeAngle(dx, dy);

	if (angle < 0)
		angle += 360.0;

	int result = (int) floor(angle * 65536.0 / 360.0 + 0.5);

	return (result & 0xFFFF);
}


/* ----- UDMF reading routines ------------------------- */

#define UDMF_THING    1
#define UDMF_VERTEX   2
#define UDMF_SECTOR   3
#define UDMF_SIDEDEF  4
#define UDMF_LINEDEF  5

void ParseThingField(thing_t *thing, const std::string& key, token_kind_e kind, const std::string& value)
{
	if (key == "x")
		thing->x = LEX_Double(value);

	if (key == "y")
		thing->y = LEX_Double(value);

	if (key == "type")
		thing->type = LEX_Double(value);
}


void ParseVertexField(vertex_t *vertex, const std::string& key, token_kind_e kind, const std::string& value)
{
	if (key == "x")
		vertex->x = LEX_Double(value);

	if (key == "y")
		vertex->y = LEX_Double(value);
}


void ParseSectorField(sector_t *sector, const std::string& key, token_kind_e kind, const std::string& value)
{
	// nothing actually needed
}


void ParseSidedefField(sidedef_t *side, const std::string& key, token_kind_e kind, const std::string& value)
{
	if (key == "sector")
	{
		int num = LEX_Int(value);

		if (num < 0 || num >= num_sectors)
			cur_info->FatalError("illegal sector number #%d\n", (int)num);

		side->sector = lev_sectors[num];
	}
}


void ParseLinedefField(linedef_t *line, const std::string& key, token_kind_e kind, const std::string& value)
{
	if (key == "v1")
		line->start = SafeLookupVertex(LEX_Int(value));

	if (key == "v2")
		line->end = SafeLookupVertex(LEX_Int(value));

	if (key == "special")
		line->type = LEX_Int(value);

	if (key == "twosided")
		line->two_sided = LEX_Boolean(value);

	if (key == "sidefront")
	{
		int num = LEX_Int(value);

		if (num < 0 || num >= (int)num_sidedefs)
			line->right = NULL;
		else
			line->right = lev_sidedefs[num];
	}

	if (key == "sideback")
	{
		int num = LEX_Int(value);

		if (num < 0 || num >= (int)num_sidedefs)
			line->left = NULL;
		else
			line->left = lev_sidedefs[num];
	}
}


void ParseUDMF_Block(lexer_c& lex, int cur_type)
{
	vertex_t  * vertex = NULL;
	thing_t   * thing  = NULL;
	sector_t  * sector = NULL;
	sidedef_t * side   = NULL;
	linedef_t * line   = NULL;

	switch (cur_type)
	{
		case UDMF_VERTEX:  vertex = NewVertex();  break;
		case UDMF_THING:   thing  = NewThing();   break;
		case UDMF_SECTOR:  sector = NewSector();  break;
		case UDMF_SIDEDEF: side   = NewSidedef(); break;
		case UDMF_LINEDEF: line   = NewLinedef(); break;
		default: break;
	}

	for (;;)
	{
		if (lex.Match("}"))
			break;

		std::string key;
		std::string value;

		token_kind_e tok = lex.Next(key);

		if (tok == TOK_EOF)
			cur_info->FatalError("Malformed TEXTMAP lump: unclosed block\n");

		if (tok != TOK_Ident)
			cur_info->FatalError("Malformed TEXTMAP lump: missing key\n");

		if (! lex.Match("="))
			cur_info->FatalError("Malformed TEXTMAP lump: missing '='\n");

		tok = lex.Next(value);

		if (tok == TOK_EOF || tok == TOK_ERROR || value == "}")
			cur_info->FatalError("Malformed TEXTMAP lump: missing value\n");

		if (! lex.Match(";"))
			cur_info->FatalError("Malformed TEXTMAP lump: missing ';'\n");

		switch (cur_type)
		{
			case UDMF_VERTEX:  ParseVertexField (vertex, key, tok, value); break;
			case UDMF_THING:   ParseThingField  (thing,  key, tok, value); break;
			case UDMF_SECTOR:  ParseSectorField (sector, key, tok, value); break;
			case UDMF_SIDEDEF: ParseSidedefField(side,   key, tok, value); break;
			case UDMF_LINEDEF: ParseLinedefField(line,   key, tok, value); break;

			default: /* just skip it */ break;
		}
	}

	// validate stuff

	if (line != NULL)
	{
		if (line->start == NULL || line->end == NULL)
			cur_info->FatalError("Linedef #%d is missing a vertex!\n", line->index);

		if (line->right || line->left)
			num_real_lines++;

		line->self_ref = (line->left && line->right &&
				(line->left->sector == line->right->sector));
	}
}


void ParseUDMF_Pass(const std::string& data, int pass)
{
	// pass = 1 : vertices, sectors, things
	// pass = 2 : sidedefs
	// pass = 3 : linedefs

	lexer_c lex(data);

	for (;;)
	{
		std::string section;
		token_kind_e tok = lex.Next(section);

		if (tok == TOK_EOF)
			return;

		if (tok != TOK_Ident)
		{
			cur_info->FatalError("Malformed TEXTMAP lump.\n");
			return;
		}

		// ignore top-level assignments
		if (lex.Match("="))
		{
			lex.Next(section);
			if (! lex.Match(";"))
				cur_info->FatalError("Malformed TEXTMAP lump: missing ';'\n");
			continue;
		}

		if (! lex.Match("{"))
			cur_info->FatalError("Malformed TEXTMAP lump: missing '{'\n");

		int cur_type = 0;

		if (section == "thing")
		{
			if (pass == 1)
				cur_type = UDMF_THING;
		}
		else if (section == "vertex")
		{
			if (pass == 1)
				cur_type = UDMF_VERTEX;
		}
		else if (section == "sector")
		{
			if (pass == 1)
				cur_type = UDMF_SECTOR;
		}
		else if (section == "sidedef")
		{
			if (pass == 2)
				cur_type = UDMF_SIDEDEF;
		}
		else if (section == "linedef")
		{
			if (pass == 3)
				cur_type = UDMF_LINEDEF;
		}

		// process the block
		ParseUDMF_Block(lex, cur_type);
	}
}


void ParseUDMF()
{
	Lump_c *lump = FindLevelLump("TEXTMAP");

	if (lump == NULL || ! lump->Seek(0))
		cur_info->FatalError("Error finding TEXTMAP lump.\n");

	int remain = lump->Length();

	// load the lump into this string
	std::string data;

	while (remain > 0)
	{
		char buffer[4096];

		int want = std::min(remain, (int)sizeof(buffer));

		if (! lump->Read(buffer, want))
			cur_info->FatalError("Error reading TEXTMAP lump.\n");

		data.append(buffer, want);

		remain -= want;
	}

	// now parse it...

	// the UDMF spec does not require objects to be in a dependency order.
	// for example: sidedefs may occur *after* the linedefs which refer to
	// them.  hence we perform multiple passes over the TEXTMAP data.

	ParseUDMF_Pass(data, 1);
	ParseUDMF_Pass(data, 2);
	ParseUDMF_Pass(data, 3);

	num_old_vert = num_vertices;
}


/* ----- writing routines ------------------------------ */

void MarkOverflow(int flags)
{
	// flags are ignored

	lev_overflows = true;
}


void PutVertices()
{
	int count, i;

	// this size is worst-case scenario
	int size = num_vertices * (int)sizeof(raw_vertex_t);

	Lump_c *lump = CreateLevelLump("VERTEXES", size);

	for (i=0, count=0 ; i < num_vertices ; i++)
	{
		raw_vertex_t raw;

		const vertex_t *vert = lev_vertices[i];

		if (vert->is_new)
		{
			continue;
		}

		raw.x = LE_S16(I_ROUND(vert->x));
		raw.y = LE_S16(I_ROUND(vert->y));

		lump->Write(&raw, sizeof(raw));

		count++;
	}

	lump->Finish();

	if (count != num_old_vert)
	{
		BugError("PutVertices miscounted (%d != %d)\n", count, num_old_vert);
	}

	if (count > 65534)
	{
		Failure("Number of vertices has overflowed.\n");
		MarkOverflow(LIMIT_VERTEXES);
	}
}


static inline uint16_t VertexIndex16Bit(const vertex_t *v)
{
	if (v->is_new)
		return (uint16_t) (v->index | 0x8000U);

	return (uint16_t) v->index;
}


static inline uint32_t VertexIndex_XNOD(const vertex_t *v)
{
	if (v->is_new)
		return (uint32_t) (num_old_vert + v->index);

	return (uint32_t) v->index;
}


void PutSegs()
{
	// this size is worst-case scenario
	int size = num_segs * (int)sizeof(raw_seg_t);

	Lump_c *lump = CreateLevelLump("SEGS", size);

	for (int i=0 ; i < num_segs ; i++)
	{
		raw_seg_t raw;

		const seg_t *seg = lev_segs[i];

		raw.start   = LE_U16(VertexIndex16Bit(seg->start));
		raw.end     = LE_U16(VertexIndex16Bit(seg->end));
		raw.angle   = LE_U16(VanillaSegAngle(seg));
		raw.linedef = LE_U16(seg->linedef->index);
		raw.flip    = LE_U16(seg->side);
		raw.dist    = LE_U16(VanillaSegDist(seg));

		lump->Write(&raw, sizeof(raw));

#if DEBUG_BSP
		cur_info->Debug("PUT SEG: %04X  Vert %04X->%04X  Line %04X %s  "
				"Angle %04X  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n", seg->index,
				LE_U16(raw.start), LE_U16(raw.end), LE_U16(raw.linedef),
				seg->side ? "L" : "R", LE_U16(raw.angle),
				seg->start->x, seg->start->y, seg->end->x, seg->end->y);
#endif
	}

	lump->Finish();

	if (num_segs > 65534)
	{
		Failure("Number of segs has overflowed.\n");
		MarkOverflow(LIMIT_SEGS);
	}
}


void PutSubsecs()
{
	int size = num_subsecs * (int)sizeof(raw_subsec_t);

	Lump_c * lump = CreateLevelLump("SSECTORS", size);

	for (int i=0 ; i < num_subsecs ; i++)
	{
		raw_subsec_t raw;

		const subsec_t *sub = lev_subsecs[i];

		raw.first = LE_U16(sub->seg_list->index);
		raw.num   = LE_U16(sub->seg_count);

		lump->Write(&raw, sizeof(raw));

#if DEBUG_BSP
		cur_info->Debug("PUT SUBSEC %04X  First %04X  Num %04X\n",
				sub->index, LE_U16(raw.first), LE_U16(raw.num));
#endif
	}

	if (num_subsecs > 32767)
	{
		Failure("Number of subsectors has overflowed.\n");
		MarkOverflow(LIMIT_SSECTORS);
	}

	lump->Finish();
}


static int node_cur_index;

static void PutOneNode(node_t *node, Lump_c *lump)
{
	if (node->r.node)
		PutOneNode(node->r.node, lump);

	if (node->l.node)
		PutOneNode(node->l.node, lump);

	node->index = node_cur_index++;

	raw_node_t raw;

	// note that x/y/dx/dy are always integral in non-UDMF maps
	raw.x  = LE_S16(I_ROUND(node->x));
	raw.y  = LE_S16(I_ROUND(node->y));
	raw.dx = LE_S16(I_ROUND(node->dx));
	raw.dy = LE_S16(I_ROUND(node->dy));

	raw.b1.minx = LE_S16(node->r.bounds.minx);
	raw.b1.miny = LE_S16(node->r.bounds.miny);
	raw.b1.maxx = LE_S16(node->r.bounds.maxx);
	raw.b1.maxy = LE_S16(node->r.bounds.maxy);

	raw.b2.minx = LE_S16(node->l.bounds.minx);
	raw.b2.miny = LE_S16(node->l.bounds.miny);
	raw.b2.maxx = LE_S16(node->l.bounds.maxx);
	raw.b2.maxy = LE_S16(node->l.bounds.maxy);

	if (node->r.node)
		raw.right = LE_U16(node->r.node->index);
	else if (node->r.subsec)
		raw.right = LE_U16(node->r.subsec->index | 0x8000);
	else
		BugError("Bad right child in node %d\n", node->index);

	if (node->l.node)
		raw.left = LE_U16(node->l.node->index);
	else if (node->l.subsec)
		raw.left = LE_U16(node->l.subsec->index | 0x8000);
	else
		BugError("Bad left child in node %d\n", node->index);

	lump->Write(&raw, sizeof(raw));

#if DEBUG_BSP
	cur_info->Debug("PUT NODE %04X  Left %04X  Right %04X  "
			"(%d,%d) -> (%d,%d)\n", node->index, LE_U16(raw.left),
			LE_U16(raw.right), node->x, node->y,
			node->x + node->dx, node->y + node->dy);
#endif
}


void PutNodes(node_t *root)
{
	int struct_size = (int)sizeof(raw_node_t);

	// this can be bigger than the actual size, but never smaller
	int max_size = (num_nodes + 1) * struct_size;

	Lump_c *lump = CreateLevelLump("NODES", max_size);

	node_cur_index = 0;

	if (root != NULL)
	{
		PutOneNode(root, lump);
	}

	lump->Finish();

	if (node_cur_index != num_nodes)
		BugError("PutNodes miscounted (%d != %d)\n", node_cur_index, num_nodes);

	if (node_cur_index > 32767)
	{
		Failure("Number of nodes has overflowed.\n");
		MarkOverflow(LIMIT_NODES);
	}
}


void CheckLimits()
{
	// this could potentially be 65536, since there are no reserved values
	// for sectors, but there may be source ports or tools treating 0xFFFF
	// as a special value, so we are extra cautious here (and in some of
	// the other checks below, like the vertex counts).
	if (num_sectors > 65535)
	{
		Failure("Map has too many sectors.\n");
		MarkOverflow(LIMIT_SECTORS);
	}

	// the sidedef 0xFFFF is reserved to mean "no side" in DOOM map format
	if (num_sidedefs > 65535)
	{
		Failure("Map has too many sidedefs.\n");
		MarkOverflow(LIMIT_SIDEDEFS);
	}

	// the linedef 0xFFFF is reserved for minisegs in GL nodes
	if (num_linedefs > 65535)
	{
		Failure("Map has too many linedefs.\n");
		MarkOverflow(LIMIT_LINEDEFS);
	}

	if (! (cur_info->force_xnod || cur_info->ssect_xgl3))
	{
		if (num_old_vert > 32767 ||
			num_new_vert > 32767 ||
			num_segs     > 32767 ||
			num_nodes    > 32767)
		{
			Warning("Forcing XNOD format nodes due to overflows.\n");
			lev_force_xnod = true;
		}
	}
}


struct Compare_seg_pred
{
	inline bool operator() (const seg_t *A, const seg_t *B) const
	{
		return A->index < B->index;
	}
};

void SortSegs()
{
	// do a sanity check
	for (int i = 0 ; i < num_segs ; i++)
		if (lev_segs[i]->index < 0)
			BugError("Seg %p never reached a subsector!\n", i);

	// sort segs into ascending index
	std::sort(lev_segs.begin(), lev_segs.end(), Compare_seg_pred());

	// remove unwanted segs
	while (lev_segs.size() > 0 && lev_segs.back()->index == SEG_IS_GARBAGE)
	{
		UtilFree((void *) lev_segs.back());
		lev_segs.pop_back();
	}
}


/* ----- ZDoom format writing --------------------------- */

void PutZVertices(Lump_c *lump)
{
	int count, i;

	uint32_t orgverts = LE_U32(num_old_vert);
	uint32_t newverts = LE_U32(num_new_vert);

	lump->Write(&orgverts, 4);
	lump->Write(&newverts, 4);

	for (i=0, count=0 ; i < num_vertices ; i++)
	{
		raw_zdoom_vertex_t raw;

		const vertex_t *vert = lev_vertices[i];

		if (! vert->is_new)
			continue;

		raw.x = LE_S32(I_ROUND(vert->x * 65536.0));
		raw.y = LE_S32(I_ROUND(vert->y * 65536.0));

		lump->Write(&raw, sizeof(raw));

		count++;
	}

	if (count != num_new_vert)
		BugError("PutZVertices miscounted (%d != %d)\n", count, num_new_vert);
}


void PutZSubsecs(Lump_c *lump)
{
	uint32_t raw_num = LE_U32(num_subsecs);
	lump->Write(&raw_num, 4);

	int cur_seg_index = 0;

	for (int i=0 ; i < num_subsecs ; i++)
	{
		const subsec_t *sub = lev_subsecs[i];

		raw_num = LE_U32(sub->seg_count);
		lump->Write(&raw_num, 4);

		// sanity check the seg index values
		int count = 0;
		for (const seg_t *seg = sub->seg_list ; seg ; seg = seg->next, cur_seg_index++)
		{
			if (cur_seg_index != seg->index)
				BugError("PutZSubsecs: seg index mismatch in sub %d (%d != %d)\n",
						i, cur_seg_index, seg->index);

			count++;
		}

		if (count != sub->seg_count)
			BugError("PutZSubsecs: miscounted segs in sub %d (%d != %d)\n",
					i, count, sub->seg_count);
	}

	if (cur_seg_index != num_segs)
		BugError("PutZSubsecs miscounted segs (%d != %d)\n", cur_seg_index, num_segs);
}


void PutZSegs(Lump_c *lump)
{
	uint32_t raw_num = LE_U32(num_segs);
	lump->Write(&raw_num, 4);

	for (int i=0 ; i < num_segs ; i++)
	{
		const seg_t *seg = lev_segs[i];

		if (seg->index != i)
			BugError("PutZSegs: seg index mismatch (%d != %d)\n", seg->index, i);

		uint32_t v1 = LE_U32(VertexIndex_XNOD(seg->start));
		uint32_t v2 = LE_U32(VertexIndex_XNOD(seg->end));

		uint16_t line = LE_U16(seg->linedef->index);
		uint8_t  side = (uint8_t) seg->side;

		lump->Write(&v1,   4);
		lump->Write(&v2,   4);
		lump->Write(&line, 2);
		lump->Write(&side, 1);
	}
}


void PutXGL3Segs(Lump_c *lump)
{
	uint32_t raw_num = LE_U32(num_segs);
	lump->Write(&raw_num, 4);

	for (int i=0 ; i < num_segs ; i++)
	{
		const seg_t *seg = lev_segs[i];

		if (seg->index != i)
			BugError("PutXGL3Segs: seg index mismatch (%d != %d)\n", seg->index, i);

		uint32_t v1      = LE_U32(VertexIndex_XNOD(seg->start));
		uint32_t partner = LE_U32(seg->partner ? seg->partner->index : -1);
		uint32_t line    = LE_U32(seg->linedef ? seg->linedef->index : -1);
		uint8_t  side    = (uint8_t) seg->side;

		lump->Write(&v1,      4);
		lump->Write(&partner, 4);
		lump->Write(&line,    4);
		lump->Write(&side,    1);

#if DEBUG_BSP
		fprintf(stderr, "SEG[%d] v1=%d partner=%d line=%d side=%d\n", i, v1, partner, line, side);
#endif
	}
}


static void PutOneZNode(Lump_c *lump, node_t *node, bool xgl3)
{
	raw_zdoom_node_t raw;

	if (node->r.node)
		PutOneZNode(lump, node->r.node, xgl3);

	if (node->l.node)
		PutOneZNode(lump, node->l.node, xgl3);

	node->index = node_cur_index++;

	if (xgl3)
	{
		uint32_t x  = LE_S32(I_ROUND(node->x  * 65536.0));
		uint32_t y  = LE_S32(I_ROUND(node->y  * 65536.0));
		uint32_t dx = LE_S32(I_ROUND(node->dx * 65536.0));
		uint32_t dy = LE_S32(I_ROUND(node->dy * 65536.0));

		lump->Write(&x,  4);
		lump->Write(&y,  4);
		lump->Write(&dx, 4);
		lump->Write(&dy, 4);
	}
	else
	{
		raw.x  = LE_S16(I_ROUND(node->x));
		raw.y  = LE_S16(I_ROUND(node->y));
		raw.dx = LE_S16(I_ROUND(node->dx));
		raw.dy = LE_S16(I_ROUND(node->dy));

		lump->Write(&raw.x,  2);
		lump->Write(&raw.y,  2);
		lump->Write(&raw.dx, 2);
		lump->Write(&raw.dy, 2);
	}

	raw.b1.minx = LE_S16(node->r.bounds.minx);
	raw.b1.miny = LE_S16(node->r.bounds.miny);
	raw.b1.maxx = LE_S16(node->r.bounds.maxx);
	raw.b1.maxy = LE_S16(node->r.bounds.maxy);

	raw.b2.minx = LE_S16(node->l.bounds.minx);
	raw.b2.miny = LE_S16(node->l.bounds.miny);
	raw.b2.maxx = LE_S16(node->l.bounds.maxx);
	raw.b2.maxy = LE_S16(node->l.bounds.maxy);

	lump->Write(&raw.b1, sizeof(raw.b1));
	lump->Write(&raw.b2, sizeof(raw.b2));

	if (node->r.node)
		raw.right = LE_U32(node->r.node->index);
	else if (node->r.subsec)
		raw.right = LE_U32(node->r.subsec->index | 0x80000000U);
	else
		BugError("Bad right child in ZDoom node %d\n", node->index);

	if (node->l.node)
		raw.left = LE_U32(node->l.node->index);
	else if (node->l.subsec)
		raw.left = LE_U32(node->l.subsec->index | 0x80000000U);
	else
		BugError("Bad left child in ZDoom node %d\n", node->index);

	lump->Write(&raw.right, 4);
	lump->Write(&raw.left,  4);

#if DEBUG_BSP
	cur_info->Debug("PUT Z NODE %08X  Left %08X  Right %08X  "
			"(%d,%d) -> (%d,%d)\n", node->index, LE_U32(raw.left),
			LE_U32(raw.right), node->x, node->y,
			node->x + node->dx, node->y + node->dy);
#endif
}


void PutZNodes(Lump_c *lump, node_t *root, bool xgl3)
{
	uint32_t raw_num = LE_U32(num_nodes);
	lump->Write(&raw_num, 4);

	node_cur_index = 0;

	if (root)
		PutOneZNode(lump, root, xgl3);

	if (node_cur_index != num_nodes)
		BugError("PutZNodes miscounted (%d != %d)\n", node_cur_index, num_nodes);
}


static int CalcZDoomNodesSize()
{
	// compute size of the ZDoom format nodes.
	// it does not need to be exact, but it *does* need to be bigger
	// (or equal) to the actual size of the lump.

	int size = 32;  // header + a bit extra

	size += 8 + num_vertices * 8;
	size += 4 + num_subsecs  * 4;
	size += 4 + num_segs     * 11;
	size += 4 + num_nodes    * sizeof(raw_zdoom_node_t);

	return size;
}


void SaveZDFormat(node_t *root_node)
{
	SortSegs();

	int max_size = CalcZDoomNodesSize();

	Lump_c *lump = CreateLevelLump("NODES", max_size);

	lump->Write(XNOD_MAGIC, 4);

	PutZVertices(lump);
	PutZSubsecs(lump);
	PutZSegs(lump);
	PutZNodes(lump, root_node, false);

	lump->Finish();
	lump = NULL;
}


void SaveXGL3Format(Lump_c *lump, node_t *root_node)
{
	SortSegs();

	// WISH : compute a max_size

	lump->Write(XGL3_MAGIC, 4);

	PutZVertices(lump);
	PutZSubsecs(lump);
	PutXGL3Segs(lump);
	PutZNodes(lump, root_node, true);

	lump->Finish();
	lump = NULL;
}


/* ----- whole-level routines --------------------------- */

void LoadLevel()
{
	Lump_c *LEV = cur_wad->GetLump(lev_current_start);

	lev_current_name = LEV->Name();
	lev_long_name = false;
	lev_overflows = false;

	cur_info->ShowMap(lev_current_name);

	num_new_vert   = 0;
	num_real_lines = 0;

	if (lev_format == MAPF_UDMF)
	{
		ParseUDMF();
	}
	else
	{
		GetVertices();
		GetSectors();
		GetSidedefs();

		if (lev_format == MAPF_Hexen)
		{
			GetLinedefsHexen();
			GetThingsHexen();
		}
		else
		{
			GetLinedefs();
			GetThings();
		}

		// always prune vertices at end of lump, otherwise all the
		// unused vertices from seg splits would keep accumulating.
		PruneVerticesAtEnd();
	}

	cur_info->Print_Verbose("    Loaded %d vertices, %d sectors, %d sides, %d lines, %d things\n",
				num_vertices, num_sectors, num_sidedefs, num_linedefs, num_things);

	DetectOverlappingVertices();
	DetectOverlappingLines();

	CalculateWallTips();

	// -JL- Find sectors containing polyobjs
	switch (lev_format)
	{
		case MAPF_Hexen: DetectPolyobjSectors(false); break;
		case MAPF_UDMF:  DetectPolyobjSectors(true);  break;
		default:         break;
	}
}


void FreeLevel()
{
	FreeVertices();
	FreeSidedefs();
	FreeLinedefs();
	FreeSectors();
	FreeThings();
	FreeSegs();
	FreeSubsecs();
	FreeNodes();
	FreeWallTips();
	FreeIntersections();
}


static void AddMissingLump(const char *name, const char *after)
{
	if (cur_wad->LevelLookupLump(lev_current_idx, name) >= 0)
		return;

	int exist = cur_wad->LevelLookupLump(lev_current_idx, after);

	// if this happens, the level structure is very broken
	if (exist < 0)
	{
		Warning("Missing %s lump -- level structure is broken\n", after);

		exist = cur_wad->LevelLastLump(lev_current_idx);
	}

	cur_wad->InsertPoint(exist + 1);

	cur_wad->AddLump(name)->Finish();
}


build_result_e SaveLevel(node_t *root_node)
{
	// Note: root_node may be NULL

	cur_wad->BeginWrite();

	// ensure all necessary level lumps are present
	AddMissingLump("SEGS",     "VERTEXES");
	AddMissingLump("SSECTORS", "SEGS");
	AddMissingLump("NODES",    "SSECTORS");
	AddMissingLump("REJECT",   "SECTORS");
	AddMissingLump("BLOCKMAP", "REJECT");

	// user preferences
	lev_force_xnod = cur_info->force_xnod;

	// check for overflows...
	// this sets the force_xxx vars if certain limits are breached
	CheckLimits();


	/* --- Normal nodes --- */

	if ((lev_force_xnod || cur_info->ssect_xgl3) && num_real_lines > 0)
	{
		// leave SEGS empty
		CreateLevelLump("SEGS")->Finish();

		if (cur_info->ssect_xgl3)
		{
			Lump_c *lump = CreateLevelLump("SSECTORS");
			SaveXGL3Format(lump, root_node);
		}
		else
		{
			CreateLevelLump("SSECTORS")->Finish();
		}

		if (lev_force_xnod)
		{
			// remove all the mini-segs from subsectors
			NormaliseBspTree();

			SaveZDFormat(root_node);
		}
		else
		{
			CreateLevelLump("NODES")->Finish();
		}
	}
	else
	{
		// remove all the mini-segs from subsectors
		NormaliseBspTree();

		// reduce vertex precision for classic DOOM nodes.
		// some segs can become "degenerate" after this, and these
		// are removed from subsectors.
		RoundOffBspTree();

		SortSegs();

		PutVertices();

		PutSegs();
		PutSubsecs();
		PutNodes(root_node);
	}


	PutBlockmap();
	PutReject();

	cur_wad->EndWrite();

	if (lev_overflows)
	{
		// no message here
		// [ in verbose mode, each overflow already printed a message ]
		// [ in normal mode, we don't want any messages at all ]

		return BUILD_LumpOverflow;
	}

	return BUILD_OK;
}


build_result_e SaveUDMF(node_t *root_node)
{
	cur_wad->BeginWrite();

	// remove any existing ZNODES lump
	cur_wad->RemoveZNodes(lev_current_idx);

	Lump_c *lump = CreateLevelLump("ZNODES", -1);

	// [EA] Ensure needed lumps exist
	AddMissingLump("REJECT",   "ZNODES");
	AddMissingLump("BLOCKMAP", "REJECT");

	if (num_real_lines == 0)
	{
		lump->Finish();
	}
	else
	{
		SaveXGL3Format(lump, root_node);
	}

	// [EA]
	PutBlockmap();
	PutReject();

	cur_wad->EndWrite();

	return BUILD_OK;
}


/* ---------------------------------------------------------------- */

Lump_c * FindLevelLump(const char *name)
{
	int idx = cur_wad->LevelLookupLump(lev_current_idx, name);

	if (idx < 0)
		return NULL;

	return cur_wad->GetLump(idx);
}


Lump_c * CreateLevelLump(const char *name, int max_size)
{
	// look for existing one
	Lump_c *lump = FindLevelLump(name);

	if (lump)
	{
		cur_wad->RecreateLump(lump, max_size);
	}
	else
	{
		int last_idx = cur_wad->LevelLastLump(lev_current_idx);

		// in UDMF maps, insert before the ENDMAP lump, otherwise insert
		// after the last known lump of the level.
		if (lev_format != MAPF_UDMF)
			last_idx += 1;

		cur_wad->InsertPoint(last_idx);

		lump = cur_wad->AddLump(name, max_size);
	}

	return lump;
}


//------------------------------------------------------------------------
// MAIN STUFF
//------------------------------------------------------------------------

buildinfo_t * cur_info = NULL;

void SetInfo(buildinfo_t *info)
{
	cur_info = info;
}


void OpenWad(const char *filename)
{
	cur_wad = Wad_file::Open(filename, 'a');
	if (cur_wad == NULL)
		cur_info->FatalError("Cannot open file: %s\n", filename);

	if (cur_wad->IsReadOnly())
	{
		delete cur_wad;
		cur_wad = NULL;

		cur_info->FatalError("file is read only: %s\n", filename);
	}
}


void CloseWad()
{
	if (cur_wad != NULL)
	{
		// this closes the file
		delete cur_wad;
		cur_wad = NULL;
	}
}


int LevelsInWad()
{
	if (cur_wad == NULL)
		return 0;

	return cur_wad->LevelCount();
}


const char * GetLevelName(int lev_idx)
{
	SYS_ASSERT(cur_wad != NULL);

	int lump_idx = cur_wad->LevelHeader(lev_idx);

	return cur_wad->GetLump(lump_idx)->Name();
}


/* ----- build nodes for a single level ----- */

build_result_e BuildLevel(int lev_idx)
{
	if (cur_info->cancelled)
		return BUILD_Cancelled;

	node_t   *root_node = NULL;
	subsec_t *root_sub  = NULL;

	lev_current_idx   = lev_idx;
	lev_current_start = cur_wad->LevelHeader(lev_idx);
	lev_format        = cur_wad->LevelFormat(lev_idx);

	LoadLevel();

	InitBlockmap();

	build_result_e ret = BUILD_OK;

	if (num_real_lines > 0)
	{
		bbox_t dummy;

		// create initial segs
		seg_t *list = CreateSegs();

		// recursively create nodes
		ret = BuildNodes(list, 0, &dummy, &root_node, &root_sub);
	}

	if (ret == BUILD_OK)
	{
		cur_info->Print_Verbose("    Built %d NODES, %d SSECTORS, %d SEGS, %d VERTEXES\n",
				num_nodes, num_subsecs, num_segs, num_old_vert + num_new_vert);

		if (root_node != NULL)
		{
			cur_info->Print_Verbose("    Heights of subtrees: %d / %d\n",
					ComputeBspHeight(root_node->r.node),
					ComputeBspHeight(root_node->l.node));
		}

		ClockwiseBspTree();

		switch (lev_format)
		{
			case MAPF_Doom:
			case MAPF_Hexen: ret = SaveLevel(root_node); break;
			case MAPF_UDMF:  ret = SaveUDMF(root_node);  break;
			default: break;
		}
	}
	else
	{
		/* build was Cancelled by the user */
	}

	FreeLevel();

	return ret;
}


}  // namespace elfbsp


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
