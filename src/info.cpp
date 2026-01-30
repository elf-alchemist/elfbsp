//------------------------------------------------------------------------------
//
//  ELFBSP
//
//------------------------------------------------------------------------------
//
//  Copyright 2026 Guilherme Miranda
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

#include "core.hpp"
#include "local.hpp"

static void WriteAnalysis(const char *filename, bool is_fast, std::vector<AnalysisData> &list)
{
  auto png_path = std::string(filename);
  if (auto ext_pos = FindExtension(filename); ext_pos > 0)
  {
    png_path.resize(ext_pos);
  }
  png_path += "_";
  png_path += GetLevelName(lev_current_idx);
  png_path += "_";
  png_path += is_fast ? "fast" : "normal";
  png_path += ".png";
}

static void ComputeBspDepth(const node_t *node, size_t depth, double &leaf_depth_sum)
{
  if (!node)
  {
    return;
  }

  depth++;

  if (!node->l.node)
  {
    leaf_depth_sum += static_cast<double>(depth);
  }
  else
  {
    ComputeBspDepth(node->l.node, depth, leaf_depth_sum);
  }

  if (!node->r.node)
  {
    leaf_depth_sum += static_cast<double>(depth);
  }
  else
  {
    ComputeBspDepth(node->r.node, depth, leaf_depth_sum);
  }
}

void AnalysisBSP(const char *filename)
{
  auto generate_analysis_data = [](AnalysisData &data, bool is_fast, size_t split_cost) -> auto
  {
    node_t *analysis_node = nullptr;
    subsec_t *analysis_sub = nullptr;

    // Using 'double's to get around strict type casting
    // and implicit conversion warnings
    double left_size = 0.0;
    double right_size = 0.0;
    double total_depth_sum = 0.0;

    double num_leafs = 0.0;
    double optimal_depth = 0.0;
    double expected_leafs_for_optimal_depth = 0.0;

    // external path length
    double min_epl = 0.0;
    double max_epl = 0.0;

    bbox_t dummy = {0, 0, 0, 0};
    BuildNodes(CreateSegs(), 0, &dummy, &analysis_node, &analysis_sub, static_cast<double>(split_cost), is_fast, true);

    // TODO: pass level data by context instead of globally :v
    data.vertex = num_old_vert;
    data.lines = lev_linedefs.size();
    data.sides = lev_sidedefs.size();
    data.sectors = lev_sectors.size();

    data.bsp_vertex = num_new_vert;
    data.nodes = lev_nodes.size();
    data.subsecs = lev_subsecs.size();
    data.segs = lev_segs.size();

    // TODO: sidedef math should account for non-seg-generating sides, actually
    data.splits = lev_segs.size() - lev_sidedefs.size();
    num_leafs = static_cast<double>(data.subsecs);

    ComputeBspDepth(analysis_node, 0, total_depth_sum);
    data.left_depth = ComputeTreeDepth(analysis_node->l.node);
    data.right_depth = ComputeTreeDepth(analysis_node->r.node);

    left_size = static_cast<double>(data.left_depth);
    right_size = static_cast<double>(data.right_depth);

    data.average_depth = total_depth_sum / num_leafs;

    optimal_depth = ceil(log2(num_leafs));

    data.optimal_depth = static_cast<size_t>(optimal_depth);
    data.tree_balance = (left_size < right_size) ? left_size / right_size : right_size / left_size;

    // math pulled from ZenNode/ZokumBSP's BSPInfo utility
    expected_leafs_for_optimal_depth = (1u << data.optimal_depth); // equals to "pow(2, optimal_depth);"
    min_epl = num_leafs * (optimal_depth + 1) - expected_leafs_for_optimal_depth;
    max_epl = num_leafs * ((num_leafs - 1) / 2 + 1) - 1;
    data.worst_case_ratio = min_epl / max_epl;
    data.tree_quality = ((min_epl / total_depth_sum) - data.worst_case_ratio) / (1 - data.worst_case_ratio);

    // TODO: pass level data by context instead of globally :v
    FreeNodes();
    FreeSubsecs();
    FreeSegs();
    ClearNewVertices();
    num_new_vert = 0;
  };

  // normal mode, and fast mode
  for (size_t is_fast = 0; is_fast <= 1; is_fast++)
  {
    std::vector<AnalysisData> list;
    list.reserve(32);

    // across all split costs
    for (size_t split_cost = 1; split_cost <= 32; split_cost++)
    {
      generate_analysis_data(list[split_cost - 1], is_fast != 0, split_cost);
    }

    WriteAnalysis(filename, is_fast, list);
  }
}
