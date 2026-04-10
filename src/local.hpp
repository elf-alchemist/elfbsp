//------------------------------------------------------------------------------
//
//  ELFBSP -- Originally based on the program 'BSP', version 2.3.
//
//------------------------------------------------------------------------------
//
//  Copyright 2025-2026 Guilherme Miranda
//  Copyright 2000-2018 Andrew Apted, et al
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

#pragma once

#include "core.hpp"

#include <vector>

struct Lump_c;
struct Wad_file;

// current WAD file
extern Wad_file *cur_wad;

//------------------------------------------------------------------------
// BLOCKMAP : Generate the blockmap
//------------------------------------------------------------------------

// utility routines...
int CheckLinedefInsideBox(int xmin, int ymin, int xmax, int ymax, int x1, int y1, int x2, int y2);

//------------------------------------------------------------------------
// LEVEL : Level structures & read/write functions.
//------------------------------------------------------------------------

struct node_t;
struct sector_t;
struct quadtree_c;

// a wall-tip is where a wall meets a vertex
struct walltip_t
{
  // link in list.  List is kept in ANTI-clockwise order.
  walltip_t *next;
  walltip_t *prev;

  // angle that line makes at vertex (degrees).
  double angle;

  // whether each side of wall is OPEN or CLOSED.
  // left is the side of increasing angles, whereas
  // right is the side of decreasing angles.
  bool open_left;
  bool open_right;
};

struct vertex_t
{
  // coordinates
  double x, y;

  // vertex index.  Always valid after loading and pruning of unused
  // vertices has occurred.
  size_t index;

  // vertex is newly created (from a seg split)
  bool is_new;

  // when building normal nodes, unused vertices will be pruned.
  bool is_used;

  // usually nullptr, unless this vertex occupies the same location as a
  // previous vertex.
  vertex_t *overlap;

  // list of wall-tips
  walltip_t *tip_set;
};

// check whether a line with the given delta coordinates from this
// vertex is open or closed.  If there exists a walltip at same
// angle, it is closed, likewise if line is in void space.
bool CheckOpen(const vertex_t *vertex, double dx, double dy);

bool Overlaps(const vertex_t *vertex, const vertex_t *other);

struct sector_t
{
  // sector index.  Always valid after loading & pruning.
  size_t index;

  // most info (floor_h, floor_tex, etc) omitted.  We don't need to
  // write the SECTORS lump, only read it.

  // -JL- non-zero if this sector contains a polyobj.
  bool has_polyobj;

  // used when building REJECT table.  Each set of sectors that are
  // isolated from other sectors will have a different group number.
  // Thus: on every 2-sided linedef, the sectors on both sides will be
  // in the same group.  The rej_next, rej_prev fields are a link in a
  // RING, containing all sectors of the same group.
  size_t rej_group;

  double height_floor;
  double height_ceiling;
};

struct sidedef_t
{
  // adjacent sector.  Can be nullptr (invalid sidedef)
  sector_t *sector;

  double offset_x = 0.0;
  double offset_y = 0.0;
  char tex_upper[8] = "-";
  char tex_middle[8] = "-";
  char tex_lower[8] = "-";

  // sidedef index.  Always valid after loading & pruning.
  size_t index;
};

struct linedef_t
{
  // link for list
  linedef_t *Next;

  vertex_t *start; // from this vertex...
  vertex_t *end;   // ... to this vertex

  sidedef_t *right; // right sidedef
  sidedef_t *left;  // left sidedef, or nullptr if none

  int32_t special; //
  uint16_t flags;  // currently we only care about two-sided lines, but who knows
  int32_t args[5]; // Tag => arg0/id split
  int32_t id;      //

  uint32_t effects = FX_Nothing;
  seg_rotation_t angle = FX_DoNotRotate;

  // normally nullptr, except when this linedef directly overlaps an earlier
  // one (a rarely-used trick to create higher mid-masked textures).
  // No segs should be created for these overlapping linedefs.
  linedef_t *overlap;

  // linedef index.  Always valid after loading & pruning of zero
  // length lines has occurred.
  size_t index;

  inline double MinX(void) const
  {
    return std::min(start->x, end->x);
  }
};

struct thing_t
{
  double x, y;
  doomednum_t type;

  // other info (angle, and hexen stuff) omitted.  We don't need to
  // write the THINGS lump, only read it.

  // Always valid (thing indices never change).
  size_t index;
};

struct seg_t
{
  // link for list
  seg_t *next;

  vertex_t *start; // from this vertex...
  vertex_t *end;   // ... to this vertex

  // linedef that this seg goes along, or nullptr if miniseg
  linedef_t *linedef;

  // 0 for right, 1 for left
  bool side = false;

  // seg on other side, or nullptr if one-sided.  This relationship is
  // always one-to-one -- if one of the segs is split, the partner seg
  // must also be split.
  seg_t *partner;

  // seg index.  Only valid once the seg has been added to a
  // subsector.  A negative value means it is invalid -- there
  // shouldn't be any of these once the BSP tree has been built.
  size_t index = NO_INDEX;

  // when true, this seg has become zero length (integer rounding of the
  // start and end vertices produces the same location).
  bool is_degenerate;

  // the quad-tree node that contains this seg, or nullptr if the seg
  // is now in a subsector.
  quadtree_c *quad;

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

  // this only used by ClockwiseOrder()
  double cmp_angle;

  // compute the parallel and perpendicular distances from a partition
  // line to a point.
  inline double ParallelDist(double x, double y) const
  {
    return (x * pdx + y * pdy + p_para) / p_length;
  }

  inline double PerpDist(double x, double y) const
  {
    return (x * pdy - y * pdx + p_perp) / p_length;
  }
};

// compute the seg private info (psx/y, pex/y, pdx/y, etc).
void Recompute(seg_t *seg);

int PointOnLineSide(seg_t *seg, double x, double y);

// a seg with this index is removed by SortSegs().
// it must be a very high value.
static constexpr uint32_t SEG_IS_GARBAGE = (1 << 29);

struct subsec_t
{
  // list of segs
  seg_t *seg_list;

  // count of segs -- only valid after RenumberSegs() is called
  size_t seg_count;

  // subsector index.  Always valid, set when the subsector is
  // initially created.
  size_t index;

  // approximate middle point
  double mid_x;
  double mid_y;
};

void AddToTail(subsec_t *subsec);

void DetermineMiddle(subsec_t *subsec);
void ClockwiseOrder(subsec_t *subsec);
void RenumberSegs(subsec_t *subsec, size_t &cur_seg_index);

void RoundOffSubsector(level_t &level, subsec_t *subsec);
void Normalise(subsec_t *subsec);

void SanityCheckClosed(subsec_t *subsec);
void SanityCheckHasRealSeg(subsec_t *subsec);

struct bbox_t
{
  int16_t minx, miny;
  int16_t maxx, maxy;
};

struct child_t
{
  // child node or subsector (one must be nullptr)
  node_t *node;
  subsec_t *subsec;

  // child bounding box
  bbox_t bounds;
};

struct node_t
{
  // these coordinates are high precision to support UDMF.
  // in non-UDMF maps, they will actually be integral since a
  // partition line *always* comes from a normal linedef.

  double x, y;   // starting point
  double dx, dy; // offset to ending point

  // right & left children
  child_t r;
  child_t l;

  // node index.  Only valid once the NODES lump has been created.
  size_t index;
};

void SetPartition(node_t *node, const seg_t *part);

struct quadtree_c
{
  // NOTE: not a real quadtree, division is always binary.

  // coordinates on map for this block, from lower-left corner to
  // upper-right corner.  Fully inclusive, i.e (x,y) is inside this
  // block when x1 < x < x2 and y1 < y < y2.
  int x1, y1;
  int x2, y2;

  // sub-trees.  nullptr for leaf nodes.
  // [0] has the lower coordinates, and [1] has the higher coordinates.
  // Division of a square always occurs horizontally (e.g. 512x512 ->
  // 256x512).
  quadtree_c *subs[2];

  // count of real/minisegs contained in this node AND ALL CHILDREN.
  size_t real_num;
  size_t mini_num;

  // list of segs completely contained in this node.
  seg_t *list;

  quadtree_c(int _x1, int _y1, int _x2, int _y2);
  ~quadtree_c(void);

  inline bool Empty(void) const
  {
    return (real_num + mini_num) == 0;
  }
};

void AddSeg(quadtree_c *quadtree, seg_t *seg);
void AddList(quadtree_c *quadtree, seg_t *list);

void ConvertToList(quadtree_c *quadtree, seg_t **__list);

// check relationship between this box and the partition line.
// returns -1 or +1 if box is definitively on a particular side,
// or 0 if the line intersects or touches the box.
int OnLineSide(quadtree_c *quadtree, const seg_t *part);

// an "intersection" remembers the vertex that touches a BSP divider
// line (especially a new vertex that is created at a seg split).

struct intersection_t
{
  // link in list.  The intersection list is kept sorted by
  // along_dist, in ascending order.
  intersection_t *next;
  intersection_t *prev;

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
};

using blocklist_t = struct blocklist_t
{
  size_t hash;
  std::vector<size_t> lines;
};

// Note: ZDoom format support based on code (C) 2002,2003 Marisa "Randi" Heit

using level_t = struct level_t
{
  map_format_t format = MapFormat_INVALID;
  size_t num_old_vert = 0;
  size_t num_new_vert = 0;
  size_t num_real_lines = 0;
  size_t level_num = NO_INDEX;
  size_t level_header_lump_index = NO_INDEX;
  bool overflows = false;

  std::vector<vertex_t *> vertices;
  std::vector<linedef_t *> linedefs;
  std::vector<sidedef_t *> sidedefs;
  std::vector<sector_t *> sectors;
  std::vector<thing_t *> things;

  std::vector<seg_t *> segs;
  std::vector<subsec_t *> subsecs;
  std::vector<node_t *> nodes;
  std::vector<walltip_t *> walltips;
  std::vector<intersection_t *> intercuts;

  uint8_t *reject_matrix;
  size_t reject_size;
  std::vector<size_t> reject_groups;

  int16_t block_x, block_y;
  size_t block_w, block_h;
  size_t block_count;

  std::vector<blocklist_t> block_lines;
  std::vector<size_t> block_ptrs;
  std::vector<size_t> block_dups;

  int32_t block_compression;
  bool block_overflowed = false;

  inline Lump_c *FindLevelLump(const char *name)
  {
    SYS_ASSERT(cur_wad != nullptr);
    size_t idx = cur_wad->LevelLookupLump(level_num, name);
    return (idx != NO_INDEX) ? cur_wad->GetLump(idx) : nullptr;
  }

  inline const char *GetLevelName(void)
  {
    SYS_ASSERT(cur_wad != nullptr);
    size_t lump_idx = cur_wad->LevelHeader(level_num);
    return cur_wad->GetLump(lump_idx)->Name();
  }

  vertex_t *SafeLookupVertex(size_t num, size_t num_line)
  {
    if (num >= vertices.size())
    {
      PrintLine(LOG_ERROR, "ERROR: Illegal map-vertex number #%zu, on line #%zu, maximum is #%zu", num, num_line,
                vertices.size());
    }
    return vertices[num];
  }

  sector_t *SafeLookupSector(size_t num, size_t num_side)
  {
    if (num >= NO_INDEX_INT16)
    {
      return nullptr;
    }

    if (num >= sectors.size())
    {
      PrintLine(LOG_ERROR, "ERROR: Illegal sector number #%zu, on side #%zu, maximum is #%zu", num, num_side,
                sectors.size() - 1);
    }

    return sectors[num];
  }

  inline sidedef_t *SafeLookupSidedef(uint16_t num)
  {
    // silently ignore illegal sidedef numbers
    if (num >= NO_INDEX_INT16 || num >= sidedefs.size())
    {
      return nullptr;
    }

    return sidedefs[num];
  }
};

vertex_t *NewVertex(level_t &level);
linedef_t *NewLinedef(level_t &level);
sidedef_t *NewSidedef(level_t &level);
sector_t *NewSector(level_t &level);
thing_t *NewThing(level_t &level);

seg_t *NewSeg(level_t &level);
subsec_t *NewSubsec(level_t &level);
node_t *NewNode(level_t &level);
walltip_t *NewWallTip(level_t &level);
intersection_t *NewIntersection(level_t &level);

void FreeVertices(level_t &level);
void FreeSidedefs(level_t &level);
void FreeLinedefs(level_t &level);
void FreeSectors(level_t &level);
void FreeThings(level_t &level);

void FreeSegs(level_t &level);
void FreeSubsecs(level_t &level);
void FreeNodes(level_t &level);
void FreeWallTips(level_t &level);
void FreeIntersections(level_t &level);

Lump_c *CreateLevelLump(level_t &level, const char *name, size_t max_size = NO_INDEX);

//------------------------------------------------------------------------
// ANALYZE : Analyzing level structures
//------------------------------------------------------------------------

// detection routines
void DetectOverlappingVertices(level_t &level);
void DetectOverlappingLines(level_t &level);
void DetectPolyobjSectors(buildinfo_t &config, level_t &level);

// pruning routines
void ClearNewVertices(level_t &level);
void PruneVerticesAtEnd(level_t &level);

// computes the wall tips for all of the vertices
void CalculateWallTips(level_t &level);

// return a new vertex (with correct wall-tip info) for the split that
// happens along the given seg at the given location.
vertex_t *NewVertexFromSplitSeg(level_t &level, seg_t *seg, double x, double y);

// return a new end vertex to compensate for a seg that would end up
// being zero-length (after integer rounding).  Doesn't compute the wall-tip
// info (thus this routine should only be used *after* node building).
vertex_t *NewVertexDegenerate(level_t &level, vertex_t *start, vertex_t *end);

//------------------------------------------------------------------------
// SEG : Choose the best Seg to use for a node line.
//------------------------------------------------------------------------

static constexpr double IFFY_LEN = 4.0;

// smallest distance between two points before being considered equal
static constexpr double DIST_EPSILON = (1.0 / 1024.0);

// smallest distance between two points before being considered equal
static constexpr double DIST_EPSILON_HI = (1.0 / FRACFACTOR);

// smallest degrees between two angles before being considered equal
static constexpr double ANG_EPSILON = (1.0 / 1024.0);

//------------------------------------------------------------------------
// NODE : Recursively create nodes and return the pointers.
//------------------------------------------------------------------------

// scan all the linedef of the level and convert each sidedef into a
// seg (or seg pair).  Returns the list of segs.
seg_t *CreateSegs(level_t &level);

quadtree_c *TreeFromSegList(seg_t *list);

// takes the seg list and determines if it is convex.  When it is, the
// segs are converted to a subsector, and '*S' is the new subsector
// (and '*N' is set to nullptr). Otherwise, the seg list is divided into
// two halves, a node is created by calling this routine recursively,
// and '*N' is the new node (and '*S' is set to nullptr).  Normally
// returns BUILD_OK.

void BuildNodes(level_t &level, seg_t *seg_list, int depth, bbox_t *bounds, node_t **N, subsec_t **S, double split_cost,
                bool fast, bool analysis);

// compute the height of the bsp tree, starting at 'node'.
size_t ComputeBspHeight(const node_t *node);

// put all the segs in each subsector into clockwise order, and renumber
// the seg indices.
//
// [ This cannot be done DURING BuildNodes() since splitting a seg with
//   a partner will insert another seg into that partner's list, usually
//   in the wrong place order-wise. ]
void ClockwiseBspTree(level_t &level);

// traverse the BSP tree and do whatever is necessary to convert the
// node information from GL standard to normal standard (for example,
// removing minisegs).
void NormaliseBspTree(level_t &level);

// traverse the BSP tree, doing whatever is necessary to round
// vertices to integer coordinates (for example, removing segs whose
// rounded coordinates degenerate to the same point).
void RoundOffBspTree(level_t &level);

void InitBlockmap(level_t &level);
void PutBlockmap(level_t &level);
void PutReject(level_t &level);

void SaveDoom_Vanilla(level_t &level, node_t *root_node);
void SaveDoom_DeePBSPV4(level_t &level, node_t *root_node);
void SaveDoom_XNOD(level_t &level, node_t *root_node);
void SaveDoom_XGLN(level_t &level, node_t *root_node);
void SaveDoom_XGL2(level_t &level, node_t *root_node);
void SaveDoom_XGL3(level_t &level, node_t *root_node);

void SaveTextmap_ZNODES(level_t &level, node_t *root_node);
