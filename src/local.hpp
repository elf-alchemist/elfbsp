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
#include "wad.hpp"

#include <vector>

//------------------------------------------------------------------------
// BLOCKMAP : Generate the blockmap
//------------------------------------------------------------------------

// utility routines...
bool CheckLinedefInsideBox(int xmin, int ymin, int xmax, int ymax, int x1, int y1, int x2, int y2);

//------------------------------------------------------------------------
// LEVEL : Level structures & read/write functions.
//------------------------------------------------------------------------

struct Node;
struct Quadtree;

// a wall-tip is where a wall meets a vertex
struct WallTip
{
  // link in list.  List is kept in ANTI-clockwise order.
  WallTip *next;
  WallTip *prev;

  // angle that line makes at vertex (degrees).
  double angle;

  // whether each side of wall is OPEN or CLOSED.
  // left is the side of increasing angles, whereas
  // right is the side of decreasing angles.
  bool open_left;
  bool open_right;
};

struct Vertex
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
  Vertex *overlap;

  // list of wall-tips
  WallTip *tip_set;
};

// check whether a line with the given delta coordinates from this
// vertex is open or closed.  If there exists a walltip at same
// angle, it is closed, likewise if line is in void space.
bool CheckOpen(const Vertex *vertex, double dx, double dy);

bool Overlaps(const Vertex *vertex, const Vertex *other);

struct Sector
{
  // sector index.  Always valid after loading & pruning.
  size_t index = NO_INDEX;

  // most info (floor_h, floor_tex, etc) omitted.  We don't need to
  // write the SECTORS lump, only read it.

  SectorEffects effects = FX_Sector_None;

  // -JL- non-zero if this sector contains a polyobj.
  bool has_polyobj = false;

  // used when building REJECT table.  Each set of sectors that are
  // isolated from other sectors will have a different group number.
  // Thus: on every 2-sided linedef, the sectors on both sides will be
  // in the same group.  The rej_next, rej_prev fields are a link in a
  // RING, containing all sectors of the same group.
  size_t rej_group = NO_INDEX;

  Sector *rej_next = nullptr;
  Sector *rej_prev = nullptr;
};

struct Sidedef
{
  // adjacent sector.  Can be nullptr (invalid sidedef)
  Sector *sector;

  double offset_x = 0.0;
  double offset_y = 0.0;
  char tex_upper[8] = "-";
  char tex_middle[8] = "-";
  char tex_lower[8] = "-";

  // sidedef index.  Always valid after loading & pruning.
  size_t index;
};

struct Linedef
{
  // link for list
  Linedef *Next;

  Vertex *start; // from this vertex...
  Vertex *end;   // ... to this vertex

  Sidedef *right; // right sidedef
  Sidedef *left;  // left sidedef, or nullptr if none

  int32_t special; //
  int32_t id;      //

  uint32_t effects = FX_Nothing;
  SegmentRotation angle = FX_DoNotRotate;

  // normally nullptr, except when this linedef directly overlaps an earlier
  // one (a rarely-used trick to create higher mid-masked textures).
  // No segs should be created for these overlapping linedefs.
  Linedef *overlap;

  // linedef index.  Always valid after loading & pruning of zero
  // length lines has occurred.
  size_t index;

  inline double MinX(void) const
  {
    return std::min(start->x, end->x);
  }
};

struct Mobj
{
  double x, y;
  DoomEdNum type;

  // other info (angle, and hexen stuff) omitted.  We don't need to
  // write the THINGS lump, only read it.

  // Always valid (thing indices never change).
  size_t index;
};

struct Segment
{
  // link for list
  Segment *next;

  Vertex *start; // from this vertex...
  Vertex *end;   // ... to this vertex

  // linedef that this seg goes along, or nullptr if miniseg
  Linedef *linedef;

  // 0 for right, 1 for left
  bool side = false;

  // seg on other side, or nullptr if one-sided.  This relationship is
  // always one-to-one -- if one of the segs is split, the partner seg
  // must also be split.
  Segment *partner;

  // seg index.  Only valid once the seg has been added to a
  // subsector.  A negative value means it is invalid -- there
  // shouldn't be any of these once the BSP tree has been built.
  size_t index = NO_INDEX;

  // when true, this seg has become zero length (integer rounding of the
  // start and end vertices produces the same location).
  bool is_degenerate;

  // the quad-tree node that contains this seg, or nullptr if the seg
  // is now in a subsector.
  Quadtree *quad;

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
  Linedef *source_line;

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

// a seg with this index is removed by SortSegs().
// it must be a very high value.
static constexpr uint32_t SEG_IS_GARBAGE = (1 << 29);

struct Subsector
{
  // list of segs
  Segment *seg_list;

  // count of segs -- only valid after RenumberSegs() is called
  size_t seg_count;

  // subsector index.  Always valid, set when the subsector is
  // initially created.
  size_t index;

  // approximate middle point
  double mid_x;
  double mid_y;
};

struct NodeChild
{
  // child node or subsector (one must be nullptr)
  Node *node;
  Subsector *subsec;

  // child bounding box
  MapBoundBox bounds;
};

struct Node
{
  // these coordinates are high precision to support UDMF.
  // in non-UDMF maps, they will actually be integral since a
  // partition line *always* comes from a normal linedef.

  double x, y;   // starting point
  double dx, dy; // offset to ending point

  // right & left children
  NodeChild r;
  NodeChild l;

  // node index.  Only valid once the NODES lump has been created.
  size_t index;
};

struct Quadtree
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
  Quadtree *subs[2];

  // count of real/minisegs contained in this node AND ALL CHILDREN.
  size_t real_num;
  size_t mini_num;

  // list of segs completely contained in this node.
  Segment *list;

  Quadtree(int _x1, int _y1, int _x2, int _y2);
  ~Quadtree(void);

  inline bool Empty(void) const
  {
    return (real_num + mini_num) == 0;
  }
};

// check relationship between this box and the partition line.
// returns -1 or +1 if box is definitively on a particular side,
// or 0 if the line intersects or touches the box.
int OnLineSide(Quadtree *quadtree, const Segment *part);

// an "intersection" remembers the vertex that touches a BSP divider
// line (especially a new vertex that is created at a seg split).

struct Intersection
{
  // link in list.  The intersection list is kept sorted by
  // along_dist, in ascending order.
  Intersection *next;
  Intersection *prev;

  // vertex in question
  Vertex *vertex;

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

struct BlockList
{
  size_t blk_num = NO_INDEX;   // self-index, always valid after being created, even if sorted after
  size_t dup_index = NO_INDEX; // if not NO_INDEX, then is pointing to another BlockList as canon
  FNV1a<size_t> sum;           // to check against other blocks for duplicates detection
  std::vector<size_t> lines;   // actual line index list
};

struct BlockMap
{
  BlockmapFormat format = BlockmapFormat::DoomBSP;

  // North-west corner point
  int16_t origin_x = 0;
  int16_t origin_y = 0;

  // Negative offset from origin, for vanilla optimization data analysis
  // NOT IMPLEMENTED YET
  int16_t offset_x = 0;
  int16_t offset_y = 0;
  int16_t step_x = 0;
  int16_t step_y = 0;

  // Weighted AABB cardinality, for vanilla optimization data analysis
  // NOT IMPLEMENTED YET
  enum class CardinalWeight {
    NorthEast,
    NorthWest,
    SouthWest,
    SouthEast,

    EastEither,
    NorthEither,
    SouthEither,
    WestEither,

    Any
  } cardinal_weight;

  // Actual blockmap size, from origin
  size_t rows = 0;
  size_t columns = 0;

  // Uncompressed data
  std::vector<size_t> unpacked_offsets;
  std::vector<size_t> unpacked_blocks;

  // Data after compression
  std::vector<size_t> offsets;
  std::vector<size_t> blocks;

  // Can be safely sorted, as each block list contains its own original block index
  std::vector<BlockList> cells;

  // Logged metric
  double block_compression = 0.0;

  [[nodiscard]]
  size_t BlockCount(void) const
  {
    return this->columns * this->rows;
  }
};

struct MapLumps
{
  size_t label = NO_INDEX;

  size_t textmap = NO_INDEX;
  size_t things = NO_INDEX;
  size_t linedefs = NO_INDEX;
  size_t sidedefs = NO_INDEX;
  size_t vertexes = NO_INDEX;
  size_t sectors = NO_INDEX;
  size_t lights = NO_INDEX;

  size_t znodes = NO_INDEX;
  size_t nodes = NO_INDEX;
  size_t ssectors = NO_INDEX;
  size_t segs = NO_INDEX;
  size_t leafs = NO_INDEX;
  size_t reject = NO_INDEX;
  size_t blockmap = NO_INDEX;
  size_t behavior = NO_INDEX;
  size_t macros = NO_INDEX;
  size_t scripts = NO_INDEX;
};

struct Level
{
  MapFormat map_format = MapFormat::Invalid;
  size_t num_old_vert = 0;
  size_t num_new_vert = 0;
  size_t num_real_lines = 0;
  bool overflows = false;

  std::vector<Vertex *> vertices;
  std::vector<Linedef *> linedefs;
  std::vector<Sidedef *> sidedefs;
  std::vector<Sector *> sectors;
  std::vector<Mobj *> things;

  Node *root_node = nullptr;
  Subsector *root_sub = nullptr;

  std::vector<Segment *> segs;
  std::vector<Subsector *> subsecs;
  std::vector<Node *> nodes;
  std::vector<WallTip *> walltips;
  std::vector<Intersection *> intercuts;
  BspFormat bsp_format = BspFormat::XNOD;

  uint8_t *reject_matrix;
  size_t reject_size;

  std::vector<UVertexMinimal> udmf_vertex;
  std::vector<ULinedefMinimal> udmf_linedef;
  std::vector<USidedefMinimal> udmf_sidedef;
  std::vector<USectorMinimal> udmf_sector;
  std::vector<UThingMinimal> udmf_thing;

  MapLumps lumps;

  bool skip_bsp = false;  // Just copy over the lumps, don't build anything ...
  bool skip_bmap = false; // ... if, and only if, the lump already exists ...
  bool skip_rej = false;  // ... for if the lump does not exist, it will be built.

  void SetLumps(WadIO &io, size_t label_index);

  Vertex *SafeLookupVertex(size_t num, size_t num_line) const;
  Sector *SafeLookupSector(size_t num, size_t num_side) const;
  Sidedef *SafeLookupSidedef(int32_t num) const;

  Vertex *NewVertex(void);
  Linedef *NewLinedef(void);
  Sidedef *NewSidedef(void);
  Sector *NewSector(void);
  Mobj *NewThing(void);

  Segment *NewSeg(void);
  Subsector *NewSubsec(void);
  Node *NewNode(void);
  WallTip *NewWallTip(void);
  Intersection *NewIntersection(void);

  void FreeVertices(void);
  void FreeSidedefs(void);
  void FreeLinedefs(void);
  void FreeSectors(void);
  void FreeThings(void);

  void FreeSegs(void);
  void FreeSubsecs(void);
  void FreeNodes(void);
  void FreeWallTips(void);
  void FreeIntersections(void);
};

void ParseUDMF(WadIO &io, Level &level);

//------------------------------------------------------------------------
// ANALYZE : Analyzing level structures
//------------------------------------------------------------------------

//
// TODO: some of these are defined in misc, but only used elsewhere. static?
//

// detection routines
void DetectOverlappingVertices(Level &level);
void DetectOverlappingLines(Level &level);
void DetectPolyobjSectors(BuildInfo &config, Level &level);

// pruning routines
void ClearNewVertices(Level &level);
void PruneVerticesAtEnd(Level &level);

// computes the wall tips for all of the vertices
void CalculateWallTips(Level &level);

// return a new vertex (with correct wall-tip info) for the split that
// happens along the given seg at the given location.
Vertex *NewVertexFromSplitSeg(Level &level, Segment *seg, double x, double y);

// return a new end vertex to compensate for a seg that would end up
// being zero-length (after integer rounding).  Doesn't compute the wall-tip
// info (thus this routine should only be used *after* node building).
Vertex *NewVertexDegenerate(Level &level, Vertex *start, Vertex *end);

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
Segment *CreateSegs(Level &level);

Quadtree *TreeFromSegList(Segment *list);

// takes the seg list and determines if it is convex.  When it is, the
// segs are converted to a subsector, and '*S' is the new subsector
// (and '*N' is set to nullptr). Otherwise, the seg list is divided into
// two halves, a node is created by calling this routine recursively,
// and '*N' is the new node (and '*S' is set to nullptr).  Normally
// returns BUILD_OK.

void BuildNodes(Level &level, Segment *seg_list, int depth, MapBoundBox &bounds, Node **N, Subsector **S, double split_cost,
                bool fast, bool analysis);

// compute the height of the bsp tree, starting at 'node'.
size_t ComputeBspHeight(const Node *node);

enum class BuildResult
{
  // everything went peachy keen
  Ok = 0,

  // when saving the map, one or more lumps overflowed
  Overflow
};

// build the nodes of a particular level. otherwise the wad
// is updated to store the new lumps and returns either BUILD_OK or
// BUILD_LumpOverflow if some limits were exceeded.
BuildResult BuildLevel(WadIO &io, Level &level, std::string_view filename);
