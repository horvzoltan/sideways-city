// launcher_main.cpp - Sideways City desktop launcher: pick the display settings, then start the game.
#include "../common/config.h"
#include "process.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

const Color BG{6, 9, 18, 255}, PANEL{14, 20, 36, 255}, ACCENT{255, 207, 58, 255}, CYAN{47, 214, 255, 255},
            TEXT{243, 234, 210, 255}, DIM{150, 156, 172, 255}, FIELD{28, 34, 52, 255};

bool Button(Rectangle r, const char* label, int fs, bool enabled = true, bool primary = false) {
    bool hover = enabled && CheckCollisionPointRec(GetMousePosition(), r);
    Color bg = !enabled ? Color{30, 34, 46, 255} : primary ? (hover ? Color{255, 217, 94, 255} : ACCENT)
                                                           : (hover ? Color{40, 60, 84, 255} : FIELD);
    DrawRectangleRec(r, bg);
    Color tc = !enabled ? DIM : primary ? Color{28, 28, 28, 255} : TEXT;
    DrawText(label, (int)(r.x + r.width / 2 - MeasureText(label, fs) / 2), (int)(r.y + r.height / 2 - fs / 2), fs, tc);
    return hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

// "< value >" selector
bool Cycle(float x, float y, float w, const char* label, int& idx, const std::vector<std::string>& options) {
    DrawText(label, (int)x, (int)y, 16, DIM);
    Rectangle left{x, y + 22, 30, 30}, right{x + w - 30, y + 22, 30, 30};
    Rectangle mid{x + 32, y + 22, w - 64, 30};
    DrawRectangleRec(mid, FIELD);
    const std::string& v = options[idx];
    DrawText(v.c_str(), (int)(mid.x + mid.width / 2 - MeasureText(v.c_str(), 18) / 2), (int)(mid.y + 6), 18, TEXT);
    bool changed = false;
    if (Button(left, "<", 18)) { idx = (idx + (int)options.size() - 1) % (int)options.size(); changed = true; }
    if (Button(right, ">", 18)) { idx = (idx + 1) % (int)options.size(); changed = true; }
    return changed;
}

bool Check(float x, float y, const char* label, bool& v) {
    Rectangle box{x, y, 22, 22};
    DrawRectangleRec(box, FIELD);
    if (v) DrawRectangle((int)x + 5, (int)y + 5, 12, 12, ACCENT);
    DrawText(label, (int)x + 32, (int)y + 3, 17, TEXT);
    Rectangle hit{x, y, 32.0f + MeasureText(label, 17), 22};
    if (CheckCollisionPointRec(GetMousePosition(), hit) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) { v = !v; return true; }
    return false;
}

template <typename T> int IndexOf(const std::vector<T>& v, const T& x, int def) {
    auto it = std::find(v.begin(), v.end(), x);
    return it == v.end() ? def : (int)(it - v.begin());
}
}  // namespace

int main() {
    ChangeDirectory(GetApplicationDirectory());
    Config cfg;
    SetConfigDefaults(cfg);
    cfg.Load("sideways.cfg");

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(760, 460, "Sideways City Launcher");
    SetTargetFPS(60);
    if (FileExists("assets/icon.png")) { Image icon = LoadImage("assets/icon.png"); SetWindowIcon(icon); UnloadImage(icon); }
    Font font = FileExists("assets/fonts/chakra-petch-latin-700-normal.ttf") ? LoadFontEx("assets/fonts/chakra-petch-latin-700-normal.ttf", 64, nullptr, 0) : GetFontDefault();
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    // ---- current values ----
    std::vector<std::string> resolutions = {"1280x720", "1366x768", "1600x900", "1920x1080", "2560x1440", "3840x2160", "1024x768", "1280x960"};
    int resIdx = IndexOf(resolutions, cfg.Get("width", "1280") + "x" + cfg.Get("height", "720"), 0);
    bool fullscreen = cfg.GetBool("fullscreen", false), vsync = cfg.GetBool("vsync", true), msaa = cfg.GetBool("msaa", true);
    std::vector<std::string> fpsOpts = {"60", "75", "120", "144", "165", "240", "unlimited"};
    std::string fpsCur = cfg.Get("fps_max", "144");
    int fpsIdx = IndexOf(fpsOpts, fpsCur == "0" ? std::string("unlimited") : fpsCur, 3);

    std::string status;
    float statusTime = 0;
    bool launched = false;

    auto save = [&]() {
        const std::string& r = resolutions[resIdx];
        size_t x = r.find('x');
        cfg.Set("width", r.substr(0, x));
        cfg.Set("height", r.substr(x + 1));
        cfg.SetBool("fullscreen", fullscreen);
        cfg.SetBool("vsync", vsync);
        cfg.SetBool("msaa", msaa);
        cfg.Set("fps_max", fpsOpts[fpsIdx] == "unlimited" ? "0" : fpsOpts[fpsIdx]);
        return cfg.Save("sideways.cfg");
    };

    while (!WindowShouldClose() && !launched) {
        if (statusTime > 0) statusTime -= GetFrameTime();

        BeginDrawing();
        ClearBackground(BG);
        // header: the logo, slanted like the game's menu
        DrawRectangle(0, 0, 760, 104, {4, 7, 16, 255});
        DrawRectangleGradientH(0, 104, 420, 4, CYAN, {47, 214, 255, 0});
        DrawTextPro(font, "SIDEWAYS", {36, 20}, {0, 0}, 0, 44, 0, WHITE);
        DrawTextPro(font, "CITY", {40 + MeasureTextEx(font, "SIDEWAYS ", 44, 0).x, 20}, {0, 0}, 0, 44, 0, ACCENT);
        DrawText("Top-down drifting  |  12 stages, survival, free drive", 38, 72, 16, DIM);

        DrawRectangle(36, 128, 688, 220, PANEL);
        DrawRectangle(36, 128, 4, 220, CYAN);
        float c1 = 64, c2 = 400, w = 260;
        DrawText("DISPLAY", (int)c1, 146, 18, CYAN);
        Cycle(c1, 176, w, "Resolution", resIdx, resolutions);
        Cycle(c1, 238, w, "FPS limit", fpsIdx, fpsOpts);
        DrawText("OPTIONS", (int)c2, 146, 18, CYAN);
        Check(c2, 190, "Fullscreen (F11 in game)", fullscreen);
        Check(c2, 226, "VSync", vsync);
        Check(c2, 262, "Anti-aliasing (MSAA 4x)", msaa);
        DrawText("Sound volume, paint and progress are set in the game.", (int)c1, 314, 16, DIM);

        // footer
        if (Button({524, 372, 200, 56}, "PLAY", 28, true, true)) {
            save();
#ifdef _WIN32
            std::string exe = std::string(GetApplicationDirectory()) + "sideways.exe";
#else
            std::string exe = std::string(GetApplicationDirectory()) + "sideways";
#endif
            std::string err;
            if (LaunchDetached(exe, &err)) launched = true;
            else { status = "Could not start the game: " + err; statusTime = 6; }
        }
        if (Button({380, 372, 130, 56}, "SAVE", 20)) { status = save() ? "Settings saved to sideways.cfg" : "Could not write sideways.cfg"; statusTime = 3; }
        if (statusTime > 0) DrawText(status.c_str(), 36, 392, 16, status.rfind("Could", 0) == 0 ? Color{255, 106, 85, 255} : TEXT);
        else DrawText("In game: Esc pause  R reset  M mute", 36, 392, 16, DIM);
        EndDrawing();
    }
    if (font.texture.id != GetFontDefault().texture.id) UnloadFont(font);
    CloseWindow();
    return 0;
}
