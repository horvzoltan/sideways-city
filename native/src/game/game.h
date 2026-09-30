// game.h - Sideways City: a top-down drifting game.
// Drift stages with zones, clipping points and ghosts, and free drive.
#pragma once
#include "audio.h"
#include "car.h"
#include "common/config.h"
#include "desert_scene.h"
#include "draw.h"
#include "desert.h"
#include "tracks.h"
#include "ui.h"
#include <memory>
#include <string>

struct Paint { uint32_t hex; Color light, base, dark, roof, trim, vent, stripe; };
Paint MakePaint(uint32_t hex);
struct PaintPreset { const char* name; uint32_t hex; };
extern const PaintPreset PAINTS[8];

enum State { ST_MENU, ST_COUNT, ST_RACE, ST_FREE, ST_PAUSE, ST_DONE };
enum Mode { MODE_FREE, MODE_TRACK };
enum View { V_MAIN, V_STAGES, V_GARAGE, V_CONTROLS };

using Car = CarState;   // car.h
struct Chain { bool active = false; double pts = 0, time = 0; int mult = 1; double grace = 0; };
struct SmokePuff { double x, y, vx, vy, r, life; int kind = 0; };   // kind: 0 dust, 1 water spray, 2 wake ring
struct SkidMark { float x0, y0, x1, y1; };
struct ZoneState { double dist = 0, drift = 0; bool dirty = false, done = false; };
struct Ghost { int score = 0; std::vector<int> d; };   // flat [x, y, angle*1000, ...] every GHOST_DT seconds
struct Run {
    int i = 0;
    const StageDef* def = nullptr;
    int lap = 0, pi = 3, flags = 0, zi = -1;
    double t = 0, wrongT = 0;
    std::vector<ZoneState> zs;
    std::vector<std::vector<bool>> clipHit;   // per zone, per clip
    std::vector<int> rec;
    bool hasGhost = false;
    Ghost ghost;
};

struct Popup { std::string text; Color color; float t = 0, alpha = 0; };

class Game {
public:
    bool Init(bool desktopQuit);
    void Shutdown();
    void Frame(float dt);   // input, simulation and drawing for one frame
    bool WantsQuit() const { return quit; }

    // ---- shared with render.cpp / menus.cpp ----
    Audio audio;
    Ui ui;
    DesertWorld world;       // free roam: the endless desert (desert.cpp)
    Mode mode = MODE_FREE;
    State state = ST_MENU;
    std::unique_ptr<Track> T;
    DesertScene desert;       // how the stage looks (desert_scene.cpp)
    double shadowX = 0.7, shadowY = 0.9;   // where shadows fall, per unit of height (set from the stage's sun)
    std::unique_ptr<Run> run;
    Car car;
    Handling handling;       // all the handling constants (car.h)
    bool showDebug = false;  // F3: handling numbers
    struct { double x = 0, y = 0, z = 1; } cam;
    // The view: top-down, or isometric (the world turned 45 degrees and squashed 2:1). Only the
    // drawing changes; the simulation stays in flat world coordinates. `up` is the world offset of
    // one unit of screen height while the world is being drawn: (-sqrt2, -sqrt2) in isometric, zero
    // otherwise (top-down uses the fake perspective, and the menus draw the car flat).
    bool iso = false;
    V2 up{0, 0};
    static constexpr double ISO_ROT = 45, ISO_SQUASH = 0.5;
    static constexpr double ISO_H = 220;   // world pixels of height per unit of Scen::ht in isometric
    V2 TopOf(double x, double y, double ht) const {   // where the top of something ht high is drawn
        if (up.x || up.y) return {x + up.x * ht * ISO_H, y + up.y * ht * ISO_H};
        return {x + (x - cam.x) * ht, y + (y - cam.y) * ht};
    }
    Vector2 ToScreen(double x, double y) const;   // world to pixels through the camera (no shake)
    void SetView(bool isometric);                  // switches, saves and announces the view
    Chain chain;
    int score = 0, best = 0;
    std::vector<SmokePuff> smoke;
    double water = 0, wakeT = 0;   // free roam: how deep the car is wading (0..1), time to the next wake ring
    std::vector<SkidMark> skids;
    bool hasPrevWheels = false;
    V2 prevWheels[2];
    double shake = 0;
    Paint paint;
    uint32_t paintHex = 0xf0631a;
    ParkMiller rnd;
    float W = 1280, H = 720, S = 1;    // screen size in pixels, and UI scale (CSS px -> pixels)
    double now = 0;                    // seconds since start, for animation

    // input
    struct { bool up, down, left, right, hand; } keys{};
    float padSteer = 0, padThr = 0, padBrk = 0;
    float padHand = 0;   // X / R1: handbrake
    double ThrottleIn() const { return std::max(keys.up ? 1.0 : 0.0, (double)padThr); }
    double BrakeIn() const { return std::max(keys.down ? 1.0 : 0.0, (double)padBrk); }
    double HandbrakeIn() const { return std::max(keys.hand ? 1.0 : 0.0, (double)padHand); }
    double SteerInput() const;

    // HUD messages
    Popup toast, zmsg;
    bool bannerOn = false;
    std::string bannerN, bannerT, bannerD;
    double countT = 0, goT = 0;
    int lastBeep = 0;
    // result card
    bool resultOn = false;
    double resultDelay = 0;
    std::string rTitle, rScore, rBest;
    bool rGood = false, rNext = false;
    int rStars = 0;
    // menus
    View view = V_MAIN;
    int stageSel = 0;
    State pausedFrom = ST_FREE;
    bool desktopQuit = true, quit = false;
    float debugThr = 0, debugSteer = 0;   // held inputs for screenshots (+debug_gas, +debug_steer)
    bool debugHand = false, debugNoFocusPause = false;
    bool customPaint = false;
    float customH = 0, customS = 1, customV = 1;

    // persistence (save.cfg and ghosts/)
    Config save;
    void Save() { save.Save("save.cfg"); }
    struct StageProg { int best = 0, stars = 0; };
    StageProg Prog(int i) const;
    bool LoadGhost(int i, Ghost& g) const;
    bool SaveGhost(int i, const Ghost& g) const;
    bool GhostExists(int i) const;

    // game flow
    void ShowMenu(View v = V_MAIN);
    void StartStage(int i);
    void StartFree();
    void Pause();
    void Resume();
    void ResetCar();
    void Wreck();
    void Bank();
    void Msg(const std::string& t, Color c);
    void Toast(const std::string& t, Color c);
    void ChoosePaint(uint32_t hex);

    // tracks
    struct Nearest { int i; double d, px, py; };
    Nearest NearestOn(double x, double y, int i0, int i1) const;
    Nearest NearestAll(double x, double y) const { return NearestOn(x, y, 0, T->N - 1); }
    V2 GhostAt(double t, double& a, bool& ok) const;

    // world queries
    int SurfAt(double x, double y);   // on a stage: road or gravel
    bool CollideCircle(double cx, double cy, double r, Hit& h);

    // render.cpp
    void Render(double speed);
    void DrawCar(double x, double y, double a, double w, bool braking, const Paint& p, float alpha = 1);
    void DrawPalm(const Scen& p);
    void DrawGarageCar(Rectangle r);
    void DrawTrackPreview(int i, Rectangle r);
    void BuildMini();
    void DrawMini(Rectangle r);

    // menus.cpp
    void DrawMenus();
    void DrawHud(double speed);
    void DrawOverlays();

private:
    void HandleInput(float dt);
    void PollPad(float dt);
    double Update(double dt);
    void TrackTick(double dt, bool drifting, int surf);
    void JudgeZone(int k);
    void NewLapZones();
    void Finish(bool completed);
    void LoadStage(int i);
    void RecordGhost();
    void ClearFx();
    void HideOverlays();

    // pad edges
    struct { bool a, b, y, start, up, down, left, right; } padPrev{};
    bool trigSeen[2] = {false, false};

    // minimap backgrounds
    RenderTexture2D miniBg{};
    bool miniOk = false;
    struct { double s = 1, ox = 0, oy = 0; } mm;
    friend class Ui;
};

std::string FmtNum(long long v);    // 12,345
std::string FmtT(double t);         // m:ss, rounded up
extern const Color INK, SAND_C, MUTED, ACCENT_C, BAD, GOOD;
