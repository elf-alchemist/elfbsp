using CSV
using Tables
using Makie
using CairoMakie
using Base.Filesystem

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
stem = splitext(basename(file_path))[1]
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

maps = Dict{String, Dict{Bool, Vector{DataVisBSP}}}()

for ((map, is_fast), data) in levels
    by_mode = get!(maps, map, Dict{Bool, Vector{DataVisBSP}}())
    by_mode[is_fast] = data
end

styles = Dict(
  false => (linestyle = :solid, marker = :circle),
  true  => (linestyle = :dash,  marker = :utriangle)
)

colors = Dict(
  :nodes => :dodgerblue,
  :subsecs => :seagreen,
  :segs => :firebrick,
  :size_left => :purple,
  :size_right => :darkgoldenrod,
)

for (map, modes) in maps
  fig = Figure(size = (720, 1280))
  xt = 0:4:64

  ax = Axis(fig[1, 1], xticks = xt, ytickformat = "{:d}", title = "Nodes")
  ax2 = Axis(fig[2, 1], xticks = xt, ytickformat = "{:d}", title = "Subsectors")
  ax3 = Axis(fig[3, 1], xticks = xt, ytickformat = "{:d}", title = "Segments")

  ax4 = Axis(fig[4, 1], xticks = xt, ytickformat = "{:d}", title = "Tree depth left")
  ax5 = Axis(fig[5, 1], xticks = xt, ytickformat = "{:d}", title = "Tree depth right", xlabel = "Cost factor of splitting segments")

  linkxaxes!(ax, ax2, ax3)
  linkxaxes!(ax4, ax5)

  for is_fast in (false, true)
    haskey(modes, is_fast) || continue
    data = modes[is_fast]
    x = 1:length(data)

    y_nodes = getfield.(data, :num_nodes)
    y_subsecs = getfield.(data, :num_subsecs)
    y_segs = getfield.(data, :num_segs)
    y_size_left = getfield.(data, :size_left)
    y_size_right = getfield.(data, :size_right)

    st = styles[is_fast]

    lines!( ax, x, y_nodes; color = colors[:nodes], linestyle = st.linestyle, label = is_fast ? "Nodes (fast)" : "Nodes (normal)" )
    scatter!( ax, x, y_nodes; color = colors[:nodes], marker = st.marker )

    lines!( ax2, x, y_subsecs; color = colors[:subsecs], linestyle = st.linestyle, label = is_fast ? "Subsectors (fast)" : "Subsectors (normal)" )
    scatter!( ax2, x, y_subsecs; color = colors[:subsecs], marker = st.marker )

    lines!( ax3, x, y_segs; color = colors[:segs], linestyle = st.linestyle, label = is_fast ? "Segments (fast)" : "Segments (normal)" )
    scatter!( ax3, x, y_segs; color = colors[:segs], marker = st.marker )

    lines!( ax4, x, y_size_left; color = colors[:size_left], linestyle = st.linestyle, label = is_fast ? "Tree depth left (fast)" : "Tree depth left (normal)" )
    scatter!( ax4, x, y_size_left; color = colors[:size_left], marker = st.marker )

    lines!( ax5, x, y_size_right; color = colors[:size_right], linestyle = st.linestyle, label = is_fast ? "Tree depth right (fast)" : "Tree depth right (normal)" )
    scatter!( ax5, x, y_size_right; color = colors[:size_right], marker = st.marker )

  end

  Label(fig[0, :], "ELFBSP Split Cost Analysis - $(stem) $(map)", justification = :center, font = :bold, fontsize = 24)
  colsize!(fig.layout, 1, Relative(1.00))

  save("ELFBSP $(stem) $(map).png", fig)
end
