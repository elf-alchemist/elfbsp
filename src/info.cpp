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

#include <format>
#include <fstream>

#include "core.hpp"
#include "local.hpp"

static std::vector<std::string> analysis_csv;

//------------------------------------------------------------------------

void SetupAnalysisFile(const char *filepath)
{
  auto csv_path = std::string(filepath);

  auto ext_pos = FindExtension(filepath);
  if (ext_pos > 0)
  {
    csv_path.resize(ext_pos);
  }

  csv_path += ".csv";
  FileClear(csv_path.c_str());

  std::string line = "map,is_fast,split_cost,old_vertex,lines,sides,sectors,new_vertex,nodes,subsecs,segs,splits,left_depth,"
                     "right_depth,average_depth,optimal_depth,tree_balance,worst_case_ratio,tree_quality";
  analysis_csv.push_back(line);
}

// writes out for current file
// expects AnalysisPushLine to have been called with all 0-32 split costs during node-building
void WriteAnalysis(const char *filename)
{
  auto csv_path = std::string(filename);

  if (const auto ext_pos = FindExtension(filename); ext_pos > 0)
  {
    csv_path.resize(ext_pos);
  }

  csv_path += ".csv";

  // Append to fresh CSV
  auto csv_file = std::ofstream(csv_path.c_str(), std::ios::app);

  if (!csv_file.is_open())
  {
    PrintLine(LOG_WARN, "[%s] Couldn't open file %s for writing.", __func__, csv_path.c_str());
    return;
  }

  for (const auto &line : analysis_csv)
  {
    csv_file << line << '\n';
  }

  csv_file.close();

  // Flush out from memory
  analysis_csv.clear();

  PrintLine(LOG_NORMAL, "[%s] Successfully wrote data to CSV file %s.", __func__, csv_path.c_str());
}

static void ComputeTotalBspHeights(const node_t *node, size_t depth, double &leaf_depth_sum)
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
    ComputeTotalBspHeights(node->l.node, depth, leaf_depth_sum);
  }

  if (!node->r.node)
  {
    leaf_depth_sum += static_cast<double>(depth);
  }
  else
  {
    ComputeTotalBspHeights(node->r.node, depth, leaf_depth_sum);
  }
}

void GenerateAnalysis(const char *filename)
{
  auto generate_analysis_data = [](size_t level_index, bool is_fast, size_t split_cost) -> auto
  {
    AnalysisData data;
    node_t *analysis_node = nullptr;
    subsec_t *analysis_sub = nullptr;
    seg_t *analysis_seg = nullptr;

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
    analysis_seg = CreateSegs();
    BuildNodes(analysis_seg, 0, &dummy, &analysis_node, &analysis_sub, static_cast<double>(split_cost), is_fast, true);

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

    ComputeTotalBspHeights(analysis_node, 0, total_depth_sum);
    data.left_depth = ComputeBspHeight(analysis_node->l.node);
    data.right_depth = ComputeBspHeight(analysis_node->r.node);

    left_size = static_cast<double>(data.left_depth);
    right_size = static_cast<double>(data.right_depth);

    data.average_depth = total_depth_sum / num_leafs;

    optimal_depth = ceil(log2(num_leafs));

    data.optimal_depth = static_cast<size_t>(optimal_depth);
    data.tree_balance = (left_size < right_size) ? left_size / right_size : right_size / left_size;

    // math pulled from Marc Rousseau's BSPInfo utility
    expected_leafs_for_optimal_depth = pow(2, static_cast<double>(data.optimal_depth)); // 2 to the power N
    min_epl = num_leafs * (optimal_depth + 1) - expected_leafs_for_optimal_depth;
    max_epl = num_leafs * ((num_leafs - 1) / 2 + 1) - 1;
    data.worst_case_ratio = min_epl / max_epl;
    data.tree_quality = ((min_epl / total_depth_sum) - data.worst_case_ratio) / (1 - data.worst_case_ratio);

    std::string line =
        std::format("{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}", GetLevelName(level_index), is_fast, split_cost,
                    data.vertex, data.lines, data.sides, data.sectors, data.bsp_vertex, data.nodes, data.subsecs, data.segs,
                    data.splits, data.left_depth, data.right_depth, data.average_depth, data.optimal_depth, data.tree_balance,
                    data.worst_case_ratio, data.tree_quality);
    analysis_csv.push_back(line);

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
    // across all split costs
    for (size_t split_cost = 1; split_cost <= 32; split_cost++)
    {
      generate_analysis_data(lev_current_idx, is_fast != 0, split_cost);
      PrintLine(LOG_NORMAL, "[%s] Analyzed %s, %s mode, split cost factor of %zu", __func__, GetLevelName(lev_current_idx), is_fast ? "fast" : "normal", split_cost);
    }
  }
}
