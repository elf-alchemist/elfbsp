

#include "SDL3/SDL_dialog.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_video.h"

#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "imgui.h"

#include "core.hpp"

//------------------------------------------------------------------------------

struct RenderContext_t
{
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  ImGuiIO *io = nullptr;
} RenderContext;

//------------------------------------------------------------------------------

static void SystemInit(RenderContext_t &ctx)
{
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, PROJECT_NAME);
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, PROJECT_VERSION);
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, PROJECT_APPID);
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING, PROJECT_COMPANY);
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_COPYRIGHT_STRING, PROJECT_COPYRIGHT);
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_URL_STRING, PROJECT_URL);
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, PROJECT_TYPE);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0)
  {
    PrintLine(LOG_WARN, "SDL_Init failed: %s", SDL_GetError());
    exit(4);
  }

  ctx.window = SDL_CreateWindow(nullptr, 1280, 720, SDL_WINDOW_RESIZABLE);
  if (!ctx.window)
  {
    PrintLine(LOG_WARN, "SDL_CreateWindow failed: %s", SDL_GetError());
    SDL_Quit();
    exit(4);
  }

  ctx.renderer = SDL_CreateRenderer(ctx.window, nullptr);
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
}

static void SystemShutdown(RenderContext_t &ctx)
{
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyRenderer(ctx.renderer);
  SDL_DestroyWindow(ctx.window);
  SDL_Quit();
}

//------------------------------------------------------------------------------

constexpr void WadFileDialog(RenderContext_t &ctx)
{
  constexpr SDL_DialogFileFilter wad_filter[] = {
    {"Doom WAD File", "wad"},
  };

  auto WadFileDialogCallback = [](void *, const char *const *filelist, int) -> void
  {
    if (!filelist)
    {
      SDL_Log("An error ocurred: %s", SDL_GetError());
      return;
    }

    if (!*filelist)
    {
      SDL_Log("Dialog canceled.");
      return;
    }

    for (int i = 0; filelist[i] != NULL; i++)
    {
      SDL_Log("Selected file: %s", filelist[i]);
    }
  };

  SDL_ShowOpenFileDialog(WadFileDialogCallback, nullptr, ctx.window, wad_filter, SDL_arraysize(wad_filter), nullptr, false);
}

//------------------------------------------------------------------------------

static auto WelcomeDialog = [](RenderContext_t &ctx) -> void
{
  ImGui::Begin("Welcome to ELFBSP");
  ImGui::Text("FPS: %.1f", ctx.io->Framerate);
  if (ImGui::Button("Open WAD.."))
  {
    WadFileDialog(ctx);
  }
  ImGui::End();
};

static void SystemLoop(RenderContext_t &ctx)
{
  SDL_Event event;
  bool running = true;

  auto EventPoller = [&](void) -> void
  {
    while (SDL_PollEvent(&event))
    {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT)
      {
        running = false;
      }
    }
  };

  while (running)
  {
    EventPoller();

    // Frame
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // UI
    WelcomeDialog(ctx);

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
  SystemInit(RenderContext);
  SystemLoop(RenderContext);
  SystemShutdown(RenderContext);
}
