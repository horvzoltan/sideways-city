#include "game.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>

const Color INK = Hex(0xf3ead2), CYAN = Hex(0x2fd6ff), MUTED = Hex(0xb9b09a), ACCENT_C = Hex(0xffcf3a), BAD = Hex(0xff6a55);

namespace {
const std::map<std::string, Theme> THEMES = {
    {"grass",   {0x4d6a3b, 0xa39a7c, 0x3b3e44, 0xeeeae0, 0xc8352b, {SC_TREE}, {0x3f5a30, 0x476836, 0x35502b}}},
    {"port",    {0x77756e, 0x8f8b80, 0x3a3c41, 0xeeeae0, 0x2f6fb5, {SC_BLOCK}, {0xb5452f, 0x2f6b8f, 0xc9922c, 0x4d7d44, 0x8c8f93}, 0.7}},
    {"forest",  {0x2f4a2a, 0x8a8266, 0x383a3f, 0xeeeae0, 0xc8352b, {SC_TREE}, {0x27401f, 0x2e4b25, 0x223a1c, 0x355a2c}, 2.2}},
    {"stadium", {0x5d6068, 0x8a8a86, 0x34363b, 0xeeeae0, 0xd9a21b, {SC_TIRES, SC_BLOCK}, {0x7a3b3b, 0x3b5a7a, 0x6b6f78}}},
    {"desert",  {0xb48a5c, 0xc9a574, 0x46433f, 0xeeeae0, 0xc8352b, {SC_ROCK}, {0x8f6a45, 0xa07a50, 0x7d5c3c}}},
    {"night",   {0x1c2a1f, 0x5b574a, 0x2c2e33, 0xd8d4ca, 0xb3302a, {SC_TREE}, {0x15241a, 0x1a2d1f, 0x203626}, 1.6, true}},
    {"beach",   {0xe9d7a6, 0xd6bf88, 0x3d3f46, 0xf1ede4, 0x2bb5b0, {SC_PALM}, FRONDS, 0.7}},
    {"snow",    {0xe4ebf0, 0xc7d1da, 0x56606c, 0xf4f6f8, 0x2f6fb5, {SC_TREE}, {0x2c4a3c, 0x35574a, 0x23402f}, 1.3, false, 0.72}},   // grip: ice
    {"neon",    {0x15131f, 0x2a2838, 0x24232e, 0xff3fa4, 0x2fd6ff, {SC_BLOCK}, {0x3a2f5c, 0x23365e, 0x512a4f, 0x2b2b3d}, 0.9, true}},
};
const double GRIP[7] = {1, 0.9, 0.55, 1, 0.5, 0.5, 0.5};   // by Surface
bool Soft(int s) { return s == GRASS || s == GRAVEL || s == SAND; }
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
    miami.Generate();
    miniBg = LoadRenderTexture(280, 280);
    miniOk = miniBg.id != 0;
    ResetCar();
    cam.x = car.x; cam.y = car.y;
    BuildMini();
    ShowMenu();
    return true;
}

void Game::Shutdown() {
    if (miniOk) UnloadRenderTexture(miniBg);
    if (radarBg.id) UnloadRenderTexture(radarBg);
    audio.Shutdown();
}

Game::StageProg Game::Prog(int i) const {
    return {save.GetInt("stage" + std::to_string(i) + "_best", 0), save.GetInt("stage" + std::to_string(i) + "_stars", 0)};
}
static std::string GhostPath(int i) { return "ghosts/stage_" + std::to_string(i) + ".ghost"; }
bool Game::GhostExists(int i) const { return FileExists(GhostPath(i).c_str()); }
bool Game::LoadGhost(int i, Ghost& g) const {
    std::ifstream f(GhostPath(i), std::ios::binary);
    int32_t hdr[2];
    if (!f.read((char*)hdr, sizeof hdr) || hdr[1] < 0 || hdr[1] > 10000000) return false;
    std::vector<int32_t> d(hdr[1]);
    if (!f.read((char*)d.data(), d.size() * sizeof(int32_t))) return false;
    g.score = hdr[0];
    g.d.assign(d.begin(), d.end());
    return true;
}
bool Game::SaveGhost(int i, const Ghost& g) const {
    std::error_code ec;
    std::filesystem::create_directories("ghosts", ec);
    std::ofstream f(GhostPath(i), std::ios::binary);
    int32_t hdr[2] = {g.score, (int32_t)g.d.size()};
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

int Game::SurfAt(double x, double y) {
    if (mode == MODE_CITY) return TileAt(x, y);
    return NearestAll(x, y).d < T->half + 7 ? ROAD : GRAVEL;
}

bool Game::CollideCircle(double cx, double cy, double r, Hit& h) {
    if (mode == MODE_TRACK) {
        Nearest n = NearestAll(cx, cy);
        if (n.d + r <= T->edge) return false;
        double d = n.d ? n.d : 0.001;
        h = {(n.px - cx) / d, (n.py - cy) / d, n.d + r - T->edge};
        return true;
    }
    bool hit = false;
    ForSolidsNear(cx, cy, r, [&](const Solid& b) { hit = CircleHit(b, cx, cy, r, h); return hit; });
    if (!hit && !endless) {
        const double WW = Miami::WORLD_W, WH = Miami::WORLD_H;
        if (cx < r) { h = {1, 0, r - cx}; hit = true; }
        else if (cx > WW - r) { h = {-1, 0, cx - (WW - r)}; hit = true; }
        else if (cy < r) { h = {0, 1, r - cy}; hit = true; }
        else if (cy > WH - r) { h = {0, -1, cy - (WH - r)}; hit = true; }
    }
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
    } else if (endless) {
        RoadSpot s = city.NearestRoad(car.x, car.y);
        car = {s.x, s.y, s.a, 0, 0, 0};
    } else {
        const double B = Miami::BLOCK * Miami::TILE;
        car = {3 * B + Miami::TILE, 3 * B + Miami::TILE * 5, -PI / 2, 0, 0, 0};
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
        if (mode == MODE_CITY && p > best) { best = (int)p; save.SetInt("best", best); Save(); }
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
                Msg("Clip  +" + std::to_string(pts), Hex(0x7fe08a));
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
    const double fx = std::cos(car.a), fy = std::sin(car.a), rx = -fy, ry = fx;
    double vf = car.vx * fx + car.vy * fy, vr = car.vx * rx + car.vy * ry;
    const int surf = SurfAt(car.x, car.y);
    const double g = GRIP[surf] * ((mode == MODE_TRACK && theme->grip) ? theme->grip : 1);

    const double thr = ThrottleIn(), brk = BrakeIn();
    if (thr) vf += (vf < 0 ? 1100 : 540 * perf.acc) * thr * dt * (0.6 + 0.4 * g);
    if (brk) vf -= (vf > 20 ? 950 : 320) * brk * dt;
    vf = std::max(-220.0, std::min(640 * perf.top, vf));
    vf -= vf * (0.5 + (Soft(surf) ? 1.2 : 0)) * dt;
    if (keys.hand) vf -= Sign(vf) * std::min(std::fabs(vf), 170 * dt);
    if (!thr && !brk && std::fabs(vf) < 8) vf = 0;

    double grip = keys.hand ? 1.0 : (vf > 260 ? 7.5 - 4.5 * thr : 7.5);   // more throttle, looser rear
    grip *= g;
    vr *= std::exp(-grip * dt);

    const double speed = Hypot(vf, vr);
    const double steer = SteerInput();
    const double target = steer * 2.8 * std::min(1.0, std::fabs(vf) / 150) * (vf >= 0 ? 1 : -1) * (keys.hand ? 1.4 : 1);
    car.w += (target - car.w) * std::min(1.0, 9 * dt);
    car.a += car.w * dt;

    car.vx = fx * vf + rx * vr; car.vy = fy * vf + ry * vr;
    car.x += car.vx * dt; car.y += car.vy * dt;

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
    const bool skidding = std::fabs(vr) > 110 || (keys.hand && speed > 90) || (thr > 0.7 && std::fabs(vf) < 120 && std::fabs(vf) > 5 && !Soft(surf));
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
            smoke.push_back({sx, sy, svx, svy, (6 + Frand() * 6) * perf.smoke, 1});
        }
    } else hasPrevWheels = false;
    for (auto& s : smoke) { s.life -= dt * 1.4 / perf.smokeLife; s.r += dt * 22 * perf.smoke; s.x += s.vx * dt; s.y += s.vy * dt; }
    smoke.erase(std::remove_if(smoke.begin(), smoke.end(), [](const SmokePuff& s) { return s.life <= 0; }), smoke.end());

    shake = std::max(0.0, shake - dt * 30);
    audio.UpdateEngine((float)dt, (float)std::fabs(vf), (float)(std::fabs(vr) + (keys.hand && speed > 90 ? 120 : 0)), Soft(surf), (float)thr);
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
Scen Game::MakeDeco(double x, double y) {
    const ScenKind kind = Pick(theme->deco, rnd());
    const uint32_t col = Pick(theme->cols, rnd());
    Scen d;
    d.t = kind; d.x = d.cx = x; d.y = d.cy = y; d.col = col;
    if (kind == SC_BLOCK) {
        bool horiz = rnd() < 0.5;
        d.w = horiz ? 130 : 52; d.h = horiz ? 52 : 130;
        d.x = x - d.w / 2; d.y = y - d.h / 2;
        d.ht = 0.05 + rnd() * 0.09;
        d.vents = 0; d.s = rnd(); d.clear = 110;
    } else if (kind == SC_ROCK) { d.r = 18 + rnd() * 34; d.ht = 0.03 + rnd() * 0.03; d.clear = 70; }
    else if (kind == SC_TIRES) { d.r = 15; d.ht = 0.05; d.clear = 40; }
    else if (kind == SC_PALM) { d = MakePalm(x, y, rnd); d.clear = 60; }
    else { d.r = 24 + rnd() * 24; d.ht = 0.05 + rnd() * 0.06; d.clear = 60; }
    return d;
}

void Game::LoadStage(int i) {
    const StageDef& def = STAGES[i];
    mode = MODE_TRACK;
    T = std::make_unique<Track>(BuildTrack(def));
    theme = &THEMES.at(def.theme);
    StageGoals(*T, i);
    rnd.seed = 4242 + i * 977;
    decos.clear();
    const double x0 = T->bounds[0], y0 = T->bounds[1], x1 = T->bounds[2], y1 = T->bounds[3], m = 500;
    const long long count = JsRound((x1 - x0 + 2 * m) * (y1 - y0 + 2 * m) / 60000 * theme->dense);
    for (long long k = 0; k < count; k++) {
        double x = x0 - m + rnd() * (x1 - x0 + 2 * m);
        double y = y0 - m + rnd() * (y1 - y0 + 2 * m);
        Scen d = MakeDeco(x, y);
        if (NearestAll(x, y).d > T->edge + d.clear) decos.push_back(d);
    }
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
    EndSurvivalView();
    perf = {1, 1, 1, 1};
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

void Game::StartCity() {
    EndSurvivalView();
    perf = {1, 1, 1, 1};
    HideOverlays();
    mode = MODE_CITY; T.reset(); run.reset();
    ClearFx();
    BuildMini();
    ResetCar();
    cam.x = car.x; cam.y = car.y;
    state = ST_FREE; bannerOn = false;
}

void Game::ShowMenu(View v) {
    EndSurvivalView();
    if (mode == MODE_TRACK) {
        mode = MODE_CITY; T.reset(); run.reset();
        ClearFx(); BuildMini(); ResetCar();
        cam.x = car.x; cam.y = car.y;
    }
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
    const int stars = !completed ? 0 : score >= pass * 2 ? 3 : score >= pass * 1.45 ? 2 : score >= pass ? 1 : 0;
    StageProg old = Prog(i);
    StageProg now{std::max(old.best, stars ? score : 0), std::max(old.stars, stars)};
    save.SetInt("stage" + std::to_string(i) + "_best", now.best);
    save.SetInt("stage" + std::to_string(i) + "_stars", now.stars);
    Save();
    rTitle = !completed ? "Out of time" : stars ? "Stage clear" : "Not enough points";
    rGood = stars > 0; rStars = stars; rStarsShown = true; rSurv = false;
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
    if (!IsGamepadAvailable(gp)) { padSteer = padThr = padBrk = 0; padHand = false; ui.SetPadNav(0, false, false); return; }
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
    padBrk = pedal(GAMEPAD_AXIS_LEFT_TRIGGER, 0);
    padHand = btn(GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || btn(GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
    const bool dl = btn(GAMEPAD_BUTTON_LEFT_FACE_LEFT), dr = btn(GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
    const float steer = dl ? -1 : dr ? 1 : ax, a = std::fabs(steer), dz = 0.15f;
    padSteer = a < dz ? 0 : (float)(Sign(steer) * std::pow((a - dz) / (1 - dz), 1.4));

    const bool bA = btn(GAMEPAD_BUTTON_RIGHT_FACE_DOWN), bB = btn(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT), bY = btn(GAMEPAD_BUTTON_RIGHT_FACE_UP);
    const bool bStart = btn(GAMEPAD_BUTTON_MIDDLE_RIGHT);
    int nav = btn(GAMEPAD_BUTTON_LEFT_FACE_UP) || ay < -0.6f ? 1 : btn(GAMEPAD_BUTTON_LEFT_FACE_DOWN) || ay > 0.6f ? 2 : dl || ax < -0.6f ? 3 : dr || ax > 0.6f ? 4 : 0;
    const bool aP = bA && !padPrev.a, bP = bB && !padPrev.b, yP = bY && !padPrev.y, sP = bStart && !padPrev.start;
    const bool menu = state == ST_MENU || state == ST_PAUSE || state == ST_LEVELUP || (state == ST_DONE && resultOn);
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
    keys.hand = IsKeyDown(KEY_SPACE) || padHand || debugHand;
    const bool driving = state == ST_RACE || state == ST_FREE || state == ST_COUNT;
    if (!driving) keys = {};

    if (IsKeyPressed(KEY_F11) || ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER))) ToggleBorderlessWindowed();
    if (IsKeyPressed(KEY_R) && (state == ST_RACE || state == ST_FREE)) { Wreck(); ResetCar(); }
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
    if (state == ST_LEVELUP && upLock <= 0) {
        const int keysN[3][2] = {{KEY_ONE, KEY_KP_1}, {KEY_TWO, KEY_KP_2}, {KEY_THREE, KEY_KP_3}};
        for (int n = 0; n < 3 && n < (int)upOpts.size(); n++)
            if (IsKeyPressed(keysN[n][0]) || IsKeyPressed(keysN[n][1])) { audio.Ui(UI_SELECT); ChooseUp(upOpts[n]); break; }
    }
    // the game pauses when the window loses focus, like the web version when the tab is hidden
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
        if (surv) SurvUpdate(dt);
        if (state == ST_RACE) { run->t += dt; RecordGhost(); if (run->t >= T->limit) Finish(false); }
    } else if (state == ST_MENU && mode == MODE_CITY && !endless) {
        car.a = -PI / 2; car.vx = 0; car.vy = -70; car.y += car.vy * dt;
        if (car.y < 300) { car.y = Miami::WORLD_H - 300; cam.y = car.y; }
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
    if (upLock > 0) upLock -= dt;
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
