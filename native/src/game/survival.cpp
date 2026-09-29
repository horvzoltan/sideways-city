// survival.cpp - "Survive the night". Hordes chase the car through the endless city at night.
// Ramming, tyre smoke and upgrades kill them, and the drift combo multiplies all damage. Kills
// drop XP gems; each level-up offers 3 upgrade cards. Last SURV_LEN seconds to win. Enemies
// use a spatial hash so a few hundred stay cheap.
#include "game.h"
#include "rlgl.h"
#include <algorithm>
#include <cstdio>
#include <unordered_map>

namespace {
constexpr double SURV_LEN = 600, BOSS_T = 450, CELL = 80;
constexpr int MAXL = 5;
struct EType { double r, hp, spd, dmg; int xp; uint32_t col; };
const EType ETYPES[4] = {
    {11, 20, 78, 6, 1, 0x6f9a52},       // walker
    {9, 12, 150, 5, 1, 0xb8bf78},       // runner
    {18, 110, 52, 16, 5, 0x7b5a8c},     // brute
    {24, 1600, 125, 30, 40, 0xf4f4f2},  // boss: the police
};
int XpNeed(int l) { return 4 + l * 2 + (int)std::floor(l * l * 0.3); }
double Frand() { return (double)std::rand() / ((double)RAND_MAX + 1); }

// enemy spatial hash, rebuilt every frame
std::unordered_map<long long, std::vector<int>> ehash;
long long HKey(long long cx, long long cy) { return (cx << 32) ^ (cy & 0xffffffffLL); }
void RebuildHash(Surv& S) {
    for (auto& kv : ehash) kv.second.clear();
    for (int i = 0; i < (int)S.enemies.size(); i++) {
        const Enemy& e = S.enemies[i];
        ehash[HKey((long long)std::floor(e.x / CELL), (long long)std::floor(e.y / CELL))].push_back(i);
    }
}
template <class F> void Near(Surv& S, double x, double y, double r, F fn) {   // fn(enemy, dx, dy) for every live enemy overlapping the circle
    const long long c0 = (long long)std::floor((x - r - 30) / CELL), c1 = (long long)std::floor((x + r + 30) / CELL);
    const long long d0 = (long long)std::floor((y - r - 30) / CELL), d1 = (long long)std::floor((y + r + 30) / CELL);
    for (long long cx = c0; cx <= c1; cx++)
        for (long long cy = d0; cy <= d1; cy++) {
            auto it = ehash.find(HKey(cx, cy));
            if (it == ehash.end()) continue;
            for (int i : it->second) {
                Enemy& e = S.enemies[i];
                if (e.dead) continue;
                double dx = e.x - x, dy = e.y - y, rr = r + e.r;
                if (dx * dx + dy * dy < rr * rr) fn(e, dx, dy);
            }
        }
}
}  // namespace

const char* UpName(int k) {
    static const char* names[U_COUNT] = {"Flaming tyres", "Toxic smoke", "Spiked bumper", "Tesla coil", "Oil slick", "Afterburner", "Magnet", "Armour plating", "Engine tune", "Inferno drift"};
    return k == U_REPAIR ? "Full repair" : names[k];
}
std::string UpDesc(int k, int l) {   // describes the level you would get
    char b[160];
    switch (k) {
        case U_FIRE: std::snprintf(b, sizeof b, "Skid marks burn enemies for %d damage a second.", 14 + 8 * l); break;
        case U_TOXIC: std::snprintf(b, sizeof b, "Tyre smoke is bigger and does %d%% more damage.", 40 * l); break;
        case U_BUMPER: std::snprintf(b, sizeof b, "%d%% more ramming damage and harder knockback.", 40 * l); break;
        case U_TESLA:
            if (l > 1) std::snprintf(b, sizeof b, "Zaps the %d nearest enemies every %.1f s.", l, 1.7 - 0.2 * l);
            else std::snprintf(b, sizeof b, "Zaps the nearest enemy every %.1f s.", 1.7 - 0.2 * l);
            break;
        case U_OIL: std::snprintf(b, sizeof b, "Drops oil every %.1f s that slows enemies to a crawl.", std::max(1.2, 3.2 - 0.4 * l)); break;
        case U_BURNER: return "Exhaust flames scorch anything behind you while on the gas.";
        case U_MAGNET: std::snprintf(b, sizeof b, "Collect XP from %d px away.", 70 + 45 * l); break;
        case U_ARMOUR: std::snprintf(b, sizeof b, "+25 max health, %d%% less damage taken, repairs 25.", 8 * l); break;
        case U_TUNE: std::snprintf(b, sizeof b, "%d%% more acceleration and top speed.", 8 * l); break;
        case U_INFERNO: return "Evolution: tyre smoke sets the ground on fire and does double damage.";
        default: return "Restore all health.";
    }
    return b;
}

// ---------- spawning ----------
static bool BlockedAt(Game& g, double x, double y, double r) {
    if (g.TileAt(x, y) == WATER) return true;
    bool hit = false;
    Hit h;
    g.ForSolidsNear(x, y, r, [&](const Solid& b) { return hit = CircleHit(b, x, y, r, h); });
    return hit;
}
static bool InWorld(Game& g, double x, double y, double m) {
    return g.endless || (x > m && y > m && x < Miami::WORLD_W - m && y < Miami::WORLD_H - m);
}
static bool SpawnPoint(Game& g, V2& out) {
    const double ang = Frand() * PI * 2, R = Hypot(g.W / g.S, g.H / g.S) / 2 / g.cam.z + 60 + Frand() * 140;
    for (int k = 0; k < 10; k++) {
        double a = ang + k * 0.7, x = g.car.x + std::cos(a) * R, y = g.car.y + std::sin(a) * R;
        if (!InWorld(g, x, y, 20) || BlockedAt(g, x, y, 14)) continue;
        out = {x, y};
        return true;
    }
    return false;
}
static void SpawnEnemy(Game& g, EnemyType type, const V2* at = nullptr) {
    Surv& S = *g.surv;
    if (S.enemies.size() >= 450) return;
    V2 pos;
    if (at) pos = *at;
    else if (!SpawnPoint(g, pos)) return;
    const EType& d = ETYPES[type];
    const double hp = d.hp * (type == E_BOSS ? 1 : 1 + S.t / 300);
    Enemy e{type, pos.x, pos.y, d.r, hp, hp, d.spd * (0.88 + Frand() * 0.24), d.dmg, d.xp, d.col};
    e.sd = Frand() < .5 ? -1 : 1;
    e.wob = Frand() * 7;
    S.enemies.push_back(e);
}
static void Horde(Game& g, int n) {   // a ring of walkers closing in from every side
    const double R = Hypot(g.W / g.S, g.H / g.S) / 2 / g.cam.z + 80;
    for (int i = 0; i < n; i++) {
        double a = (double)i / n * PI * 2;
        V2 p{g.car.x + std::cos(a) * R, g.car.y + std::sin(a) * R};
        if (InWorld(g, p.x, p.y, 20) && !BlockedAt(g, p.x, p.y, 12)) SpawnEnemy(g, E_WALKER, &p);
    }
}
static void KillEnemy(Game& g, Enemy& e) {
    Surv& S = *g.surv;
    e.dead = true; S.kills++;
    g.score += 10 * g.chain.mult;
    if (g.chain.active) g.chain.pts += 4 * g.chain.mult;
    if (e.type == E_BOSS) {
        for (int i = 0; i < 6; i++) {
            double gx = e.x + (Frand() - .5) * 60, gy = e.y + (Frand() - .5) * 60;
            S.gems.push_back({gx, gy, e.xp / 6});
        }
        S.gems.push_back({e.x, e.y, 0, true});
        g.shake = 10; g.audio.Crash(1);
    } else S.gems.push_back({e.x, e.y, e.xp});
    if (e.type != E_BOSS && Frand() < 0.012) S.gems.push_back({e.x + 8, e.y + 8, 0, true});
    double sr = e.r * (1.2 + Frand() * 0.6), sa = Frand() * 7;
    S.splats.push_back({e.x, e.y, sr, sa, e.type == E_BOSS ? 0x2a2a2au : e.type == E_BRUTE ? 0x3d2a48u : 0x35502au});
    if (S.splats.size() > 260) S.splats.erase(S.splats.begin());
    if (S.sfxT <= 0 && !g.audio.Muted()) { S.sfxT = 0.05; g.audio.Burst(0, 0.09f, LOWPASS, 700, 180, 1, 0.3f); }
}
static void Damage(Game& g, Enemy& e, double amt) {
    if (e.dead) return;
    e.hp -= amt; e.flash = 0.08;
    if (e.hp <= 0) KillEnemy(g, e);
}
static void PushOutSolids(Game& g, Enemy& e) {   // walls near an enemy (Miami grid or endless-city blocks)
    g.ForSolidsNear(e.x, e.y, e.r, [&](const Solid& b) {
        Hit h;
        if (CircleHit(b, e.x, e.y, e.r, h)) { e.x += h.nx * h.pen; e.y += h.ny * h.pen; }
        return false;
    });
    if (!g.endless) { e.x = Clamp(e.x, e.r, Miami::WORLD_W - e.r); e.y = Clamp(e.y, e.r, Miami::WORLD_H - e.r); }
}

// ---------- flow ----------
void Game::StartSurvival() {
    StartCity();
    perf = {1, 1, 1, 1};
    endless = true;
    city.Clear();
    RoadSpot s = city.Start();
    car = {s.x, s.y, s.a, 0, 0, 0};
    cam.x = car.x; cam.y = car.y;
    radarKey = LLONG_MIN;
    surv = std::make_unique<Surv>();
    surv->next = XpNeed(1);
    ehash.clear();
    bannerN = "";
    bannerT = "Survive the night";
    bannerD = "They come from every side. Drift through them: tyre smoke and ramming kill, and your combo multiplies the damage. Last 10 minutes.";
    bannerOn = true; goT = 5;
}

void Game::EndSurvivalView() {
    surv.reset();
    upOpts.clear();
    if (endless) {
        endless = false;
        city.Clear();
        if (mode == MODE_CITY) { ResetCar(); cam.x = car.x; cam.y = car.y; }
    }
}

void Game::SurvUpdate(double dt) {
    Surv& S = *surv;
    const double t = (S.t += dt);
    S.sfxT -= dt; S.gemT -= dt;
    city.Evict(car.x, car.y);
    S.hurtT = std::max(0.0, S.hurtT - dt); S.flameT = std::max(0.0, S.flameT - dt);
    if (t >= SURV_LEN) { SurvEnd(true); return; }

    // waves: a steady trickle that grows, a horde every minute, the police at BOSS_T
    const double cap = std::min(320.0, 14 + t * 0.55);
    S.spawnAcc += dt * (1.2 + t / 40);
    while (S.spawnAcc >= 1) {
        S.spawnAcc--;
        if (S.enemies.size() < cap) { double r = Frand(); SpawnEnemy(*this, t > 180 && r < 0.1 ? E_BRUTE : t > 60 && r < 0.35 ? E_RUNNER : E_WALKER); }
    }
    if (t >= S.hordeMin * 60 && S.hordeMin < 10) { Horde(*this, 18 + S.hordeMin * 6); S.hordeMin++; Msg("Horde incoming", Hex(0xff6a55)); }
    if (!S.boss && t >= BOSS_T) {
        S.boss = true;
        for (int i = 0; i < 3; i++) SpawnEnemy(*this, E_BOSS);
        Msg("The police are here", Hex(0xff6a55));
        audio.Notes({660, 440, 660, 440}, SQUARE, 0.14f, 0.1f);
    }

    RebuildHash(S);
    const double fx = std::cos(car.a), fy = std::sin(car.a);
    const double dmgMult = 1 + (chain.active ? (chain.mult - 1) * 0.25 : 0);   // x8 combo = 2.75x damage

    // enemies walk at the car, slide along walls, and sidestep when they get stuck
    for (Enemy& e : S.enemies) {
        e.flash = std::max(0.0, e.flash - dt); e.hitT = std::max(0.0, e.hitT - dt);
        const double dx = car.x - e.x, dy = car.y - e.y;
        double d = Hypot(dx, dy);
        if (d == 0) d = 1;
        if (d > 1700) { V2 p; if (SpawnPoint(*this, p)) { e.x = p.x; e.y = p.y; } continue; }
        double ux = dx / d, uy = dy / d;
        if (e.stuckT > 0) { e.stuckT -= dt; double s = e.sd, ox2 = ux; ux = -uy * s; uy = ox2 * s; }
        const double v = e.spd * (e.slow > 0 ? 0.3 : 1) * dt, ox = e.x, oy = e.y;
        e.slow = std::max(0.0, e.slow - dt);
        e.x += ux * v; e.y += uy * v; e.a = std::atan2(dy, dx);
        Near(S, e.x, e.y, e.r, [&](Enemy& o, double ex, double ey) {
            if (&o == &e) return;
            double dd = Hypot(ex, ey);
            if (dd == 0) dd = 1;
            double pen = (e.r + o.r - dd) * 0.25;
            e.x -= ex / dd * pen; e.y -= ey / dd * pen;
        });
        PushOutSolids(*this, e);
        if (!(e.stuckT > 0) && v > 0.3 && Hypot(e.x - ox, e.y - oy) < v * 0.25) {
            e.stuckA += dt;
            if (e.stuckA > 0.25) { e.stuckT = 0.9; e.stuckA = 0; }
        } else e.stuckA = 0;
    }

    // the car: ramming damage, knockback, and contact damage when you are slow
    const int bumper = S.up[U_BUMPER], armour = S.up[U_ARMOUR];
    double hurt = 0;
    for (double off : {13.0, -13.0}) {
        Near(S, car.x + fx * off, car.y + fy * off, 13, [&](Enemy& e, double ex, double ey) {
            double dd = Hypot(ex, ey);
            if (dd == 0) dd = 1;
            const double nx = ex / dd, ny = ey / dd, closing = car.vx * nx + car.vy * ny;
            if (closing > 90 && e.hitT <= 0) {
                e.hitT = 0.25;
                Damage(*this, e, closing * 0.13 * (1 + 0.4 * bumper) * dmgMult);
                const double kb = (e.type == E_BOSS ? 0.1 : e.type == E_BRUTE ? 0.4 : 1) * (1 + 0.5 * bumper);
                e.x += nx * 18 * kb; e.y += ny * 18 * kb;
                const double drag = e.type == E_BOSS ? 0.55 : e.type == E_BRUTE ? 0.85 : 0.97;
                car.vx *= drag; car.vy *= drag;
                if (e.type == E_BOSS) { shake = 8; audio.Crash((float)(closing / 600)); }
            }
            const double pen = 13 + e.r - dd;
            if (pen > 0) {
                if (e.type == E_BOSS) { car.x -= nx * pen; car.y -= ny * pen; }
                else { e.x += nx * pen; e.y += ny * pen; }
            }
            if (!e.dead && closing <= 90) hurt += e.dmg;
        });
    }
    if (hurt) {
        S.hp -= hurt * dt * (1 - 0.08 * armour);
        S.hurtT = 0.2;
        if (S.hp <= 0) { S.hp = 0; SurvEnd(false); return; }
    }

    // tyre smoke: the core weapon
    const int toxic = S.up[U_TOXIC];
    const bool inferno = S.up[U_INFERNO] > 0;
    const double smokeDps = 10 * (1 + 0.4 * toxic) * (inferno ? 2 : 1) * dmgMult;
    for (const SmokePuff& s : smoke) {
        if (s.life < 0.15) continue;
        Near(S, s.x, s.y, s.r * 0.8, [&](Enemy& e, double, double) { Damage(*this, e, smokeDps * dt * s.life); });
        if (inferno && Frand() < dt * 1.5) S.fire.push_back({s.x, s.y, 1.2, 1.2});
    }

    // flaming tyres: burning patches along the skid marks
    const int fireL = S.up[U_FIRE];
    if (fireL && hasPrevWheels) {
        S.fireT -= dt;
        if (S.fireT <= 0) {
            S.fireT = 0.07;
            for (const V2& w : prevWheels) S.fire.push_back({w.x, w.y, 1.2 + 0.3 * fireL, 1.2 + 0.3 * fireL});
        }
    }
    if (S.fire.size() > 450) S.fire.erase(S.fire.begin(), S.fire.begin() + (S.fire.size() - 450));
    const double fireDps = (14 + 8 * std::max(fireL, inferno ? 3 : 0)) * dmgMult;
    for (Fire& f : S.fire) {
        f.life -= dt;
        if (f.life > 0) Near(S, f.x, f.y, 14, [&](Enemy& e, double, double) { Damage(*this, e, fireDps * dt); });
    }
    S.fire.erase(std::remove_if(S.fire.begin(), S.fire.end(), [](const Fire& f) { return f.life <= 0; }), S.fire.end());

    // oil slick
    const int oilL = S.up[U_OIL];
    if (oilL) {
        S.oilT -= dt;
        if (S.oilT <= 0) {
            S.oilT = std::max(1.2, 3.2 - 0.4 * oilL);
            S.oil.push_back({car.x - fx * 34, car.y - fy * 34, 38.0 + 8 * oilL, 5.0 + oilL, Frand() * 7});
        }
    }
    for (Oil& o : S.oil) {
        o.life -= dt;
        if (o.life > 0) Near(S, o.x, o.y, o.r, [&](Enemy& e, double, double) { e.slow = 0.25; Damage(*this, e, 3 * oilL * dt); });
    }
    S.oil.erase(std::remove_if(S.oil.begin(), S.oil.end(), [](const Oil& o) { return o.life <= 0; }), S.oil.end());

    // tesla coil
    const int tl = S.up[U_TESLA];
    if (tl) {
        S.teslaT -= dt;
        if (S.teslaT <= 0) {
            S.teslaT = 1.7 - 0.2 * tl;
            std::vector<std::pair<double, Enemy*>> tg;
            Near(S, car.x, car.y, 320, [&](Enemy& e, double ex, double ey) { tg.push_back({ex * ex + ey * ey, &e}); });
            std::stable_sort(tg.begin(), tg.end(), [](auto& a, auto& b) { return a.first < b.first; });
            for (int k = 0; k < tl && k < (int)tg.size(); k++) {
                Enemy& e = *tg[k].second;
                S.zaps.push_back({e.x, e.y, 0.18, Frand() * 99});
                Damage(*this, e, (28 + 12 * tl) * dmgMult);
            }
            if (!tg.empty() && !audio.Muted()) audio.Burst(0, 0.12f, HIGHPASS, 4000, 2000, 2, 0.18f);
        }
    }
    for (Zap& z : S.zaps) z.life -= dt;
    S.zaps.erase(std::remove_if(S.zaps.begin(), S.zaps.end(), [](const Zap& z) { return z.life <= 0; }), S.zaps.end());

    // afterburner
    const int bl = S.up[U_BURNER];
    if (bl && ThrottleIn() > 0.4) {
        const double k = 40 + 6 * bl;
        Near(S, car.x - fx * k, car.y - fy * k, 22 + 5 * bl, [&](Enemy& e, double, double) { Damage(*this, e, (25 + 15 * bl) * dt * dmgMult); });
        S.flameT = 0.1;
    }

    S.enemies.erase(std::remove_if(S.enemies.begin(), S.enemies.end(), [](const Enemy& e) { return e.dead; }), S.enemies.end());

    // XP gems and repair kits
    const double mag = 70 + 45 * S.up[U_MAGNET], sp = Hypot(car.vx, car.vy);
    for (int i = (int)S.gems.size() - 1; i >= 0; i--) {
        Gem& g = S.gems[i];
        const double dx = car.x - g.x, dy = car.y - g.y;
        double d = Hypot(dx, dy);
        if (d == 0) d = 1;
        if (g.pull || d < mag) {
            g.pull = true;
            double v = std::min(d, std::max(320.0, sp + 220) * dt);
            g.x += dx / d * v; g.y += dy / d * v;
        }
        if (d < 22) {
            Gem got = g;
            S.gems.erase(S.gems.begin() + i);
            if (got.kit) { S.hp = std::min(S.maxHp, S.hp + 30); Msg("Repaired +30", Hex(0x7fe08a)); audio.Notes({784, 1047}, TRIANGLE, 0.06f, 0.1f); }
            else {
                S.xp += got.v;
                if (S.gemT <= 0 && !audio.Muted()) { S.gemT = 0.05; audio.Notes({(float)(1320 + Frand() * 200)}, SINE, 0.03f, 0.035f); }
            }
        }
    }
    if (S.gems.size() > 500) S.gems.erase(S.gems.begin(), S.gems.begin() + (S.gems.size() - 500));
    while (S.xp >= S.next) { S.xp -= S.next; S.lvl++; S.next = XpNeed(S.lvl); S.pendingLv++; }
    if (S.pendingLv > 0) OpenLevelUp();
}

// ---------- level-up cards ----------
void Game::OpenLevelUp() {
    Surv& S = *surv;
    S.pendingLv--;
    state = ST_LEVELUP;
    audio.Silence();
    std::vector<int> pool;
    for (int k = 0; k < U_INFERNO; k++) if (S.up[k] < MAXL) pool.push_back(k);
    upOpts.clear();
    if (S.up[U_FIRE] >= MAXL && S.up[U_TOXIC] >= MAXL && !S.up[U_INFERNO]) upOpts.push_back(U_INFERNO);
    while (upOpts.size() < 3 && !pool.empty()) {
        size_t k = (size_t)std::floor(Frand() * pool.size());
        upOpts.push_back(pool[k]);
        pool.erase(pool.begin() + k);
    }
    if (upOpts.empty()) upOpts.push_back(U_REPAIR);
    upLock = 0.45;   // short lock so a held drift button or a stray tap does not pick a card by accident
    ui.ResetFocus();
    audio.Notes({523, 784, 1047}, TRIANGLE, 0.07f, 0.1f);
}

void Game::ChooseUp(int k) {
    if (!surv || state != ST_LEVELUP) return;
    Surv& S = *surv;
    if (k == U_REPAIR) S.hp = S.maxHp;
    else {
        const int l = ++S.up[k];
        if (k == U_ARMOUR) { S.maxHp += 25; S.hp = std::min(S.maxHp, S.hp + 25); }
        if (k == U_TOXIC) { perf.smoke = 1 + 0.2 * l; perf.smokeLife = 1 + 0.15 * l; }
        if (k == U_TUNE) { perf.acc = 1 + 0.08 * l; perf.top = 1 + 0.08 * l; }
        if (k == U_INFERNO) Msg("Inferno drift!", Hex(0xff7a1a));
    }
    state = ST_FREE;
    if (S.pendingLv > 0) OpenLevelUp();
}

void Game::SurvEnd(bool won) {
    Surv& S = *surv;
    if (chain.active) Bank();
    state = ST_DONE;
    audio.Silence();
    const int lasted = (int)std::floor(std::min(S.t, SURV_LEN));
    const int recT = SurvBestT(), recK = SurvBestKills();
    const bool record = lasted > recT || S.kills > recK;
    save.SetInt("surv_t", std::max(recT, lasted));
    save.SetInt("surv_kills", std::max(recK, S.kills));
    Save();
    rTitle = won ? "You survived the night" : "Wrecked";
    rGood = won; rSurv = true; rStars = 3; rStarsShown = won;
    rScore = "Survived " + FmtT(lasted) + ", " + FmtNum(S.kills) + " kills, level " + std::to_string(S.lvl) + ", score " + FmtNum(score) + ".";
    rBest = "Your best: " + FmtT(SurvBestT()) + " survived, " + FmtNum(SurvBestKills()) + " kills." + (record ? " New record!" : "");
    rNext = false;
    resultDelay = 0.9;
    ui.ResetFocus();
    if (won) audio.Notes({523, 659, 784, 1047}, SQUARE, 0.12f, 0.1f);
    else audio.Notes({392, 330, 262}, SAWTOOTH, 0.12f, 0.1f);
}

// ---------- drawing ----------
static void HpBar(double x, double y, double w, double k, Color col) {
    draw::FillRect((float)(x - w / 2 - 1), (float)(y - 1), (float)w + 2, 6, Rgba(0, 0, 0, .6f));
    draw::FillRect((float)(x - w / 2), (float)y, (float)(w * std::max(0.0, k)), 4, col);
}

void Game::SurvDrawGround(double hw, double hh) {
    Surv& S = *surv;
    auto vis = [&](double x, double y) { return std::fabs(x - cam.x) < hw + 60 && std::fabs(y - cam.y) < hh + 60; };
    for (const Splat& s : S.splats) {
        if (!vis(s.x, s.y)) continue;
        const Color c = Hex(s.col, 0.55f);
        draw::FillEllipse((float)s.x, (float)s.y, (float)s.r, (float)s.r * 0.7f, (float)s.a, c);
        draw::FillCircle((float)(s.x + std::cos(s.a) * s.r), (float)(s.y + std::sin(s.a) * s.r), (float)s.r * 0.3f, c);
    }
    for (const Oil& o : S.oil) {
        if (!vis(o.x, o.y)) continue;
        const float al = (float)std::min(1.0, o.life);
        draw::FillEllipse((float)o.x, (float)o.y, (float)o.r, (float)o.r * 0.8f, (float)o.a, Rgba(12, 12, 16, 0.8f * al));
        draw::StrokeEllipseArc((float)(o.x - o.r * 0.2), (float)(o.y - o.r * 0.15), (float)o.r * 0.5f, (float)o.r * 0.3f, (float)o.a, 0, 4, 3, Rgba(140, 90, 255, 0.25f * al));
    }
    for (const Fire& f : S.fire) {
        if (!vis(f.x, f.y)) continue;
        const double k = f.life / f.max, fl = 0.8 + std::sin(now * 30 + f.x) * 0.2;
        draw::FillCircle((float)f.x, (float)f.y, (float)(12 * fl * (0.6 + 0.4 * k)), Rgba(255, (int)(120 + 80 * k), 30, (float)(0.55 * k)));
        draw::FillCircle((float)f.x, (float)f.y, (float)(5 * fl), Rgba(255, 230, 120, (float)(0.6 * k)));
    }
    for (const Gem& g : S.gems) {
        if (!vis(g.x, g.y)) continue;
        const float x = (float)g.x, y = (float)g.y;
        if (g.kit) {
            draw::FillRect(x - 9, y - 9, 18, 18, Hex(0xf1ede4));
            draw::FillRect(x - 2.5f, y - 6, 5, 12, Hex(0xe0413a));
            draw::FillRect(x - 6, y - 2.5f, 12, 5, Hex(0xe0413a));
            continue;
        }
        const float s = g.v >= 20 ? 9.0f : g.v >= 5 ? 7.0f : 5.0f;
        const Color c = Hex(g.v >= 20 ? 0xff4f6a : g.v >= 5 ? 0x5fe07a : 0x4fb6ff);
        const Vector2 d[4] = {{x, y - s * 1.3f}, {x + s, y}, {x, y + s * 1.3f}, {x - s, y}};
        draw::FillConvex(d, 4, c);
        draw::FillRect(x - 1.5f, y - s * 0.7f, 3, s * 0.6f, Rgba(255, 255, 255, .6f));
    }
    // enemies stand on the ground, so they go under the car
    static const Paint policePaint = MakePaint(0xf4f4f2);
    for (const Enemy& e : S.enemies) {
        if (!vis(e.x, e.y)) continue;
        if (e.type == E_BOSS) {
            DrawCar(e.x, e.y, e.a, 0, false, policePaint);
            rlPushMatrix();
            rlTranslatef((float)e.x, (float)e.y, 0);
            rlRotatef((float)(e.a * RAD2DEG), 0, 0, 1);
            const bool on = (long long)std::floor(now * 6) % 2;
            const Color red = Hex(0xff2d2d), blue = Hex(0x2d6bff);
            draw::FillRect(-4, -7, 5, 6, on ? red : blue);
            draw::FillRect(-4, 1, 5, 6, on ? blue : red);
            draw::FillCircle(0, 0, 40, on ? Rgba(255, 45, 45, .25f) : Rgba(45, 107, 255, .25f));
            if (e.flash > 0) draw::FillRect(-24, -12, 48, 24, Rgba(255, 255, 255, .5f));
            rlPopMatrix();
            HpBar(e.x, e.y - 34, 56, e.hp / e.max, Hex(0xff4f4f));
            continue;
        }
        rlPushMatrix();
        rlTranslatef((float)e.x, (float)e.y, 0);
        rlRotatef((float)(e.a * RAD2DEG), 0, 0, 1);
        const float r = (float)e.r, sway = (float)std::sin(now * 8 + e.wob) * r * 0.15f;
        draw::FillCircle(3, 4, r, Rgba(0, 0, 0, .28f));
        const Color arm = Shade(e.col, 0.75);   // arms reaching out
        draw::FillRect(0, -r * 0.95f + sway, r * 1.25f, r * 0.36f, arm);
        draw::FillRect(0, r * 0.6f - sway, r * 1.25f, r * 0.36f, arm);
        draw::FillCircle(0, 0, r, e.flash > 0 ? WHITE : Hex(e.col));
        draw::FillCircle(r * 0.15f, 0, r * 0.55f, e.flash > 0 ? WHITE : Shade(e.col, 1.18));
        draw::FillRect(r * 0.45f, -r * 0.28f, r * 0.16f, r * 0.16f, Hex(0xe8323a));   // glowing eyes
        draw::FillRect(r * 0.45f, r * 0.12f, r * 0.16f, r * 0.16f, Hex(0xe8323a));
        rlPopMatrix();
        if (e.type == E_BRUTE && e.hp < e.max) HpBar(e.x, e.y - e.r - 8, 30, e.hp / e.max, Hex(0xc44cff));
    }
}

void Game::SurvDrawFx() {
    Surv& S = *surv;
    const double fx = std::cos(car.a), fy = std::sin(car.a);
    if (S.flameT > 0) {
        const int bl = S.up[U_BURNER] ? S.up[U_BURNER] : 1;
        const double L = 40 + 6 * bl, t = now * 10;
        const Color cols[3] = {Rgba(255, 240, 150, .8f), Rgba(255, 150, 40, .6f), Rgba(255, 70, 20, .35f)};
        for (int i = 0; i < 3; i++) {
            double k = L * (0.5 + i * 0.35) + std::sin(t + i) * 4, w = 6 + i * 4 + bl;
            draw::FillCircle((float)(car.x - fx * k), (float)(car.y - fy * k), (float)w, cols[i]);
        }
    }
    for (const Zap& z : S.zaps) {
        std::vector<Vector2> p = {{(float)car.x, (float)car.y}};
        for (int i = 1; i <= 6; i++) {
            double u = i / 6.0, j = i < 6 ? std::sin(z.seed + i * 12.9) * 14 : 0;
            p.push_back({(float)(car.x + (z.x1 - car.x) * u - fy * j), (float)(car.y + (z.y1 - car.y) * u + fx * j)});
        }
        draw::Polyline(p.data(), (int)p.size(), false, 3, Rgba(150, 230, 255, (float)(z.life / 0.18)), draw::ROUND, false);
    }
    HpBar(car.x, car.y + 30, 52, S.hp / S.maxHp, S.hp / S.maxHp < 0.3 ? Hex(0xff5a4a) : Hex(0x5fe07a));
}

void Game::SurvOverlay() {   // night: dark except around the headlights, red flash when hurt
    const Surv& sv = *surv;
    const double fx = car.x + std::cos(car.a) * 60, fy = car.y + std::sin(car.a) * 60;
    const float sx = (float)(W / 2 + (fx - cam.x) * cam.z * S), sy = (float)(H / 2 + (fy - cam.y) * cam.z * S);
    draw::RadialGradient(sx, sy, (float)(110 * cam.z * S), (float)(620 * cam.z * S), Rgba(8, 6, 30, 0), Rgba(8, 6, 30, 0.66f), (W + H) * 2);
    if (sv.hurtT > 0)   // red glow at the screen edges only, so the view stays readable
        draw::RadialGradient(W / 2, H / 2, std::min(W, H) * 0.3f, Hypot(W, H) / 2, Rgba(255, 30, 30, 0), Rgba(255, 30, 30, (float)std::min(0.55, sv.hurtT * 2.5)), (W + H) * 2);
}
