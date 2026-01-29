using CSV
using Tables
using Makie
using CairoMakie
using Base.Filesystem

println("[Init] Starting up")

struct DataVisBSP
  map_name::String
  is_fast::Bool
  split_cost::Int64
  old_vertex::Int64
  lines::Int64
  sides::Int64
  sectors::Int64
  new_vertex::Int64
  nodes::Int64
  subsecs::Int64
  segs::Int64
  splits::Int64
  left_depth::Int64
  right_depth::Int64
  average_depth::Float64
  optimal_depth::Float64
  tree_balance::Float64
  worst_case_ratio::Float64
  tree_quality::Float64
end

file_path = ARGS[1]

out_dir = dirname(file_path)
stem = splitext(basename(file_path))[1]

println("[CSV] Reading table")
csv_file = CSV.File(file_path)
rows = Tables.rowtable(csv_file)

println("[CSV] Loading BSP stats")
bsp_vis = DataVisBSP[]
for row in rows
  push!(bsp_vis,
    DataVisBSP(
      row.map_name,
      row.is_fast,
      row.split_cost,
      row.old_vertex,
      row.lines,
      row.sides,
      row.sectors,
      row.new_vertex,
      row.nodes,
      row.subsecs,
      row.segs,
      row.splits,
      row.left_depth,
      row.right_depth,
      row.average_depth,
      row.optimal_depth,
      row.tree_balance,
      row.worst_case_ratio,
      row.tree_quality
    )
  )
end

levels = Dict{Tuple{String,Bool},Vector{DataVisBSP}}()

println("[CSV] Setting up internal data format")
for row in bsp_vis
  key = (row.map_name, row.is_fast)
  push!(get!(levels, key, DataVisBSP[]), row)
end

maps = Dict{String,Dict{Bool,Vector{DataVisBSP}}}()

for ((map, is_fast), data) in levels
  by_mode = get!(maps, map, Dict{Bool,Vector{DataVisBSP}}())
  by_mode[is_fast] = data
end

ytick_int = xs -> string.(round.(Int, xs))

for (map, modes) in maps
  println("[Figure] Setting up $(map) of $(stem)")

  fig = Figure(; size=(2100, 900))
  xt = 0:4:64

  ax_nodes = Axis(fig[1, 1], xticks=xt, ytickformat=ytick_int, ylabel="Nodes")
  ax_subsecs = Axis(fig[2, 1], xticks=xt, ytickformat=ytick_int, ylabel="Subsectors", xlabel="Cost factor of splitting segments")

  ax_segs = Axis(fig[1, 2], xticks=xt, ytickformat=ytick_int, ylabel="Segments")
  ax_splits = Axis(fig[2, 2], xticks=xt, ytickformat=ytick_int, ylabel="Splits", xlabel="Cost factor of splitting segments")

  ax_left_depth = Axis(fig[1, 3], xticks=xt, ytickformat=ytick_int, ylabel="Tree depth left")
  ax_right_depth = Axis(fig[2, 3], xticks=xt, ytickformat=ytick_int, ylabel="Tree depth right", xlabel="Cost factor of splitting segments")

  ax_average_depth = Axis(fig[1, 4], xticks=xt, ytickformat=ytick_int, ylabel="Average tree depth until leaf")
  ax_optimal_depth = Axis(fig[2, 4], xticks=xt, ytickformat=ytick_int, ylabel="Optimal EPL tree depth", xlabel="Cost factor of splitting segments")

  ax_tree_balance = Axis(fig[1, 5], xticks=xt, ylabel="Tree balance")
  ax_tree_quality = Axis(fig[2, 5], xticks=xt, ylabel="Tree quality", xlabel="Cost factor of splitting segments")

  linkxaxes!(ax_nodes, ax_subsecs)
  linkxaxes!(ax_segs, ax_splits)
  linkxaxes!(ax_left_depth, ax_right_depth)
  linkxaxes!(ax_average_depth, ax_optimal_depth)
  linkxaxes!(ax_tree_balance, ax_tree_quality)

  # Start ploting
  data_normal = modes[false]
  data_fast = modes[true]
  x = 1:length(data_normal)

  # normal
  y_nodes = getfield.(data_normal, :nodes)
  y_subsecs = getfield.(data_normal, :subsecs)

  y_segs = getfield.(data_normal, :segs)
  y_splits = getfield.(data_normal, :splits)

  y_left_depth = getfield.(data_normal, :left_depth)
  y_right_depth = getfield.(data_normal, :right_depth)

  y_average_depth = getfield.(data_normal, :average_depth)
  y_optimal_depth = getfield.(data_normal, :optimal_depth)

  y_tree_balance = getfield.(data_normal, :tree_balance)
  y_tree_quality = getfield.(data_normal, :tree_quality)

  # fast
  y_nodes_fast = getfield.(data_fast, :nodes)
  y_subsecs_fast = getfield.(data_fast, :subsecs)

  y_segs_fast = getfield.(data_fast, :segs)
  y_splits_fast = getfield.(data_fast, :splits)

  y_left_depth_fast = getfield.(data_fast, :left_depth)
  y_right_depth_fast = getfield.(data_fast, :right_depth)

  y_average_depth_fast = getfield.(data_fast, :average_depth)
  y_optimal_depth_fast = getfield.(data_fast, :optimal_depth)

  y_tree_balance_fast = getfield.(data_fast, :tree_balance)
  y_tree_quality_fast = getfield.(data_fast, :tree_quality)

  lines!(ax_nodes, x, y_nodes; color=:dodgerblue, linestyle=:solid, alpha=0.50)
  scatter!(ax_nodes, x, y_nodes; color=:dodgerblue, marker=:circle)
  lines!(ax_nodes, x, y_nodes_fast; color=:dodgerblue, linestyle=:dash, alpha=0.50)
  scatter!(ax_nodes, x, y_nodes_fast; color=:dodgerblue, marker=:utriangle)

  lines!(ax_subsecs, x, y_subsecs; color=:seagreen, linestyle=:solid, alpha=0.50)
  scatter!(ax_subsecs, x, y_subsecs; color=:seagreen, marker=:circle)
  lines!(ax_subsecs, x, y_subsecs_fast; color=:seagreen, linestyle=:dash, alpha=0.50)
  scatter!(ax_subsecs, x, y_subsecs_fast; color=:seagreen, marker=:utriangle)

  lines!(ax_segs, x, y_segs; color=:firebrick, linestyle=:solid, alpha=0.50)
  scatter!(ax_segs, x, y_segs; color=:firebrick, marker=:circle)
  lines!(ax_segs, x, y_segs_fast; color=:firebrick, linestyle=:dash, alpha=0.50)
  scatter!(ax_segs, x, y_segs_fast; color=:firebrick, marker=:utriangle)

  lines!(ax_splits, x, y_splits; color=:maroon, linestyle=:solid, alpha=0.50)
  scatter!(ax_splits, x, y_splits; color=:maroon, marker=:circle)
  lines!(ax_splits, x, y_splits_fast; color=:maroon, linestyle=:dash, alpha=0.50)
  scatter!(ax_splits, x, y_splits_fast; color=:maroon, marker=:utriangle)

  lines!(ax_left_depth, x, y_left_depth; color=:purple, linestyle=:solid, alpha=0.50)
  scatter!(ax_left_depth, x, y_left_depth; color=:purple, marker=:circle)
  lines!(ax_left_depth, x, y_left_depth_fast; color=:purple, linestyle=:dash, alpha=0.50)
  scatter!(ax_left_depth, x, y_left_depth_fast; color=:purple, marker=:utriangle)

  lines!(ax_right_depth, x, y_right_depth; color=:darkgoldenrod, linestyle=:solid, alpha=0.50)
  scatter!(ax_right_depth, x, y_right_depth; color=:darkgoldenrod, marker=:circle)
  lines!(ax_right_depth, x, y_right_depth_fast; color=:darkgoldenrod, linestyle=:dash, alpha=0.50)
  scatter!(ax_right_depth, x, y_right_depth_fast; color=:darkgoldenrod, marker=:utriangle)

  lines!(ax_average_depth, x, y_average_depth; color=:teal, linestyle=:solid, alpha=0.50)
  scatter!(ax_average_depth, x, y_average_depth; color=:teal, marker=:circle)
  lines!(ax_average_depth, x, y_average_depth_fast; color=:teal, linestyle=:dash, alpha=0.50)
  scatter!(ax_average_depth, x, y_average_depth_fast; color=:teal, marker=:utriangle)

  lines!(ax_optimal_depth, x, y_optimal_depth; color=:navajowhite3, linestyle=:solid, alpha=0.50)
  scatter!(ax_optimal_depth, x, y_optimal_depth; color=:navajowhite3, marker=:circle)
  lines!(ax_optimal_depth, x, y_optimal_depth_fast; color=:navajowhite3, linestyle=:dash, alpha=0.50)
  scatter!(ax_optimal_depth, x, y_optimal_depth_fast; color=:navajowhite3, marker=:utriangle)

  lines!(ax_tree_balance, x, y_tree_balance; color=:seagreen, linestyle=:solid, alpha=0.50)
  scatter!(ax_tree_balance, x, y_tree_balance; color=:seagreen, marker=:circle)
  lines!(ax_tree_balance, x, y_tree_balance_fast; color=:seagreen, linestyle=:dash, alpha=0.50)
  scatter!(ax_tree_balance, x, y_tree_balance_fast; color=:seagreen, marker=:utriangle)

  lines!(ax_tree_quality, x, y_tree_quality; color=:steelblue, linestyle=:solid, alpha=0.50)
  scatter!(ax_tree_quality, x, y_tree_quality; color=:steelblue, marker=:circle)
  lines!(ax_tree_quality, x, y_tree_quality_fast; color=:steelblue, linestyle=:dash, alpha=0.50)
  scatter!(ax_tree_quality, x, y_tree_quality_fast; color=:steelblue, marker=:utriangle)

  # End ploting
  build_normal = [
    LineElement(color=:black, linestyle=:solid, alpha=0.50),
    MarkerElement(color=:black, marker=:circle, strokecolor=:black)
  ]

  build_fast = [
    LineElement(color=:black, linestyle=:dash, alpha=0.50),
    MarkerElement(color=:black, marker=:utriangle, strokecolor=:black)
  ]

  # Legend(fig[1, 2], [build_normal, build_fast], ["Normal build", "Fast build"])
  Label(fig[0, :], "ELFBSP Segment Split Cost Analysis - $(stem) $(map)", justification=:center, font=:bold, fontsize=20)

  # colsize!(fig.layout, 1, Relative(0.50))
  # colsize!(fig.layout, 2, Relative(0.50))

  save(joinpath(out_dir, "$(stem) $(map).png"), fig)
end
