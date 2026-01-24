using CSV
using Tables
using Makie
using CairoMakie

struct DataVisBSP
  map_name::String
  is_fast::Bool
  split_cost::Int64
  num_segs::Int64
  num_subsecs::Int64
  num_nodes::Int64
  size_left::Int64
  size_right::Int64
end

file_path = ARGS[1]

file = CSV.File(file_path)
rows = Tables.rowtable(file)

bsp_vis = DataVisBSP[]
for row in rows
  push!(bsp_vis,
    DataVisBSP(
      row.map_name,
      row.is_fast,
      row.split_cost,
      row.num_segs,
      row.num_subsecs,
      row.num_nodes,
      row.size_left,
      row.size_right
    )
  )
end

levels = Dict{Tuple{String, Bool}, Vector{DataVisBSP}}()

for row in bsp_vis
  key = (row.map_name, row.is_fast)
  push!(get!(levels, key, DataVisBSP[]), row)
end

for ((map, is_fast), data) in levels
  x = 1:length(data)
  xt = 0:4:length(data)
  mode_label = is_fast ? "FAST" : "NORMAL"

  fig = Figure(size = (960, 960))

  ax = Axis(fig[1, 1], xticks = xt, ylabel = "Nodes")
  ax2 = Axis(fig[2, 1], xticks = xt, ylabel = "Subsectors")
  ax3 = Axis(fig[3, 1], xticks = xt, ylabel = "Segments")
  ax4 = Axis(fig[4, 1], xticks = xt, ylabel = "Binary tree depth", xlabel = "Cost factor of splitting segments")

  linkxaxes!(ax, ax2, ax3, ax4)

  y_nodes = getfield.(data, :num_nodes)
  y_subsecs = getfield.(data, :num_subsecs)
  y_segs = getfield.(data, :num_segs)
  y_size_left = getfield.(data, :size_left)
  y_size_right = getfield.(data, :size_right)

  plot_nodes = lines!(ax, x, y_nodes, color = :dodgerblue)
  scatter!(ax, x, y_nodes, markersize = 6, color = :dodgerblue)

  plot_subsecs = lines!(ax2, x, y_subsecs, color = :seagreen)
  scatter!(ax2, x, y_subsecs, markersize = 6, color = :seagreen)

  plot_segments = lines!(ax3, x, y_segs, color = :firebrick)
  scatter!(ax3, x, y_segs, markersize = 6, color = :firebrick)

  plot_size_left = lines!(ax4, x, y_size_left, color = :purple)
  scatter!(ax4, x, y_size_left, markersize = 6, color = :purple)

  plot_size_right = lines!(ax4, x, y_size_right, color = :darkgoldenrod)
  scatter!(ax4, x, y_size_right, markersize = 6, color = :darkgoldenrod)

  Legend(
    fig[4, 2],
    [plot_nodes, plot_subsecs, plot_segments, plot_size_left, plot_size_right],
    ["Nodes", "Subsectors", "Segments", "Left side tree depth", "Right side tree depth"];
    framevisible = true
  )

  Label(fig[0, :], "ELFBSP Segment Split Cost Analysis - $(map) ($(mode_label))", justification = :center, font = :bold, fontsize = 24)

  trim!(fig.layout)

  save("ELFBSP_SegmentSplitCostAnalysis_$(map)_$(mode_label).png", fig)
end
