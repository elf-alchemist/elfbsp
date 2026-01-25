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
  num_segs::Int64
  num_subsecs::Int64
  num_nodes::Int64
  size_left::Int64
  size_right::Int64
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
      row.num_segs,
      row.num_subsecs,
      row.num_nodes,
      row.size_left,
      row.size_right
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

  fig = Figure(size=(250 .* (4, 3)))
  xt = 0:4:64

  ax = Axis(fig[1, 1], xticks=xt, ytickformat=ytick_int, ylabel="Nodes")
  ax2 = Axis(fig[2, 1], xticks=xt, ytickformat=ytick_int, ylabel="Subsectors")
  ax3 = Axis(fig[3, 1], xticks=xt, ytickformat=ytick_int, ylabel="Segments", xlabel="Cost factor of splitting segments")

  ax4 = Axis(fig[2, 2], xticks=xt, ytickformat=ytick_int, ylabel="Tree depth left", yaxisposition=:right)
  ax5 = Axis(fig[3, 2], xticks=xt, ytickformat=ytick_int, ylabel="Tree depth right", yaxisposition=:right, xlabel="Cost factor of splitting segments")

  linkxaxes!(ax, ax2, ax3)
  linkxaxes!(ax4, ax5)

  # Start ploting
  data_normal = modes[false]
  data_fast = modes[true]
  x = 1:length(data_normal)

  y_nodes = getfield.(data_normal, :num_nodes)
  y_subsecs = getfield.(data_normal, :num_subsecs)
  y_segs = getfield.(data_normal, :num_segs)
  y_size_left = getfield.(data_normal, :size_left)
  y_size_right = getfield.(data_normal, :size_right)

  y_nodes_fast = getfield.(data_fast, :num_nodes)
  y_subsecs_fast = getfield.(data_fast, :num_subsecs)
  y_segs_fast = getfield.(data_fast, :num_segs)
  y_size_left_fast = getfield.(data_fast, :size_left)
  y_size_right_fast = getfield.(data_fast, :size_right)

  lines!(ax, x, y_nodes; color=:dodgerblue, linestyle=:solid, alpha=0.50)
  scatter!(ax, x, y_nodes; color=:dodgerblue, marker=:circle)
  lines!(ax, x, y_nodes_fast; color=:dodgerblue, linestyle=:dash, alpha=0.50)
  scatter!(ax, x, y_nodes_fast; color=:dodgerblue, marker=:utriangle)

  lines!(ax2, x, y_subsecs; color=:seagreen, linestyle=:solid, alpha=0.50)
  scatter!(ax2, x, y_subsecs; color=:seagreen, marker=:circle)
  lines!(ax2, x, y_subsecs_fast; color=:seagreen, linestyle=:dash, alpha=0.50)
  scatter!(ax2, x, y_subsecs_fast; color=:seagreen, marker=:utriangle)

  lines!(ax3, x, y_segs; color=:firebrick, linestyle=:solid, alpha=0.50)
  scatter!(ax3, x, y_segs; color=:firebrick, marker=:circle)
  lines!(ax3, x, y_segs_fast; color=:firebrick, linestyle=:dash, alpha=0.50)
  scatter!(ax3, x, y_segs_fast; color=:firebrick, marker=:utriangle)

  lines!(ax4, x, y_size_left; color=:purple, linestyle=:solid, alpha=0.50)
  scatter!(ax4, x, y_size_left; color=:purple, marker=:circle)
  lines!(ax4, x, y_size_left_fast; color=:purple, linestyle=:dash, alpha=0.50)
  scatter!(ax4, x, y_size_left_fast; color=:purple, marker=:utriangle)

  lines!(ax5, x, y_size_right; color=:darkgoldenrod, linestyle=:solid, alpha=0.50)
  scatter!(ax5, x, y_size_right; color=:darkgoldenrod, marker=:circle)
  lines!(ax5, x, y_size_right_fast; color=:darkgoldenrod, linestyle=:dash, alpha=0.50)
  scatter!(ax5, x, y_size_right_fast; color=:darkgoldenrod, marker=:utriangle)

  # End ploting
  build_normal = [
    LineElement(color=:black, linestyle=:solid, alpha=0.50),
    MarkerElement(color=:black, marker=:circle, strokecolor=:black)
  ]

  build_fast = [
    LineElement(color=:black, linestyle=:dash, alpha=0.50),
    MarkerElement(color=:black, marker=:utriangle, strokecolor=:black)
  ]

  Legend(fig[1, 2], [build_normal, build_fast], ["Normal build", "Fast build"])
  Label(fig[0, :], "ELFBSP Segment Split Cost Analysis - $(stem) $(map)", justification=:center, font=:bold, fontsize=20)

  colsize!(fig.layout, 1, Relative(0.50))
  colsize!(fig.layout, 2, Relative(0.50))

  save(joinpath(out_dir, "$(stem) $(map).png"), fig)
end
