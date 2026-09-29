// game_main.cpp - Sideways City game executable. Start it from the launcher or directly
// (uses sideways.cfg; any setting can be overridden on the command line, e.g. +fullscreen 1).
#include "common/config.h"
#include "game/game.h"
#include "raylib.h"
#include "rlgl.h"
#include <algorithm>
#include <string>

int main(int argc, char** argv) {
    ChangeDirectory(GetApplicationDirectory());
    Config cfg;
    SetConfigDefaults(cfg);
    cfg.Load("sideways.cfg");
    cfg.ApplyArgs(argc, argv);

    unsigned flags = FLAG_WINDOW_RESIZABLE;
    if (cfg.GetBool("msaa", true)) flags |= FLAG_MSAA_4X_HINT;
    if (cfg.GetBool("vsync", true)) flags |= FLAG_VSYNC_HINT;
    SetConfigFlags(flags);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(cfg.GetInt("width", 1280), cfg.GetInt("height", 720), "Sideways City");
    SetWindowMinSize(800, 500);
    if (cfg.GetBool("fullscreen", false)) ToggleBorderlessWindowed();
    SetExitKey(KEY_NULL);   // Esc pauses and goes back in menus
    SetTargetFPS(std::max(0, cfg.GetInt("fps_max", 144)));
    rlDisableBackfaceCulling();   // 2D shapes are wound both ways
    if (!LoadFonts()) TraceLog(LOG_WARNING, "Fonts not found in assets/fonts, using the raylib default");

    Game game;
    game.Init(true);

    // Developer helpers for screenshots and checks:
    //   +debug_start stage:N | survive | free | garage | stages | controls   jump straight to a screen
    //   +debug_gas X  +debug_steer X  +debug_hand 1                           hold these inputs
    //   +debug_autoshot N                                                     save autoshot.png after N frames and quit
    //   +debug_pause 1  +debug_levelup 1  +debug_result 1                     open an overlay once the run is going
    const std::string start = cfg.Get("debug_start", "");
    if (start.rfind("stage:", 0) == 0) game.StartStage(std::clamp(std::atoi(start.c_str() + 6), 0, (int)STAGES.size() - 1));
    else if (start == "survive") game.StartSurvival();
    else if (start == "free") game.StartCity();
    else if (start == "garage") game.view = V_GARAGE;
    else if (start == "stages") game.view = V_STAGES;
    else if (start == "controls") game.view = V_CONTROLS;
    game.debugThr = cfg.GetFloat("debug_gas", 0);
    game.debugSteer = cfg.GetFloat("debug_steer", 0);
    game.debugHand = cfg.GetBool("debug_hand", false);
    game.debugNoFocusPause = !start.empty() || cfg.GetBool("debug_nofocuspause", false);
    const int autoShot = cfg.GetInt("debug_autoshot", 0);
    const bool dbgPause = cfg.GetBool("debug_pause", false), dbgLevel = cfg.GetBool("debug_levelup", false), dbgResult = cfg.GetBool("debug_result", false);
    const int dbgSkip = cfg.GetInt("debug_skip", 0);   // seconds of survival to skip ahead
    if (dbgSkip && game.surv) game.surv->t = dbgSkip;

    int frame = 0;
    while (!WindowShouldClose() && !game.WantsQuit()) {
        game.Frame(GetFrameTime());
        frame++;
        if (autoShot && frame == autoShot * 2 / 3) {
            if (dbgPause) game.Pause();
            if (dbgLevel && game.surv) { game.surv->xp = game.surv->next; }
            if (dbgResult && game.surv) game.SurvEnd(true);
        }
        if (autoShot && frame == autoShot) { TakeScreenshot("autoshot.png"); break; }
    }

    game.Shutdown();
    UnloadFonts();
    CloseWindow();
    return 0;
}
