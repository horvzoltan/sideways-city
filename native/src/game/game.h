// game.h - Sideways City: a top-down drifting game (port of js/game.js).
// Free drive around a little Miami Beach, twelve drift stages with zones, clipping points and
// ghosts, and "Survive the night", a horde mode in an endless generated city.
#pragma once
#include "audio.h"
#include "city.h"
#include "common/config.h"
#include "draw.h"
#include "miami.h"
#include "tracks.h"
#include "ui.h"
#include <array>
#include <climits>
#include <memory>
#include <string>

struct Theme {
    uint32_t out, gravel, asph, barrier0, barrier1;
    std::vector<ScenKind> deco;
    std::vector<uint32_t> cols;
    double dense = 1;
    bool night = false;
    double grip = 0;   // below 1 makes the stage slippery (ice)
};

struct Paint { uint32_t hex; Color light, base, dark, roof, trim, vent, stripe; };
Paint MakePaint(uint32_t hex);
struct PaintPreset { const char* name; uint32_t hex; };
extern const PaintPreset PAINTS[8];

enum State { ST_MENU, ST_COUNT, ST_RACE, ST_FREE, ST_PAUSE, ST_LEVELUP, ST_DONE };
enum Mode { MODE_CITY, MODE_TRACK };
enum View { V_MAIN, V_STAGES, V_GARAGE, V_CONTROLS };

struct Car { double x = 0, y = 0, a = 0, vx = 0, vy = 0, w = 0; };
struct Chain { bool active = false; double pts = 0, time = 0; int mult = 1; double grace = 0; };
struct SmokePuff { double x, y, vx, vy, r, life; };
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

// ---- survival ----
enum EnemyType { E_WALKER, E_RUNNER, E_BRUTE, E_BOSS };
struct Enemy {
    EnemyType type;
    double x, y, r, hp, max, spd, dmg;
    int xp;
    uint32_t col;
    double a = 0, hitT = 0, flash = 0, slow = 0, stuckA = 0, stuckT = 0;
    int sd = 1;
    double wob = 0;
    bool dead = false;
};
struct Gem { double x, y; int v = 0; bool kit = false, pull = false; };
struct Fire { double x, y, life, max; };
struct Oil { double x, y, r, life, a; };
struct Splat { double x, y, r, a; uint32_t col; };
struct Zap { double x1, y1, life, seed; };
enum Upgrade { U_FIRE, U_TOXIC, U_BUMPER, U_TESLA, U_OIL, U_BURNER, U_MAGNET, U_ARMOUR, U_TUNE, U_INFERNO, U_COUNT, U_REPAIR = 100 };
struct Surv {
    double t = 0, hp = 100, maxHp = 100;
    int xp = 0, lvl = 1, next = 0, pendingLv = 0, kills = 0;
    std::vector<Enemy> enemies;
    std::vector<Gem> gems;
    std::vector<Fire> fire;
    std::vector<Oil> oil;
    std::vector<Splat> splats;
    std::vector<Zap> zaps;
    std::array<int, U_COUNT> up{};
    double spawnAcc = 0;
    int hordeMin = 1;
    bool boss = false;
    double teslaT = 1, oilT = 2, fireT = 0, flameT = 0, hurtT = 0, sfxT = 0, gemT = 0;
};

struct Popup { std::string text; Color color; float t = 0, alpha = 0; };

class Game {
public:
    bool Init(bool desktopQuit);
    void Shutdown();
    void Frame(float dt);   // input, simulation and drawing for one frame
    bool WantsQuit() const { return quit; }

    // ---- shared with render.cpp / menus.cpp / survival.cpp ----
    Audio audio;
    Ui ui;
    Miami miami;
    EndlessCity city;
    bool endless = false;   // survival plays in the endless city, free drive in Miami
    Mode mode = MODE_CITY;
    State state = ST_MENU;
    std::unique_ptr<Track> T;
    const Theme* theme = nullptr;
    std::vector<Scen> decos;
    std::unique_ptr<Run> run;
    std::unique_ptr<Surv> surv;
    Car car;
    struct { double x = 0, y = 0, z = 1; } cam;
    Chain chain;
    int score = 0, best = 0;
    std::vector<SmokePuff> smoke;
    std::vector<SkidMark> skids;
    bool hasPrevWheels = false;
    V2 prevWheels[2];
    double shake = 0;
    struct { double acc = 1, top = 1, smoke = 1, smokeLife = 1; } perf;   // survival upgrades tweak these
    Paint paint;
    uint32_t paintHex = 0xf0631a;
    ParkMiller rnd;
    float W = 1280, H = 720, S = 1;    // screen size in pixels, and UI scale (CSS px -> pixels)
    double now = 0;                    // seconds since start, for animation

    // input
    struct { bool up, down, left, right, hand; } keys{};
    float padSteer = 0, padThr = 0, padBrk = 0;
    bool padHand = false;
    double ThrottleIn() const { return std::max(keys.up ? 1.0 : 0.0, (double)padThr); }
    double BrakeIn() const { return std::max(keys.down ? 1.0 : 0.0, (double)padBrk); }
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
    bool rGood = false, rNext = false, rSurv = false;
    int rStars = 0;             // stage stars; survival shows three when won
    bool rStarsShown = true;
    // level-up
    std::vector<int> upOpts;
    double upLock = 0;
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
    int SurvBestT() const { return save.GetInt("surv_t", 0); }
    int SurvBestKills() const { return save.GetInt("surv_kills", 0); }

    // game flow
    void ShowMenu(View v = V_MAIN);
    void StartStage(int i);
    void StartCity();
    void StartSurvival();
    void EndSurvivalView();
    void Pause();
    void Resume();
    void ResetCar();
    void Wreck();
    void Bank();
    void Msg(const std::string& t, Color c);
    void Toast(const std::string& t, Color c);
    void ChoosePaint(uint32_t hex);
    void ChooseUp(int k);

    // tracks
    struct Nearest { int i; double d, px, py; };
    Nearest NearestOn(double x, double y, int i0, int i1) const;
    Nearest NearestAll(double x, double y) const { return NearestOn(x, y, 0, T->N - 1); }
    V2 GhostAt(double t, double& a, bool& ok) const;

    // world queries
    int TileAt(double x, double y) { return endless ? city.SurfaceAt(x, y) : miami.TileAt(x, y); }
    int SurfAt(double x, double y);
    template <class F> void ForSolidsNear(double x, double y, double r, F fn) {
        if (endless) city.ForSolidsNear(x, y, r, fn); else miami.ForSolidsNear(x, y, r, fn);
    }
    bool CollideCircle(double cx, double cy, double r, Hit& h);

    // render.cpp
    void Render(double speed);
    void DrawCar(double x, double y, double a, double w, bool braking, const Paint& p, float alpha = 1);
    void DrawBlock(const Scen& b);
    void DrawBuildingPoly(const Scen& b);
    void DrawPalm(const Scen& p);
    void DrawUmb(const Scen& u);
    void DrawGarageCar(Rectangle r);
    void DrawTrackPreview(int i, Rectangle r);
    void BuildMini();
    void DrawMini(Rectangle r);

    // menus.cpp
    void DrawMenus();
    void DrawHud(double speed);
    void DrawOverlays();

    // survival.cpp
    void SurvUpdate(double dt);
    void SurvDrawGround(double hw, double hh);
    void SurvDrawFx();
    void SurvOverlay();
    void OpenLevelUp();
    void SurvEnd(bool won);

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
    Scen MakeDeco(double x, double y);

    // pad edges
    struct { bool a, b, y, start, up, down, left, right; } padPrev{};
    bool trigSeen[2] = {false, false};

    // minimap backgrounds
    RenderTexture2D miniBg{}, radarBg{};
    bool miniOk = false;
    struct { double s = 1, ox = 0, oy = 0; } mm;
    long long radarKey = LLONG_MIN;
    double radarO[2] = {0, 0};
    void BuildRadar(int ci, int cj);
    friend class Ui;
};

const char* UpName(int k);
std::string UpDesc(int k, int l);   // what level l of upgrade k does
std::string FmtNum(long long v);    // 12,345
std::string FmtT(double t);         // m:ss, rounded up
extern const Color INK, CYAN, MUTED, ACCENT_C, BAD;
