//------------------------------------------------------------------------
//
//  AJ-BSP  Copyright (C) 2000-2018  Andrew Apted, et al
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

#ifndef __AJBSP_BSP_H__
#define __AJBSP_BSP_H__

class Lump_c;


// Node Build Information Structure
//
// Memory note: when changing the string values here (and in
// nodebuildcomms_t) they should be freed using StringFree() and
// allocated with StringDup().  The application has the final
// responsibility to free the strings in here.
//
#define DEFAULT_FACTOR  11

class nodebuildinfo_t
{
public:
	int factor;

	bool gl_nodes;

	// when these two are false, they create an empty lump
	bool do_blockmap;
	bool do_reject;

	bool fast;
	bool warnings;	// NOTE: not currently used

	bool force_v5;
	bool force_xnod;
	bool force_compress;	// NOTE: only supported when HAVE_ZLIB is defined

	// the GUI can set this to tell the node builder to stop
	bool cancelled;

	// from here on, various bits of internal state
	int total_failed_maps;
	int total_warnings;
	int total_minor_issues;

public:
	nodebuildinfo_t() :
		factor(DEFAULT_FACTOR),

		gl_nodes(true),

		do_blockmap(true),
		do_reject  (true),

		fast(false),
		warnings(false),

		force_v5(false),
		force_xnod(false),
		force_compress(false),

		cancelled(false),

		total_failed_maps(0),
		total_warnings(0),
		total_minor_issues(0)
	{ }

	~nodebuildinfo_t()
	{ }
};


typedef enum
{
	// everything went peachy keen
	BUILD_OK = 0,

	// building was cancelled
	BUILD_Cancelled,

	// the WAD file was corrupt / empty / bad filename
	BUILD_BadFile,

	// when saving the map, one or more lumps overflowed
	BUILD_LumpOverflow
}
build_result_e;


build_result_e AJBSP_BuildLevel(nodebuildinfo_t *info, short lev_idx);


//======================================================================
//
//    INTERNAL STUFF FROM HERE ON
//
//======================================================================

namespace ajbsp
{


// internal storage of node building parameters

extern nodebuildinfo_t * cur_info;



/* ----- basic types --------------------------- */

typedef double angle_g;  // degrees, 0 is E, 90 is N


//------------------------------------------------------------------------
// UTILITY : general purpose functions
//------------------------------------------------------------------------

void Failure(const char *fmt, ...);
void Warning(const char *fmt, ...);
void MinorIssue(const char *fmt, ...);

// allocate and clear some memory.  guaranteed not to fail.
void *UtilCalloc(int size);

// re-allocate some memory.  guaranteed not to fail.
void *UtilRealloc(void *old, int size);

// free some memory or a string.
void UtilFree(void *data);

// return an allocated string for the current data and time,
// or NULL if an error occurred.
char *UtilTimeString(void);

// compute angle of line from (0,0) to (dx,dy)
angle_g UtilComputeAngle(double dx, double dy);

// checksum functions
void Adler32_Begin(u32_t *crc);
void Adler32_AddBlock(u32_t *crc, const u8_t *data, int length);
void Adler32_Finish(u32_t *crc);


//------------------------------------------------------------------------
// BLOCKMAP : Generate the blockmap
//------------------------------------------------------------------------

// compute blockmap origin & size (the block_x/y/w/h variables)
// based on the set of loaded linedefs.
//
void InitBlockmap();

// build the blockmap and write the data into the BLOCKMAP lump
void PutBlockmap();

// utility routines...
void GetBlockmapBounds(int *x, int *y, int *w, int *h);

int CheckLinedefInsideBox(int xmin, int ymin, int xmax, int ymax,
    int x1, int y1, int x2, int y2);


//------------------------------------------------------------------------
// REJECT : Generate the reject table
//------------------------------------------------------------------------

// build the reject table and write it into the REJECT lump
void PutReject();


//------------------------------------------------------------------------
// LEVEL : Level structures & read/write functions.
//------------------------------------------------------------------------

struct node_s;
struct sector_s;
struct superblock_s;


// a wall-tip is where a wall meets a vertex
typedef struct walltip_s
{
	// link in list.  List is kept in ANTI-clockwise order.
	struct walltip_s *next;
	struct walltip_s *prev;

	// angle that line makes at vertex (degrees).
	angle_g angle;

	// whether each side of wall is OPEN or CLOSED.
	// left is the side of increasing angles, whereas
	// right is the side of decreasing angles.
	bool open_left;
	bool open_right;
}
walltip_t;


typedef struct vertex_s
{
	// coordinates
	double x, y;

	// vertex index.  Always valid after loading and pruning of unused
	// vertices has occurred.
	int index;

	// vertex is newly created (from a seg split)
	bool is_new;

	// when building normal nodes, unused vertices will be pruned.
	bool is_used;

	// usually NULL, unless this vertex occupies the same location as a
	// previous vertex.
	struct vertex_s *overlap;

	// list of wall-tips
	walltip_t *tip_set;
}
vertex_t;


typedef struct sector_s
{
	// sector index.  Always valid after loading & pruning.
	int index;

	// allow segs from other sectors to coexist in a subsector.
	char coalesce;

	// -JL- non-zero if this sector contains a polyobj.
	char has_polyobj;

	// when building normal nodes, unused sectors will be pruned.
	char is_used;

	// heights
	int floor_h, ceil_h;

	// textures
	char floor_tex[8];
	char ceil_tex[8];

	// attributes
	int light;
	int special;
	int tag;

	// used when building REJECT table.  Each set of sectors that are
	// isolated from other sectors will have a different group number.
	// Thus: on every 2-sided linedef, the sectors on both sides will be
	// in the same group.  The rej_next, rej_prev fields are a link in a
	// RING, containing all sectors of the same group.
	int rej_group;

	struct sector_s *rej_next;
	struct sector_s *rej_prev;

	// suppress superfluous mini warnings
	int warned_facing;
	char warned_unclosed;
}
sector_t;


typedef struct sidedef_s
{
	// adjacent sector.  Can be NULL (invalid sidedef)
	sector_t *sector;

	// offset values
	int x_offset, y_offset;

	// texture names
	char upper_tex[8];
	char lower_tex[8];
	char mid_tex[8];

	// sidedef index.  Always valid after loading & pruning.
	int index;

	// when building normal nodes, unused sidedefs will be pruned.
	char is_used;

	// usually NULL, unless this sidedef is exactly the same as a
	// previous one.  Only used during the pruning phase.
	struct sidedef_s *equiv;

	// this is true if the sidedef is on a special line.  We don't merge
	// these sidedefs together, as they might scroll, or change texture
	// when a switch is pressed.
	int on_special;
}
sidedef_t;


typedef struct linedef_s
{
	// link for list
	struct linedef_s *next;

	vertex_t *start;    // from this vertex...
	vertex_t *end;      // ... to this vertex

	sidedef_t *right;   // right sidedef
	sidedef_t *left;    // left sidede, or NULL if none

	// line is marked two-sided
	bool two_sided;

	// prefer not to split
	bool is_precious;

	// zero length (line should be totally ignored)
	bool zero_len;

	// sector is the same on both sides
	bool self_ref;

	int flags;
	int type;
	int tag;

	// Hexen support
	int specials[5];

	// normally NULL, except when this linedef directly overlaps an earlier
	// one (a rarely-used trick to create higher mid-masked textures).
	// No segs should be created for these overlapping linedefs.
	struct linedef_s *overlap;

	// linedef index.  Always valid after loading & pruning of zero
	// length lines has occurred.
	int index;
}
linedef_t;


typedef struct thing_s
{
	int x, y;
	int type;
	int options;

	// other info (angle, and hexen stuff) omitted.  We don't need to
	// write the THING lump, only read it.

	// Always valid (thing indices never change).
	int index;
}
thing_t;


typedef struct seg_s
{
	// link for list
	struct seg_s *next;

	vertex_t *start;   // from this vertex...
	vertex_t *end;     // ... to this vertex

	// linedef that this seg goes along, or NULL if miniseg
	linedef_t *linedef;

	// 0 for right, 1 for left
	int side;

	// seg on other side, or NULL if one-sided.  This relationship is
	// always one-to-one -- if one of the segs is split, the partner seg
	// must also be split.
	struct seg_s *partner;

	// seg index.  Only valid once the seg has been added to a
	// subsector.  A negative value means it is invalid -- there
	// shouldn't be any of these once the BSP tree has been built.
	int index;

	// when 1, this seg has become zero length (integer rounding of the
	// start and end vertices produces the same location).  It should be
	// ignored when writing the SEGS or V1 GL_SEGS lumps.  [Note: there
	// won't be any of these when writing the V2 GL_SEGS lump].
	bool is_degenerate;

	// the superblock that contains this seg, or NULL if the seg is no
	// longer in any superblock (e.g. now in a subsector).
	struct superblock_s *block;

	// precomputed data for faster calculations
	double psx, psy;
	double pex, pey;
	double pdx, pdy;

	double p_length;
	double p_para;
	double p_perp;

	// linedef that this seg initially comes from.  For "real" segs,
	// this is just the same as the 'linedef' field above.  For
	// "minisegs", this is the linedef of the partition line.
	linedef_t *source_line;

public:
	int PointOnLineSide(double x, double y) const;

	// compute the parallel and perpendicular distances from a partition
	// line to a point.
	//
	inline double ParallelDist(double x, double y) const
	{
		return (x * pdx + y * pdy + p_para) / p_length;
	}

	inline double PerpDist(double x, double y) const
	{
		return (x * pdy - y * pdx + p_perp) / p_length;
	}
}
seg_t;


typedef struct subsec_s
{
	// list of segs
	seg_t *seg_list;

	// count of segs
	int seg_count;

	// subsector index.  Always valid, set when the subsector is
	// initially created.
	int index;

	// approximate middle point
	double mid_x;
	double mid_y;
}
subsec_t;


typedef struct bbox_s
{
	int minx, miny;
	int maxx, maxy;
}
bbox_t;


typedef struct child_s
{
	// child node or subsector (one must be NULL)
	struct node_s *node;
	subsec_t *subsec;

	// child bounding box
	bbox_t bounds;
}
child_t;


typedef struct node_s
{
	int x, y;     // starting point
	int dx, dy;   // offset to ending point

	// right & left children
	child_t r;
	child_t l;

	// node index.  Only valid once the NODES or GL_NODES lump has been
	// created.
	int index;

	// the node is too long, and the (dx,dy) values should be halved
	// when writing into the NODES lump.
	int too_long;
}
node_t;


typedef struct superblock_s
{
	// parent of this block, or NULL for a top-level block
	struct superblock_s *parent;

	// coordinates on map for this block, from lower-left corner to
	// upper-right corner.  Pseudo-inclusive, i.e (x,y) is inside block
	// if and only if x1 <= x < x2 and y1 <= y < y2.
	int x1, y1;
	int x2, y2;

	// sub-blocks.  NULL when empty.  [0] has the lower coordinates, and
	// [1] has the higher coordinates.  Division of a square always
	// occurs horizontally (e.g. 512x512 -> 256x512 -> 256x256).
	struct superblock_s *subs[2];

	// number of real segs and minisegs contained by this block
	// (including all sub-blocks below it).
	int real_num;
	int mini_num;

	// list of segs completely contained by this block.
	seg_t *segs;
}
superblock_t;

#define SUPER_IS_LEAF(s)  \
	((s)->x2 - (s)->x1 <= 256 && (s)->y2 - (s)->y1 <= 256)


/* ----- Level data arrays ----------------------- */

extern std::vector<vertex_t *>  lev_vertices;
extern std::vector<linedef_t *> lev_linedefs;
extern std::vector<sidedef_t *> lev_sidedefs;
extern std::vector<sector_t *>  lev_sectors;
extern std::vector<thing_t *>   lev_things;

extern std::vector<seg_t *>     lev_segs;
extern std::vector<subsec_t *>  lev_subsecs;
extern std::vector<node_t *>    lev_nodes;
extern std::vector<walltip_t *> lev_walltips;

#define num_vertices  ((int)lev_vertices.size())
#define num_linedefs  ((int)lev_linedefs.size())
#define num_sidedefs  ((int)lev_sidedefs.size())
#define num_sectors   ((int)lev_sectors.size())
#define num_things    ((int)lev_things.size())

#define num_segs      ((int)lev_segs.size())
#define num_subsecs   ((int)lev_subsecs.size())
#define num_nodes     ((int)lev_nodes.size())
#define num_walltips  ((int)lev_walltips.size())

extern int num_old_vert;
extern int num_new_vert;
extern int num_complete_seg;


/* ----- function prototypes ----------------------- */

// allocation routines
vertex_t  *NewVertex();
linedef_t *NewLinedef();
sidedef_t *NewSidedef();
sector_t  *NewSector();
thing_t   *NewThing();

seg_t     *NewSeg();
subsec_t  *NewSubsec();
node_t    *NewNode();
walltip_t *NewWallTip();

Lump_c * CreateGLMarker();
Lump_c * CreateLevelLump(const char *name, int max_size = -1);
Lump_c * FindLevelLump(const char *name);

// Zlib compression support
void ZLibBeginLump(Lump_c *lump);
void ZLibAppendLump(const void *data, int length);
void ZLibFinishLump(void);

/* limit flags, to show what went wrong */
#define LIMIT_VERTEXES     0x000001
#define LIMIT_SECTORS      0x000002
#define LIMIT_SIDEDEFS     0x000004
#define LIMIT_LINEDEFS     0x000008

#define LIMIT_SEGS         0x000010
#define LIMIT_SSECTORS     0x000020
#define LIMIT_NODES        0x000040

#define LIMIT_GL_VERT      0x000100
#define LIMIT_GL_SEGS      0x000200
#define LIMIT_GL_SSECT     0x000400
#define LIMIT_GL_NODES     0x000800


//------------------------------------------------------------------------
// ANALYZE : Analyzing level structures
//------------------------------------------------------------------------


// detection routines
void DetectOverlappingVertices(void);
void DetectOverlappingLines(void);
void DetectPolyobjSectors(void);

// pruning routines
void PruneVerticesAtEnd(void);

// computes the wall tips for all of the vertices
void CalculateWallTips(void);

// return a new vertex (with correct wall-tip info) for the split that
// happens along the given seg at the given location.
//
vertex_t *NewVertexFromSplitSeg(seg_t *seg, double x, double y);

// return a new end vertex to compensate for a seg that would end up
// being zero-length (after integer rounding).  Doesn't compute the
// wall-tip info (thus this routine should only be used _after_ node
// building).
//
vertex_t *NewVertexDegenerate(vertex_t *start, vertex_t *end);

// check whether a line with the given delta coordinates from this
// vertex is open or closed.  If there exists a walltip at same
// angle, it is closed, likewise if line is in void space.
//
bool VertexCheckOpen(vertex_t *vert, double dx, double dy);


//------------------------------------------------------------------------
// SEG : Choose the best Seg to use for a node line.
//------------------------------------------------------------------------


#define IFFY_LEN  4.0


// smallest distance between two points before being considered equal
#define DIST_EPSILON  (1.0 / 128.0)

// smallest degrees between two angles before being considered equal
#define ANG_EPSILON  (1.0 / 1024.0)


// an "intersection" remembers the vertex that touches a BSP divider
// line (especially a new vertex that is created at a seg split).

typedef struct intersection_s
{
	// link in list.  The intersection list is kept sorted by
	// along_dist, in ascending order.
	struct intersection_s *next;
	struct intersection_s *prev;

	// vertex in question
	vertex_t *vertex;

	// how far along the partition line the vertex is.  Zero is at the
	// partition seg's start point, positive values move in the same
	// direction as the partition's direction, and negative values move
	// in the opposite direction.
	double along_dist;

	// true if this intersection was on a self-referencing linedef
	bool self_ref;

	// status of each side of the vertex (along the partition),
	// true if OPEN and false if CLOSED.
	bool open_before;
	bool open_after;
}
intersection_t;


/* -------- functions ---------------------------- */

// scan all the segs in the list, and choose the best seg to use as a
// partition line, returning it.  If no seg can be used, returns NULL.
// The 'depth' parameter is the current depth in the tree, used for
// computing the current progress.
//
seg_t *PickNode(superblock_t *seg_list, int depth, const bbox_t *bbox);

// compute the boundary of the list of segs
void FindLimits(superblock_t *seg_list, bbox_t *bbox);

// compute the seg private info (psx/y, pex/y, pdx/y, etc).
void RecomputeSeg(seg_t *seg);

// take the given seg 'cur', compare it with the partition line, and
// determine it's fate: moving it into either the left or right lists
// (perhaps both, when splitting it in two).  Handles partners as
// well.  Updates the intersection list if the seg lies on or crosses
// the partition line.
//
void DivideOneSeg(seg_t *cur, seg_t *part,
    superblock_t *left_list, superblock_t *right_list,
    intersection_t ** cut_list);

// remove all the segs from the list, partitioning them into the left
// or right lists based on the given partition line.  Adds any
// intersections onto the intersection list as it goes.
//
void SeparateSegs(superblock_t *seg_list, seg_t *part,
    superblock_t *left_list, superblock_t *right_list,
    intersection_t ** cut_list);

// analyse the intersection list, and add any needed minisegs to the
// given seg lists (one miniseg on each side).  All the intersection
// structures will be freed back into a quick-alloc list.
//
void AddMinisegs(seg_t *part,
    superblock_t *left_list, superblock_t *right_list,
    intersection_t *cut_list);

// free the quick allocation cut list
void FreeQuickAllocCuts(void);


//------------------------------------------------------------------------
// NODE : Recursively create nodes and return the pointers.
//------------------------------------------------------------------------


// check the relationship between the given box and the partition
// line.  Returns -1 if box is on left side, +1 if box is on right
// size, or 0 if the line intersects the box.
//
int BoxOnLineSide(superblock_t *box, seg_t *part);

// add the seg to the given list
void AddSegToSuper(superblock_t *block, seg_t *seg);

// increase the counts within the superblock, to account for the given
// seg being split.
//
void SplitSegInSuper(superblock_t *block, seg_t *seg);

// scan all the linedef of the level and convert each sidedef into a
// seg (or seg pair).  Returns the list of segs.
//
superblock_t *CreateSegs(void);

// free a super block.
void FreeSuper(superblock_t *block);

// takes the seg list and determines if it is convex.  When it is, the
// segs are converted to a subsector, and '*S' is the new subsector
// (and '*N' is set to NULL).  Otherwise the seg list is divided into
// two halves, a node is created by calling this routine recursively,
// and '*N' is the new node (and '*S' is set to NULL).  Normally
// returns BUILD_OK, or BUILD_Cancelled if user stopped it.
//
build_result_e BuildNodes(superblock_t *seg_list,
    node_t ** N, subsec_t ** S, int depth, const bbox_t *bbox);

// compute the height of the bsp tree, starting at 'node'.
int ComputeBspHeight(node_t *node);

// put all the segs in each subsector into clockwise order, and renumber
// the seg indices.
//
// [ This cannot be done DURING BuildNodes() since splitting a seg with
//   a partner will insert another seg into that partner's list, usually
//   in the wrong place order-wise. ]
//
void ClockwiseBspTree();

// traverse the BSP tree and do whatever is necessary to convert the
// node information from GL standard to normal standard (for example,
// removing minisegs).
//
void NormaliseBspTree();

// traverse the BSP tree, doing whatever is necessary to round
// vertices to integer coordinates (for example, removing segs whose
// rounded coordinates degenerate to the same point).
//
void RoundOffBspTree();

// free all the superblocks on the quick-alloc list
void FreeQuickAllocSupers(void);

}  // namespace ajbsp


#endif /* __AJBSP_BSP_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
