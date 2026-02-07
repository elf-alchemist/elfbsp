
#include <filesystem>

#include "SDL3/SDL_dialog.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_video.h"

#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "imgui.h"

#include "core.hpp"
#include "local.hpp"

namespace fs = std::filesystem;

//------------------------------------------------------------------------------

enum Job
{
  IDLE,
  BUSY,
  SUCCESS,
  ERROR,
};

//------------------------------------------------------------------------------

struct ViewContext_t
{
  // General context data
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  ImGuiIO *io = nullptr;

  // User requests
  std::string wad_path_from_dialog = "";
  bool build_all_levels = false;

  // Loaded data
  std::string wad_path = "";
  std::string wad_name = "";
  size_t wad_levels = 0;
  Job wad_build_state = Job::IDLE;
  ImGuiTextBuffer logger;
  bool scroll_to_bottom = true;

  // Abort
  bool active = true;
} ViewContext;

//------------------------------------------------------------------------------

void PRINTF_ATTR(2, 3) PrintLineGUI(const log_level_t level, const char *fmt, ...)
{
  char buffer[MSG_BUFFER_LENGTH];

  va_list arg_ptr;

  va_start(arg_ptr, fmt);
  M_vsnprintf(buffer, fmt, arg_ptr);
  va_end(arg_ptr);

  buffer[MSG_BUFFER_LENGTH - 1] = '\0';

  ViewContext.logger.appendf("%s\n", buffer);
  ViewContext.scroll_to_bottom = true; // Signal the UI to scroll
}

//------------------------------------------------------------------------------

static void SystemInit(ViewContext_t &ctx)
{
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, PROJECT_NAME);
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, PROJECT_VERSION);
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, PROJECT_APPID);
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING, PROJECT_COMPANY);
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_COPYRIGHT_STRING, PROJECT_COPYRIGHT);
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_URL_STRING, PROJECT_URL);
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, PROJECT_TYPE);

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
  {
    PrintLine(LOG_WARN, "SDL_Init failed: %s", SDL_GetError());
    exit(4);
  }

  ctx.window = SDL_CreateWindow(PROJECT_NAME, 1280, 720, SDL_WINDOW_RESIZABLE);
  if (!ctx.window)
  {
    PrintLine(LOG_WARN, "SDL_CreateWindow failed: %s", SDL_GetError());
    SDL_Quit();
    exit(4);
  }

  SDL_PropertiesID render_props = SDL_CreateProperties();
  SDL_SetPointerProperty(render_props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, ctx.window);
  SDL_SetNumberProperty(render_props, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, 1);
  ctx.renderer = SDL_CreateRendererWithProperties(render_props);
  SDL_DestroyProperties(render_props);

  if (!ctx.renderer)
  {
    PrintLine(LOG_WARN, "SDL_CreateRenderer failed: %s", SDL_GetError());
    SDL_DestroyWindow(ctx.window);
    SDL_Quit();
    exit(4);
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ctx.io = &ImGui::GetIO();
  ctx.io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  ImGui_ImplSDL3_InitForSDLRenderer(ctx.window, ctx.renderer);
  ImGui_ImplSDLRenderer3_Init(ctx.renderer);

  PrintLine = PrintLineGUI;
}

static void SystemShutdown(ViewContext_t &ctx)
{
  PrintLine = PrintLineCLI;

  CloseWad();

  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyRenderer(ctx.renderer);
  SDL_DestroyWindow(ctx.window);
  SDL_Quit();
}

//------------------------------------------------------------------------------

void WadFileDialog(ViewContext_t &ctx)
{
  const SDL_DialogFileFilter wad_filter[] = {
    {"Doom WAD File", "wad"},
  };

  auto WadFileDialogCallback = [](void *userdata, const char *const *filelist, int) -> void
  {
    if (!filelist)
    {
      PrintLine(LOG_WARN, "An error ocurred: %s", SDL_GetError());
      return;
    }

    if (!*filelist)
    {
      PrintLine(LOG_WARN, "Dialog canceled.");
      return;
    }

    for (int i = 0; filelist[i] != NULL; i++)
    {
      PrintLine(LOG_NORMAL, "Selected file: %s", filelist[i]);
      auto *ctx = static_cast<ViewContext_t *>(userdata);
      ctx->wad_path_from_dialog = filelist[i];
    }
  };

  SDL_ShowOpenFileDialog(WadFileDialogCallback, &ctx, ctx.window, wad_filter, SDL_arraysize(wad_filter), nullptr, false);
}

//------------------------------------------------------------------------------

static int SDLCALL BuildAllLevels(void *data)
{
  ViewContext_t *ctx = static_cast<ViewContext_t *>(data);

  // loop over each level in the wad
  for (size_t n = 0; n < ctx->wad_levels; n++)
  {
    BuildLevel(n, ctx->wad_path.c_str());
  }

  ctx->wad_build_state = Job::SUCCESS;
  return 0;
};

//------------------------------------------------------------------------------

void ProcessEvents(ViewContext_t &ctx)
{
  // Load selected WAD
  if (!ctx.wad_path_from_dialog.empty())
  {
    ctx.wad_path = ctx.wad_path_from_dialog;
    ctx.wad_path_from_dialog.clear();

    auto wad_fs_path = fs::path(ctx.wad_path);
    ctx.wad_name = wad_fs_path.filename().string();

    OpenWad(ctx.wad_path.c_str());
    ctx.wad_levels = LevelsInWad();
  }

  // Build all levels form selected WAD
  if (ctx.build_all_levels)
  {
    ctx.build_all_levels = false;
    if (ctx.wad_build_state != Job::BUSY)
    {
      ctx.wad_build_state = Job::BUSY;
      SDL_CreateThreadRuntime(BuildAllLevels, "BuildAllLevels", &ctx, nullptr, nullptr);
    }
  }
}

//------------------------------------------------------------------------------

static auto MainWindow = [](ViewContext_t &ctx) -> void
{
  ImGui::SetNextWindowSizeConstraints(ImVec2(200, -1), ImVec2(800, -1));
  ImGui::Begin("ELFBSP", &ctx.active, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("FPS: %.1f", ctx.io->Framerate);

  if (ImGui::BeginMenuBar())
  {
    if (ImGui::BeginMenu("File"))
    {
      if (ImGui::MenuItem("Open..", "Ctrl+O"))
      {
        WadFileDialog(ctx);
      }
      if (ImGui::MenuItem("Close", "Ctrl+W"))
      {
        ctx.active = false;
      }
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }

  if (!ctx.wad_path.empty())
  {
    ImGui::Text("Loaded: %s", ctx.wad_name.c_str());
    ImGui::Text("Level count: %zu", ctx.wad_levels);
    switch (ctx.wad_build_state)
    {
    case Job::IDLE:
      ImGui::Text("Job status: Idle.");
      break;
    case Job::BUSY:
      ImGui::Text("Job status: Building...");
      break;
    case Job::SUCCESS:
      ImGui::Text("Job status: Done.");
      break;
    case Job::ERROR:
      ImGui::Text("Job status: Error.");
      break;
    }
    if (ImGui::Button("Build All Levels"))
    {
      ctx.build_all_levels = true;
    }
  }

  ImGui::End();
};

static auto WadWindow = [](ViewContext_t &ctx) -> void
{
  // GOD THIS SUCKS I NEED CONTEXT STRUCTS
  if (ctx.wad_levels == 0 && lev_current_idx >= 0)
  {
    return;
  }
  ImGui::Begin("Currently loaded", &ctx.active, ImGuiWindowFlags_AlwaysAutoResize);

  ImGui::Text("Level: %s", GetLevelName(lev_current_idx));

  ImGui::Text("Vertices: %zu", lev_vertices.size());
  ImGui::Text("Lines: %zu", lev_linedefs.size());
  ImGui::Text("Sides: %zu", lev_sidedefs.size());
  ImGui::Text("Sectors: %zu", lev_sectors.size());
  ImGui::Text("Things: %zu", lev_things.size());

  ImGui::Text("Walltips: %zu", lev_walltips.size());

  ImGui::Text("Nodes: %zu", lev_nodes.size());
  ImGui::Text("Subsectors: %zu", lev_subsecs.size());
  ImGui::Text("Segments: %zu", lev_segs.size());

  ImGui::End();
};

static auto ConfigWindow = [](ViewContext_t &ctx) -> void
{
  ImGui::Begin("Settings and debug logging", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  if (ImGui::BeginTable("ConfigWindow", 2, ImGuiTableFlags_BordersInnerV))
  {
    ImGui::TableNextColumn();

    ImGui::InputDouble("Segment split cost", &config.split_cost, 1.0, 1.0, "%.0f");
    ImGui::Checkbox("Use faster node-picking algorithm", &config.fast);
    ImGui::Checkbox("Make backup of loaded WAD", &config.backup);
    ImGui::Checkbox("Analisys mode", &config.analysis);
    ImGui::Checkbox("Verbose logging", &config.verbose);

    ImGui::TableNextColumn();

    ImGui::CheckboxFlags("BLOCKMAP generation", &config.debug, DEBUG_BLOCKMAP);
    ImGui::CheckboxFlags("REJECT generation", &config.debug, DEBUG_REJECT);
    ImGui::CheckboxFlags("Map data loading", &config.debug, DEBUG_LOAD);
    ImGui::CheckboxFlags("BSP lump writing", &config.debug, DEBUG_BSP);
    ImGui::CheckboxFlags("Walltips calculation", &config.debug, DEBUG_WALLTIPS);
    ImGui::CheckboxFlags("PolyObjs marking", &config.debug, DEBUG_POLYOBJ);
    ImGui::CheckboxFlags("Vertices overlapping", &config.debug, DEBUG_OVERLAPS);
    ImGui::CheckboxFlags("Node-picking steps", &config.debug, DEBUG_PICKNODE);
    ImGui::CheckboxFlags("Segment splitting", &config.debug, DEBUG_SPLIT);
    ImGui::CheckboxFlags("Cut list and minisegs", &config.debug, DEBUG_CUTLIST);
    ImGui::CheckboxFlags("Node builder", &config.debug, DEBUG_BUILDER);
    ImGui::CheckboxFlags("Cloackwise segment sorting", &config.debug, DEBUG_SORTER);
    ImGui::CheckboxFlags("Subsector generation", &config.debug, DEBUG_SUBSEC);
    ImGui::CheckboxFlags("WAD loading", &config.debug, DEBUG_WAD);

    ImGui::EndTable();
  }

  ImGui::End();
};

//------------------------------------------------------------------------------

void HandleKeyboardInput(ViewContext_t &ctx, SDL_Event &event)
{
  switch (event.type)
  {
  case SDL_EVENT_KEY_DOWN:
    {
      bool ctrl_pressed = false;
      if constexpr (MACOS)
      {
        ctrl_pressed = (event.key.mod & SDL_KMOD_GUI) != 0;
      }
      else
      {
        ctrl_pressed = (event.key.mod & SDL_KMOD_CTRL) != 0;
      }

      switch (event.key.key)
      {
      case SDLK_O:
        if (ctrl_pressed)
        {
          WadFileDialog(ctx);
        }
        break;
      case SDLK_W:
        if (ctrl_pressed)
        {
          ctx.active = false;
        }
        break;
      }
    }
    break;

  default:
    break;
  }
}

static void SystemLoop(ViewContext_t &ctx)
{
  auto EventPoller = [](ViewContext_t &ctx) -> void
  {
    SDL_Event event;
    SDL_zero(event);
    while (SDL_PollEvent(&event))
    {
      ImGui_ImplSDL3_ProcessEvent(&event);

      if (event.type == SDL_EVENT_QUIT)
      {
        ctx.active = false;
        return;
      }

      if (ImGui::GetIO().WantCaptureKeyboard)
      {
        // continue;
      }

      HandleKeyboardInput(ctx, event);
    }
  };

  while (ctx.active)
  {
    // Pre-process
    EventPoller(ctx);
    ProcessEvents(ctx);

    // Frame
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // UI
    MainWindow(ctx);
    WadWindow(ctx);
    ConfigWindow(ctx);

    // Render
    ImGui::Render();
    SDL_SetRenderDrawColor(ctx.renderer, 20, 20, 20, 255);
    SDL_RenderClear(ctx.renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), ctx.renderer);
    SDL_RenderPresent(ctx.renderer);
  }
}

void EnterGUI(void)
{
  SystemInit(ViewContext);
  SystemLoop(ViewContext);
  SystemShutdown(ViewContext);
}
