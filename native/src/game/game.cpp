#include "game.h"
#include "terrain.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>

// The interface palette: warm cream text, pale sand and sun-gold accents, soft coral for bad news.
const Color INK = Hex(0xfbf4ea), SAND_C = Hex(0xf1d3a1), MUTED = Hex(0xe6d6c2), ACCENT_C = Hex(0xf4b56a), BAD = Hex(0xe9806e), GOOD = Hex(0xa9d18e);

namespace {
const double GRIP[7] = {1, 0.9, 0.55, 1, 0.5, 0.5, 0.5};   // by Surface
bool Soft(int s) { return s == GRASS || s == GRAVEL || s == SAND || s == WATER; }
constexpr size_t MAX_SKIDS = 3000;
constexpr double GHOST_DT = 0.05;
double Frand() { return (double)std::rand() / ((double)RAND_MAX + 1); }
}  // namespace

const PaintPreset PAINTS[8] = {
    {"Candy orange", 0xf0631a}, {"Racing red", 0xc4221f}, {"Sunburst yellow", 0xf2c21b}, {"Lime green", 0x74bf1e},
    {"Teal metallic", 0x1f7392}, {"Electric blue", 0x1f5fd6}, {"Midnight black", 0x1d2026}, {"Pearl white", 0xe9e6df}};

Paint MakePaint(uint32_t hex) {
    Color c = Hex(hex), Wt = WHITE, K = BLACK;
    float lum = (0.299f * c.r + 0.587f * c.g + 0.114f * c.b) / 255;
    return {hex, Mix(c, Wt, .28f), c, Mix(c, K, .38f), Mix(c, Wt, .1f), Mix(c, K, .22f), Mix(c, K, .6f), lum > 0.62f ? Hex(0x2a2d33) : Hex(0xf1ecdf)};
}

std::string FmtNum(long long v) {
    std::string s = std::to_string(v < 0 ? -v : v), out;
    for (size_t i = 0; i < s.size(); i++) { if (i && (s.size() - i) % 3 == 0) out += ','; out += s[i]; }
    return (v < 0 ? "-" : "") + out;
}
std::string FmtT(double t) {
    int s = std::max(0, (int)std::ceil(t));
    char b[16];
    std::snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60);
    return b;
}

// ---------- setup ----------
bool Game::Init(bool quitButton) {
    desktopQuit = quitButton;
    save.Load("save.cfg");
    best = save.GetInt("best", 0);
    audio.Init(std::clamp(save.GetInt("vol", 6), 0, 10), save.GetBool("muted", false));
    unsigned hex = 0;
    std::string p = save.Get("paint", "#f0631a");
    if (p.size() == 7 && p[0] == '#' && std::sscanf(p.c_str() + 1, "%6x", &hex) == 1) paintHex = hex;
    paint = MakePaint(paintHex);
    customPaint = std::none_of(std::begin(PAINTS), std::end(PAINTS), [&](const PaintPreset& q) { return q.hex == paintHex; });
    Vector3 hsv = ColorToHSV(Hex(paintHex));
    customH = hsv.x; customS = hsv.y; customV = hsv.z;
    miniBg = LoadRenderTexture(280, 280);
    miniOk = miniBg.id != 0;
    car = {0, 0, -PI / 2, 0, 0, 0};
    cam.x = car.x; cam.y = car.y;
    ShowMenu();
    return true;
}

void Game::Shutdown() {
    if (miniOk) UnloadRenderTexture(miniBg);
    desert.Unload();
    audio.Shutdown();
}

Game::StageProg Game::Prog(int i) const {
    const std::string k = "stage." + STAGES[i].id;   // saved by stage name, so reordering stages keeps progress
    return {save.GetInt(k + ".best", 0), save.GetInt(k + ".stars", 0)};
}
// Ghost file: magic, the stage file's fingerprint, score, sample count, then the samples.
// A ghost recorded on a different version of the stage is ignored.
constexpr int32_t GHOST_MAGIC = 0x32534847;   // "GHS2"
static std::string GhostPath(int i) { return "ghosts/" + STAGES[i].id + ".ghost"; }
bool Game::GhostExists(int i) const { Ghost g; return LoadGhost(i, g); }
bool Game::LoadGhost(int i, Ghost& g) const {
    std::ifstream f(GhostPath(i), std::ios::binary);
    int32_t hdr[4];
    if (!f.read((char*)hdr, sizeof hdr) || hdr[0] != GHOST_MAGIC || (uint32_t)hdr[1] != STAGES[i].fingerprint || hdr[3] < 0 || hdr[3] > 10000000) return false;
    std::vector<int32_t> d(hdr[3]);
    if (!f.read((char*)d.data(), d.size() * sizeof(int32_t))) return false;
    g.score = hdr[2];
    g.d.assign(d.begin(), d.end());
    return true;
}
bool Game::SaveGhost(int i, const Ghost& g) const {
    std::error_code ec;
    std::filesystem::create_directories("ghosts", ec);
    std::ofstream f(GhostPath(i), std::ios::binary);
    int32_t hdr[4] = {GHOST_MAGIC, (int32_t)STAGES[i].fingerprint, g.score, (int32_t)g.d.size()};
    std::vector<int32_t> d(g.d.begin(), g.d.end());
    return f.write((char*)hdr, sizeof hdr) && f.write((char*)d.data(), d.size() * sizeof(int32_t));
}

// ---------- world queries ----------
Game::Nearest Game::NearestOn(double x, double y, int i0, int i1) const {
    double bd = 1e18;
    int bi = 0;
    for (int j = i0; j <= i1; j++) {
        int i = ((j % T->N) + T->N) % T->N;
        V2 p = T->pts[i];
        double d = (p.x - x) * (p.x - x) + (p.y - y) * (p.y - y);
        if (d < bd) { bd = d; bi = i; }
    }
    V2 a = T->pts[bi];
    double best = std::sqrt(bd), px = a.x, py = a.y;
    for (V2 q : {T->pts[(bi + 1) % T->N], T->pts[(bi - 1 + T->N) % T->N]}) {
        double dx = q.x - a.x, dy = q.y - a.y;
        double u = std::max(0.0, std::min(1.0, ((x - a.x) * dx + (y - a.y) * dy) / (dx * dx + dy * dy)));
        double qx = a.x + dx * u, qy = a.y + dy * u, d = Hypot(x - qx, y - qy);
        if (d < best) { best = d; px = qx; py = qy; }
    }
    return {bi, best, px, py};
}

int Game::SurfAt(double x, double y) { return NearestAll(x, y).d < T->half + 7 ? ROAD : GRAVEL; }

bool Game::CollideCircle(double cx, double cy, double r, Hit& h) {
    if (mode == MODE_TRACK) {
        Nearest n = NearestAll(cx, cy);
        if (n.d + r <= T->edge) return false;
        double d = n.d ? n.d : 0.001;
        h = {(n.px - cx) / d, (n.py - cy) / d, n.d + r - T->edge};
        return true;
    }
    bool hit = false;   // free roam: lakes, rocks, ruins and arch legs; the desert itself never ends
    world.ForSolidsNear(cx, cy, r, [&](const Solid& b) { hit = CircleHit(b, cx, cy, r, h); return hit; });
    return hit;
}

double Game::SteerInput() const {
    double digital = (keys.left ? -1 : 0) + (keys.right ? 1 : 0);
    if (digital) return digital;
    return padSteer;
}

// ---------- car ----------
void Game::ResetCar() {
    if (mode == MODE_TRACK) {
        int i = run ? run->pi : 3;
        V2 p = T->pts[i], t = T->tan[i];
        car = {p.x, p.y, std::atan2(t.y, t.x), 0, 0, 0};
    } else {   // free roam: stop where you are, nudged clear of anything solid and out of the water
        const double a = car.a;
        V2 spot{car.x, car.y};
        Hit h;
        auto clear = [&](V2 p) { return world.GroundAt(p.x, p.y).water == 0 && !CollideCircle(p.x + std::cos(a) * 13, p.y + std::sin(a) * 13, 30, h) && !CollideCircle(p.x - std::cos(a) * 13, p.y - std::sin(a) * 13, 30, h); };
        for (double rr = 0; rr <= 600 && !clear(spot); rr += 40)
            for (int k = 0; k < 12; k++) { spot = {car.x + std::cos(k * PI / 6) * rr, car.y + std::sin(k * PI / 6) * rr}; if (clear(spot)) break; }
        car = {spot.x, spot.y, a, 0, 0, 0};
    }
    hasPrevWheels = false;
}

// ---------- scoring ----------
void Game::Toast(const std::string& t, Color c) { toast = {t, c, 1.3f, 1}; }
void Game::Msg(const std::string& t, Color c) { zmsg = {t, c, 1.4f, 1}; }

void Game::Bank() {
    long long p = JsRound(chain.pts);
    if (p > 30) {
        score += (int)p;
        Toast("+" + FmtNum(p), ACCENT_C);
        if (p > 1500) audio.Notes({523, 659, 784, 1047}, SQUARE, 0.08f, 0.1f);
        else if (p > 500) audio.Notes({523, 659, 784}, SQUARE, 0.08f, 0.1f);
        else audio.Notes({587, 784}, SQUARE, 0.08f, 0.1f);
        if (mode == MODE_FREE && p > best) { best = (int)p; save.SetInt("best", best); Save(); }
    }
    chain.active = false; chain.pts = 0; chain.time = 0; chain.mult = 1;
}

void Game::Wreck() {
    if (chain.active && chain.pts > 30) { Toast("Chain lost", BAD); audio.Notes({330, 247, 185}, SAWTOOTH, 0.09f, 0.08f); }
    chain.active = false; chain.pts = 0; chain.time = 0; chain.mult = 1;
    if (mode == MODE_TRACK && run && state == ST_RACE) {
        score = std::max(0, score - 100);
        Msg("Wall  −100", BAD);
        if (run->zi >= 0) run->zs[run->zi].dirty = true;
    }
}

// ---------- stage logic: laps, drift zones, clipping points ----------
void Game::NewLapZones() {
    run->zs.assign(T->zones.size(), ZoneState{});
    run->clipHit.clear();
    for (auto& z : T->zones) run->clipHit.push_back(std::vector<bool>(z.clips.size(), false));
}

void Game::JudgeZone(int k) {
    ZoneState& s = run->zs[k];
    const Zone& z = T->zones[k];
    if (s.done) return;
    s.done = true;
    if (s.dist < z.len * 0.5) return;
    if (!s.dirty && s.drift / s.dist >= 0.75) {
        score += 500;
        Msg("Clean zone  +500", ACCENT_C);
        audio.Notes({659, 988}, TRIANGLE, 0.07f, 0.12f);
    } else Msg(s.dirty ? "Zone failed" : "Not sideways enough", BAD);
}

void Game::TrackTick(double dt, bool drifting, int surf) {
    const int prev = run->pi;
    Nearest n = NearestOn(car.x, car.y, prev - 30, prev + 30);
    if (n.d > T->edge + 40) n = NearestAll(car.x, car.y);
    run->pi = n.i;
    int q = (int)std::floor(run->pi / (T->N / 4.0));
    if (q >= 1 && q <= 3) run->flags |= 1 << q;
    V2 t = T->tan[run->pi];
    double along = car.vx * t.x + car.vy * t.y, sp = Hypot(car.vx, car.vy);
    run->wrongT = (sp > 120 && along < -0.5 * sp) ? run->wrongT + dt : 0;
    int zi = -1;
    for (int k = 0; k < (int)T->zones.size(); k++) if (run->pi >= T->zones[k].a && run->pi <= T->zones[k].b) zi = k;
    if (run->zi >= 0 && zi != run->zi && run->pi > T->zones[run->zi].b) JudgeZone(run->zi);
    run->zi = zi;
    if (zi >= 0 && !run->zs[zi].done) {
        ZoneState& s = run->zs[zi];
        double ds = std::max(0.0, along) * dt;
        s.dist += ds;
        if (drifting) s.drift += ds;
        if (surf == GRAVEL) s.dirty = true;
        double rx = car.x - std::cos(car.a) * 16, ry = car.y - std::sin(car.a) * 16;
        const auto& clips = T->zones[zi].clips;
        for (size_t c = 0; c < clips.size(); c++) {
            if (!run->clipHit[zi][c] && drifting && Hypot(clips[c].x - rx, clips[c].y - ry) < T->clipR) {
                run->clipHit[zi][c] = true;
                int pts = 150 * chain.mult;
                chain.pts += pts;
                Msg("Clip  +" + std::to_string(pts), GOOD);
                audio.Notes({988, 1319}, SQUARE, 0.06f, 0.09f);
            }
        }
    }
    if (prev > T->N * 0.85 && run->pi < T->N * 0.15 && run->flags == 14) {
        run->lap++; run->flags = 0; NewLapZones();
        if (run->lap >= run->def->laps) Finish(true);
        else { Msg("Lap " + std::to_string(run->lap + 1) + " of " + std::to_string(run->def->laps), INK); audio.Notes({784}, TRIANGLE, 0.1f, 0.1f); }
    }
}

// ---------- update ----------
double Game::Update(double dt) {
    int surf;
    double g, drag;
    const double wasWater = water;
    water = 0;
    if (mode == MODE_TRACK) {
        surf = SurfAt(car.x, car.y);
        g = GRIP[surf] * T->def->grip * SaltGrip(*T->def, car.x, car.y);
        drag = Soft(surf) ? 1.2 : 0.0;
    } else {   // free roam: packed sand, soft dune crests, salt and rock
        const Ground gr = world.GroundAt(car.x, car.y);
        surf = gr.water > 0 ? WATER : gr.soft ? SAND : ROAD;
        g = gr.grip; drag = gr.drag; water = gr.water;
        world.Evict(car.x, car.y);
    }
    const double thr = ThrottleIn(), brk = BrakeIn();
    CarInput in;
    in.throttle = thr; in.brake = brk; in.steer = SteerInput(); in.handbrake = HandbrakeIn();
    in.digitalSteer = keys.left || keys.right;
    StepCar(car, in, CarSurface{g, drag}, 1, 1, dt, handling);   // car.cpp: the handling model

    const double fx = std::cos(car.a), fy = std::sin(car.a);
    const double vf = car.vx * fx + car.vy * fy, vr = -car.vx * fy + car.vy * fx;
    const double speed = Hypot(vf, vr);

    // collisions: two circles along the body
    for (double off : {13.0, -13.0}) {
        double nfx = std::cos(car.a), nfy = std::sin(car.a);
        Hit h;
        if (CollideCircle(car.x + nfx * off, car.y + nfy * off, 12, h)) {
            car.x += h.nx * h.pen; car.y += h.ny * h.pen;
            double vn = car.vx * h.nx + car.vy * h.ny;
            if (vn < 0) {
                car.vx -= 1.35 * vn * h.nx; car.vy -= 1.35 * vn * h.ny;
                car.vx *= 0.8; car.vy *= 0.8; car.w *= 0.5;
                if (-vn > 170) { shake = std::min(14.0, -vn / 25); audio.Crash((float)(-vn / 500)); Wreck(); }
                else if (-vn > 60) audio.Crash((float)(-vn / 900));
            }
        }
    }

    // drift detection
    const double ang = std::atan2(std::fabs(vr), std::fabs(vf));
    const bool drifting = speed > 170 && ang > 0.26 && ang < 1.95 && vf > -40;
    if (drifting) {
        if (!chain.active) { chain.active = true; chain.pts = 0; chain.time = 0; }
        chain.time += dt;
        chain.mult = std::min(8, 1 + (int)std::floor(chain.time / 1.5));
        chain.pts += ang * speed * dt * 0.1 * chain.mult * ((mode == MODE_TRACK && run && run->zi < 0) ? 0.25 : 1);
        chain.grace = 0.7;
    } else if (chain.active) { chain.grace -= dt; if (chain.grace <= 0) Bank(); }
    if (mode == MODE_TRACK && state == ST_RACE) TrackTick(dt, drifting, surf);

    // skids and smoke
    const bool skidding = std::fabs(vr) > 110 || (HandbrakeIn() > 0.5 && speed > 90) || (thr > 0.7 && std::fabs(vf) < 120 && std::fabs(vf) > 5 && !Soft(surf));
    const double nfx = std::cos(car.a), nfy = std::sin(car.a);
    const double bx = car.x - nfx * 14, by = car.y - nfy * 14;
    const V2 wl{bx - nfy * 9, by + nfx * 9}, wr{bx + nfy * 9, by - nfx * 9};
    if (skidding && !Soft(surf)) {
        if (hasPrevWheels) {
            skids.push_back({(float)prevWheels[0].x, (float)prevWheels[0].y, (float)wl.x, (float)wl.y});
            skids.push_back({(float)prevWheels[1].x, (float)prevWheels[1].y, (float)wr.x, (float)wr.y});
            if (skids.size() > MAX_SKIDS) skids.erase(skids.begin(), skids.begin() + (skids.size() - MAX_SKIDS));
        }
        prevWheels[0] = wl; prevWheels[1] = wr; hasPrevWheels = true;
        if (Frand() < 0.6) {
            double sx = (Frand() < .5 ? wl : wr).x, sy = (Frand() < .5 ? wl : wr).y;
            double svx = (Frand() - .5) * 30, svy = (Frand() - .5) * 30;
            smoke.push_back({sx, sy, svx, svy, 6 + Frand() * 6, 1});
        }
    } else hasPrevWheels = false;
    // wading: a splash on the way in, spray off the wheels and rings spreading behind
    if (water > 0) {
        if (wasWater == 0 && speed > 120) {
            audio.Burst(0, 0.5f, LOWPASS, 2600, 500, 0.8f, (float)std::min(1.0, speed / 400) * 0.6f);
            for (int k = 0; k < 18; k++) {
                const double a = Frand() * 2 * PI, v = 40 + Frand() * speed * 0.35;
                smoke.push_back({car.x + std::cos(a) * 14, car.y + std::sin(a) * 14, car.vx * 0.3 + std::cos(a) * v, car.vy * 0.3 + std::sin(a) * v, 4 + Frand() * 5, 1, 1});
            }
        }
        const double churn = std::min(1.0, (speed + std::fabs(vr)) / 350);
        if (Frand() < churn * 0.9)
            for (const V2& w : {wl, wr}) {
                const double side = (&w == &wl) ? 1 : -1, v = 30 + Frand() * 60 * churn;
                smoke.push_back({w.x, w.y, -nfy * side * v - car.vx * 0.15, nfx * side * v - car.vy * 0.15, 3 + Frand() * 4, 0.8, 1});
            }
        wakeT -= dt;
        if (wakeT <= 0 && speed > 30) { wakeT = 0.12; smoke.push_back({bx, by, 0, 0, 10, 1, 2}); }
    }
    for (auto& s : smoke) {
        if (s.kind == 2) { s.life -= dt * 0.7; s.r += dt * 45; continue; }   // a ring widening on the water
        s.life -= dt * (s.kind ? 2.2 : 1.4); s.r += dt * (s.kind ? 8 : 22); s.x += s.vx * dt; s.y += s.vy * dt;
        if (s.kind) { s.vx *= 1 - 3 * dt; s.vy *= 1 - 3 * dt; }
    }
    smoke.erase(std::remove_if(smoke.begin(), smoke.end(), [](const SmokePuff& s) { return s.life <= 0; }), smoke.end());

    shake = std::max(0.0, shake - dt * 30);
    // the engine's gearing is laid out for 640 px/s, so scale the car's speed to that
    audio.UpdateEngine((float)dt, (float)(std::fabs(vf) * 640 / handling.topSpeed), (float)(std::fabs(vr) + (speed > 90 ? 120 * HandbrakeIn() : 0)), Soft(surf), (float)thr);
    return speed;
}

// ---------- ghost: replay of the best completed run on each stage ----------
void Game::RecordGhost() {   // samples are flat [x, y, angle*1000, ...] every GHOST_DT seconds of race time
    auto& r = run->rec;
    while (r.size() / 3 * GHOST_DT <= run->t) {
        r.push_back((int)JsRound(car.x)); r.push_back((int)JsRound(car.y)); r.push_back((int)JsRound(car.a * 1000));
    }
}
V2 Game::GhostAt(double t, double& a, bool& ok) const {
    ok = false;
    if (!run || !run->hasGhost) return {};
    const auto& d = run->ghost.d;
    int n = (int)d.size() / 3;
    double f = std::max(0.0, t) / GHOST_DT;
    int k = (int)std::floor(f);
    if (k >= n - 1) return {};
    double u = f - k;
    int j = k * 3;
    double a0 = d[j + 2] / 1000.0, da = d[j + 5] / 1000.0 - a0;
    da = std::atan2(std::sin(da), std::cos(da));
    a = a0 + da * u;
    ok = true;
    return {d[j] + (d[j + 3] - d[j]) * u, d[j + 1] + (d[j + 4] - d[j + 1]) * u};
}

// ---------- stages ----------

void Game::LoadStage(int i) {
    const StageDef& def = STAGES[i];
    mode = MODE_TRACK;
    T = std::make_unique<Track>(BuildTrack(def));
    StageGoals(*T, i);
    desert.Load(*T, def);   // bakes the dunes and places the scenery for the stage's time of day
    BuildMini();
}

void Game::ClearFx() {
    skids.clear(); smoke.clear();
    chain = Chain{};
    hasPrevWheels = false; score = 0;
    toast.alpha = toast.t = 0; zmsg.alpha = zmsg.t = 0;
}

void Game::HideOverlays() { resultOn = false; audio.SetMusic(false); }

void Game::StartStage(int i) {
    HideOverlays();
    LoadStage(i);
    ClearFx();
    const StageDef& def = STAGES[i];
    run = std::make_unique<Run>();
    run->i = i; run->def = &def;
    run->hasGhost = LoadGhost(i, run->ghost);
    NewLapZones();
    ResetCar();
    cam.x = car.x; cam.y = car.y;
    audio.ResetEngine();
    state = ST_COUNT; countT = 3.99; lastBeep = 0; goT = 0;
    bannerN = "";
    bannerT = "Stage " + std::to_string(i + 1) + ": " + def.name;
    bannerD = def.desc + " Target " + FmtNum(T->pass) + " in " + std::to_string(def.laps) + " laps, " + FmtT(T->limit) + " on the clock." +
              (run->hasGhost ? " Racing your ghost (" + FmtNum(run->ghost.score) + ")." : "");
    bannerOn = true;
}

void Game::StartFree() {
    HideOverlays();
    mode = MODE_FREE; T.reset(); run.reset();
    ClearFx();
    desert.LoadFree(false);
    car = {0, 0, -PI / 2, 0, 0, 0};   // the start of the desert is always clear
    hasPrevWheels = false;
    cam.x = car.x; cam.y = car.y;
    state = ST_FREE; bannerOn = false;
}

void Game::ShowMenu(View v) {
    if (mode == MODE_TRACK) {
        mode = MODE_FREE; T.reset(); run.reset();
        ClearFx();
        car = {0, 0, -PI / 2, 0, 0, 0};
        cam.x = car.x; cam.y = car.y;
    }
    desert.LoadFree(true);   // the menu cruises the desert at dusk
    state = ST_MENU; audio.Silence();
    bannerOn = false; resultOn = false;
    audio.SetMusic(true);
    view = v;
    ui.ResetFocus();
}

void Game::Finish(bool completed) {
    if (chain.active) Bank();
    state = ST_DONE; audio.Silence();
    const int i = run->i, pass = T->pass;
    const StageDef& def = STAGES[i];
    const int stars = !completed ? 0 : score >= pass * 2 ? 3 : score >= pass * 1.45 ? 2 : score >= pass ? 1 : 0;
    StageProg old = Prog(i);
    StageProg now{std::max(old.best, stars ? score : 0), std::max(old.stars, stars)};
    save.SetInt("stage." + def.id + ".best", now.best);
    save.SetInt("stage." + def.id + ".stars", now.stars);
    Save();
    rTitle = !completed ? "Out of time" : stars ? "Stage clear" : "Not enough points";
    rGood = stars > 0; rStars = stars;
    rScore = "Score " + FmtNum(score) + " of " + FmtNum(pass) + " needed. Two stars at " + FmtNum(JsRound(pass * 1.45)) + ", three at " + FmtNum(pass * 2) + ".";
    std::string ghostMsg;
    if (completed && score > 0 && (!run->hasGhost || score > run->ghost.score)) {
        RecordGhost();
        Ghost g{score, run->rec};
        ghostMsg = SaveGhost(i, g) ? " New ghost saved." : "";
    }
    rBest = "Your best here: " + (now.best ? FmtNum(now.best) : std::string("none yet")) + "." + ghostMsg;
    rNext = stars > 0 && i < (int)STAGES.size() - 1;
    resultDelay = 0.7;
    ui.ResetFocus();
    if (stars) audio.Notes({523, 659, 784, 1047}, SQUARE, 0.12f, 0.1f);
    else audio.Notes({392, 330, 262}, SAWTOOTH, 0.12f, 0.1f);
}

void Game::Pause() {
    if (state != ST_RACE && state != ST_FREE && state != ST_COUNT) return;
    pausedFrom = state; state = ST_PAUSE;
    audio.Silence();
    ui.ResetFocus();
}
void Game::Resume() {
    if (state != ST_PAUSE) return;
    state = pausedFrom;
}

void Game::ChoosePaint(uint32_t hex) {
    paintHex = hex & 0xffffff;
    paint = MakePaint(paintHex);
    char b[16];
    std::snprintf(b, sizeof b, "#%06x", paintHex);
    save.Set("paint", b);
    Save();
}

// ---------- input ----------
void Game::PollPad(float dt) {
    const int gp = 0;
    if (!IsGamepadAvailable(gp)) { padSteer = padThr = padBrk = padHand = 0; ui.SetPadNav(0, false, false); return; }
    auto btn = [&](int b) { return IsGamepadButtonDown(gp, b); };
    // triggers rest at -1; some drivers read 0 until first touched, so wait until we have seen the rest position
    auto pedal = [&](int axis, int k) {
        float v = GetGamepadAxisMovement(gp, axis);
        if (v < -0.5f) trigSeen[k] = true;
        if (!trigSeen[k]) return 0.0f;
        float x = (v + 1) / 2, dz = 0.04f;
        return x < dz ? 0.0f : std::min(1.0f, (x - dz) / (0.97f - dz));   // small dead zone, full at the stop
    };
    const float ax = GetGamepadAxisMovement(gp, GAMEPAD_AXIS_LEFT_X), ay = GetGamepadAxisMovement(gp, GAMEPAD_AXIS_LEFT_Y);
    padThr = pedal(GAMEPAD_AXIS_RIGHT_TRIGGER, 1);
    // R2 gas and L2 brake are analog, like pedals; X (A on Xbox) or R1 is the handbrake
    padBrk = pedal(GAMEPAD_AXIS_LEFT_TRIGGER, 0);
    padHand = btn(GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || btn(GAMEPAD_BUTTON_RIGHT_TRIGGER_1) ? 1.0f : 0.0f;
    const bool dl = btn(GAMEPAD_BUTTON_LEFT_FACE_LEFT), dr = btn(GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
    const float steer = dl ? -1 : dr ? 1 : ax, a = std::fabs(steer), dz = 0.15f;
    padSteer = a < dz ? 0 : (float)(Sign(steer) * std::pow((a - dz) / (1 - dz), 1.4));

    const bool bA = btn(GAMEPAD_BUTTON_RIGHT_FACE_DOWN), bB = btn(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT), bY = btn(GAMEPAD_BUTTON_RIGHT_FACE_UP);
    const bool bStart = btn(GAMEPAD_BUTTON_MIDDLE_RIGHT);
    int nav = btn(GAMEPAD_BUTTON_LEFT_FACE_UP) || ay < -0.6f ? 1 : btn(GAMEPAD_BUTTON_LEFT_FACE_DOWN) || ay > 0.6f ? 2 : dl || ax < -0.6f ? 3 : dr || ax > 0.6f ? 4 : 0;
    const bool aP = bA && !padPrev.a, bP = bB && !padPrev.b, yP = bY && !padPrev.y, sP = bStart && !padPrev.start;
    const bool menu = state == ST_MENU || state == ST_PAUSE || (state == ST_DONE && resultOn);
    if (menu) {
        ui.SetPadNav(nav, aP, true);
        if (bP || (state == ST_PAUSE && sP)) {
            if (state == ST_PAUSE) Resume();
            else if (state == ST_DONE) ShowMenu();
            else if (state == ST_MENU && view != V_MAIN) { audio.Ui(UI_BACK); view = V_MAIN; ui.ResetFocus(); }
        }
    } else {
        ui.SetPadNav(0, false, false);
        if (sP) Pause();
        if (yP && (state == ST_RACE || state == ST_FREE)) { Wreck(); ResetCar(); }
    }
    padPrev = {bA, bB, bY, bStart};
    (void)dt;
}

void Game::HandleInput(float dt) {
    PollPad(dt);
    if (debugThr) padThr = debugThr;
    if (debugSteer) padSteer = debugSteer;
    keys.up = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);
    keys.down = IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S);
    keys.left = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A);
    keys.right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
    keys.hand = IsKeyDown(KEY_SPACE) || debugHand;
    const bool driving = state == ST_RACE || state == ST_FREE || state == ST_COUNT;
    if (!driving) keys = {};

    if (IsKeyPressed(KEY_F11) || ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER))) ToggleBorderlessWindowed();
    if (IsKeyPressed(KEY_R) && (state == ST_RACE || state == ST_FREE)) { Wreck(); ResetCar(); }
    if (IsKeyPressed(KEY_F3)) showDebug = !showDebug;
    const bool esc = IsKeyPressed(KEY_ESCAPE);
    if (esc || IsKeyPressed(KEY_P)) {   // Esc and P pause while driving; Esc also steps back in menus
        if (driving) Pause();
        else if (state == ST_PAUSE) Resume();
        else if (esc && state == ST_MENU && view != V_MAIN) { audio.Ui(UI_BACK); view = V_MAIN; ui.ResetFocus(); }
        else if (esc && state == ST_DONE) ShowMenu();
    }
    if (IsKeyPressed(KEY_M)) { audio.SetMuted(!audio.Muted()); save.SetBool("muted", audio.Muted()); Save(); }
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) { audio.SetVolume(audio.Volume() + 1); save.SetInt("vol", audio.Volume()); save.SetBool("muted", audio.Muted()); Save(); }
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) { audio.SetVolume(audio.Volume() - 1); save.SetInt("vol", audio.Volume()); save.SetBool("muted", audio.Muted()); Save(); }
    // the game pauses when the window loses focus
    if (!IsWindowFocused() && driving && !debugNoFocusPause) Pause();
}

// ---------- loop ----------
void Game::Frame(float frameDt) {
    const double dt = std::min(1.0 / 30, (double)frameDt);
    now += dt;
    W = (float)GetScreenWidth(); H = (float)GetScreenHeight();
    S = std::max(0.75f, std::min(H / 1080.0f, W / 1500.0f));
    ui.BeginFrame(this, (float)dt);
    HandleInput((float)dt);

    double speed = Hypot(car.vx, car.vy);
    if (state == ST_RACE || state == ST_FREE) {
        speed = Update(dt);
        if (state == ST_RACE) { run->t += dt; RecordGhost(); if (run->t >= T->limit) Finish(false); }
    } else if (state == ST_MENU && mode == MODE_FREE) {   // the menu background: a slow cruise north
        car.a = -PI / 2; car.vx = 0; car.vy = -70; car.y += car.vy * dt;
        world.Evict(car.x, car.y);
    } else if (state == ST_COUNT) {
        countT -= dt;
        audio.UpdateEngine((float)dt, 0, 0, false, (float)ThrottleIn());
        int n = (int)std::ceil(countT);
        if (n != lastBeep && n <= 3 && n > 0) { lastBeep = n; audio.Notes({523}, SQUARE, 0.12f, 0.1f); }
        bannerN = n > 3 ? "" : std::to_string(n);
        if (countT <= 0) { state = ST_RACE; goT = 0.8; bannerN = "GO"; audio.Notes({1047}, SQUARE, 0.2f, 0.12f); }
    }
    if (goT > 0) { goT -= dt; if (goT <= 0) bannerOn = false; }
    if (state == ST_DONE && !resultOn && resultDelay > 0) { resultDelay -= dt; if (resultDelay <= 0) { resultOn = true; ui.ResetFocus(); } }
    for (Popup* p : {&toast, &zmsg}) {
        if (p->t > 0) p->t -= (float)dt;
        float target = p->t > 0 ? 1.0f : 0.0f;
        p->alpha += (target - p->alpha) * std::min(1.0f, (float)dt / 0.1f);
    }
    audio.Update((float)dt, IsWindowFocused());

    BeginDrawing();
    ClearBackground(BLACK);
    Render(speed);
    if (state != ST_MENU) DrawHud(speed);
    DrawOverlays();
    if (state == ST_MENU) DrawMenus();
    ui.EndFrame();
    EndDrawing();
}
