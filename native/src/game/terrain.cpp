#include "terrain.h"
#include <algorithm>
#include <map>

namespace {
uint32_t Hash(int x, int y, uint32_t seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
double Lattice(int x, int y, uint32_t seed) { return Hash(x, y, seed) / 4294967296.0; }
double Smooth(double t) { return t * t * (3 - 2 * t); }
double ValueNoise(double x, double y, uint32_t seed) {   // 0..1
    const int ix = (int)std::floor(x), iy = (int)std::floor(y);
    const double fx = Smooth(x - ix), fy = Smooth(y - iy);
    const double a = Lattice(ix, iy, seed), b = Lattice(ix + 1, iy, seed), c = Lattice(ix, iy + 1, seed), d = Lattice(ix + 1, iy + 1, seed);
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}
double Fbm(double x, double y, uint32_t seed, int octaves) {
    double sum = 0, amp = 0.5, norm = 0;
    for (int o = 0; o < octaves; o++) { sum += ValueNoise(x, y, seed + o * 101) * amp; norm += amp; x *= 2.03; y *= 2.03; amp *= 0.5; }
    return sum / norm;
}

// ---- scenery ----
struct Mix { double rock, hoodoo, ruin, wall, lantern, grass, bush, palm, flags; int balloons; bool lanternRows; };
// counts per million square pixels (a stage covers about 20), balloons in total
const std::map<std::string, Mix> MIXES = {
    {"dawn",      {3, 0.6, 0, 0, 0, 14, 3, 0, 0.3, 4, false}},
    {"morning",   {3, 0.5, 0, 0, 0, 10, 2, 0, 0.8, 3, false}},
    {"noon",      {6, 3, 0, 0, 0, 4, 2, 0, 0, 0, false}},
    {"afternoon", {2, 0.3, 0, 0, 0, 12, 5, 1.2, 0, 2, false}},
    {"sunset",    {2, 0.4, 3, 1.5, 0.4, 6, 2, 0, 0.3, 1, true}},
    {"storm",     {5, 1.5, 0.4, 0.3, 0, 6, 2, 0, 0, 0, false}},
    {"night",     {3, 1.5, 0.6, 0.4, 0.5, 6, 2, 0, 0, 0, true}},
    {"predawn",   {4, 2, 0, 0, 0.2, 5, 2, 0, 0.5, 1, true}},
};
const uint32_t ROCKS[] = {0xc08a62, 0xb77e58, 0xc99a70};
const uint32_t HOODOOS[] = {0xb9785a, 0xc4855f, 0xad6f52};
const uint32_t STONEWORK[] = {0xd8c2a0, 0xcdb08a, 0xe0cdaa};
const uint32_t GRASSES[] = {0x9aa86a, 0xa9b276, 0x8e9d62};
const uint32_t BUSHES[] = {0x7d8f5a, 0x869763, 0x74864f};
const uint32_t PASTELS[] = {0xe98a7a, 0xf2c46b, 0x7fb7c9, 0x9d8fd0, 0xf3e3c3, 0x86c29a};

double TrackDistance(const Track& T, double x, double y) {
    double best = 1e18;
    for (const V2& p : T.pts) best = std::min(best, (p.x - x) * (p.x - x) + (p.y - y) * (p.y - y));
    return std::sqrt(best);
}
}  // namespace

double DuneHeight(double x, double y, uint32_t seed) {
    // ridges run across a wind from the west-north-west, bent by a slow warp
    const double wa = 0.35, c = std::cos(wa), s = std::sin(wa);
    const double u = x * c + y * s, v = -x * s + y * c;
    const double warp = Fbm(x * 0.0011, y * 0.0011, seed, 3) * 3.2;
    const double ridge = 0.5 + 0.5 * std::sin(u * 0.0042 + warp + 0.8 * std::sin(v * 0.0017));
    const double dunes = std::pow(ridge, 1.6);                      // soft troughs, rounder crests
    const double swell = Fbm(x * 0.00055, y * 0.00055, seed + 7, 4); // slow rolling ground
    return 0.55 * dunes * (0.6 + 0.8 * swell) + 0.45 * swell;
}

std::vector<Scen> ScatterDesert(const Track& T, const StageDef& def) {
    std::vector<Scen> out;
    ParkMiller rnd;
    rnd.seed = 1000 + def.seed * 7919;
    auto it = MIXES.find(def.sky);
    const Mix mix = it != MIXES.end() ? it->second : MIXES.at("noon");
    const double m = 700, x0 = T.bounds[0] - m, y0 = T.bounds[1] - m, x1 = T.bounds[2] + m, y1 = T.bounds[3] + m;
    const double area = (x1 - x0) * (y1 - y0) / 1e6;
    // lakes and salt flats keep the scattered scenery out
    std::vector<std::pair<V2, double>> keepOut;
    for (const Landmark& l : def.landmarks) {
        if (l.type == "oasis") keepOut.push_back({{l.x, l.y}, 180 * l.size + 50});
        if (l.type == "salt") keepOut.push_back({{l.x, l.y}, SaltRadius(l.size)});
    }
    auto spot = [&](double clear, V2& p) {   // a random place at least `clear` beyond the run-off
        for (int k = 0; k < 20; k++) {
            p = {x0 + rnd() * (x1 - x0), y0 + rnd() * (y1 - y0)};
            if (TrackDistance(T, p.x, p.y) <= T.edge + clear) continue;
            if (std::any_of(keepOut.begin(), keepOut.end(), [&](auto& o) { return Hypot(p.x - o.first.x, p.y - o.first.y) < o.second + clear; })) continue;
            return true;
        }
        return false;
    };
    auto add = [&](ScenKind t, V2 p, double r, double ht, uint32_t col) {
        Scen s;
        s.t = t; s.x = s.cx = p.x; s.y = s.cy = p.y; s.r = r; s.ht = ht; s.col = col;
        s.a = rnd() * 2 * PI; s.s = rnd(); s.n = 5 + (int)(rnd() * 4);
        out.push_back(s);
        return &out.back();
    };
    auto scatter = [&](double perMillion, auto make) {
        const int n = (int)std::round(perMillion * area);
        for (int i = 0; i < n; i++) make();
    };
    V2 p;
    scatter(mix.rock, [&] { double r = 14 + rnd() * 26; if (spot(r + 30, p)) add(SC_ROCK, p, r, 0.03 + rnd() * 0.03, ROCKS[(int)(rnd() * 3)]); });
    scatter(mix.hoodoo, [&] { double r = 22 + rnd() * 18; if (spot(r + 40, p)) add(SC_HOODOO, p, r, 0.08 + rnd() * 0.06, HOODOOS[(int)(rnd() * 3)]); });
    scatter(mix.ruin, [&] { double r = 11 + rnd() * 7; if (spot(r + 30, p)) add(SC_RUIN, p, r, 0.06 + rnd() * 0.05, STONEWORK[(int)(rnd() * 3)]); });
    scatter(mix.wall, [&] {
        if (!spot(90, p)) return;
        Scen* w = add(SC_WALL, p, 0, 0.04 + rnd() * 0.03, STONEWORK[(int)(rnd() * 3)]);
        w->w = 60 + rnd() * 80; w->h = 16 + rnd() * 6;
    });
    scatter(mix.lantern, [&] { if (spot(30, p)) add(SC_LANTERN, p, 5, 0.09, 0x5a4636); });
    scatter(mix.grass, [&] { if (spot(12, p)) add(SC_GRASS, p, 9 + rnd() * 9, 0.01, GRASSES[(int)(rnd() * 3)]); });
    scatter(mix.bush, [&] { if (spot(16, p)) add(SC_BUSH, p, 10 + rnd() * 10, 0.02, BUSHES[(int)(rnd() * 3)]); });
    scatter(mix.palm, [&] { if (spot(40, p)) { out.push_back(MakePalm(p.x, p.y, rnd)); } });
    scatter(mix.flags, [&] {
        if (!spot(60, p)) return;
        const double a = rnd() * PI, len = 160 + rnd() * 110;
        const V2 q{p.x + std::cos(a) * len, p.y + std::sin(a) * len};
        if (TrackDistance(T, q.x, q.y) < T.edge + 60) return;
        Scen* f = add(SC_FLAGS, p, 0, 0.09, PASTELS[(int)(rnd() * 6)]);
        f->x2 = q.x; f->y2 = q.y; f->cx = (p.x + q.x) / 2; f->cy = (p.y + q.y) / 2;
    });
    for (int i = 0; i < mix.balloons; i++) {   // high up, drifting slowly across the stage
        Scen* b = add(SC_BALLOON, {x0 + rnd() * (x1 - x0), y0 + rnd() * (y1 - y0)}, 34 + rnd() * 16, 0.5 + rnd() * 0.15, PASTELS[(int)(rnd() * 6)]);
        b->col2 = PASTELS[(int)(rnd() * 6)];
    }
    // lanterns lining the track on the evening and night stages
    if (mix.lanternRows) {
        int side = 1;
        for (int i = 0; i < T.N; i += 36) {
            const V2 c = T.pts[i], t = T.tan[i];
            const V2 q{c.x - t.y * (T.edge + 26) * side, c.y + t.x * (T.edge + 26) * side};
            if (TrackDistance(T, q.x, q.y) > T.edge + 14) add(SC_LANTERN, q, 5, 0.09, 0x5a4636);
            side = -side;
        }
    }
    // low stones along both edges of the run-off: they show where the track ends
    for (int i = 0; i < T.N; i += 4)
        for (int side : {1, -1}) {
            const V2 c = T.pts[i], t = T.tan[i];
            const double off = T.edge + 5;
            const V2 q{c.x - t.y * off * side + (rnd() - 0.5) * 6, c.y + t.x * off * side + (rnd() - 0.5) * 6};
            if (TrackDistance(T, q.x, q.y) > T.edge - 2) add(SC_STONE, q, 4 + rnd() * 3, 0.012, ROCKS[(int)(rnd() * 3)]);
        }
    // hand-placed landmarks from the stage file
    for (const Landmark& l : def.landmarks) {
        const V2 at{l.x, l.y};
        if (l.type == "arch") {
            const double half = 110 * l.size, c = std::cos(l.angle), s = std::sin(l.angle);
            Scen* a = add(SC_ARCH, {l.x - c * half, l.y - s * half}, 26 * l.size, 0.14, HOODOOS[0]);
            a->x2 = l.x + c * half; a->y2 = l.y + s * half; a->cx = l.x; a->cy = l.y;
        } else if (l.type == "oasis") {
            add(SC_OASIS, at, 180 * l.size, 0, 0x5fa9a4);
            for (int k = 0; k < 7; k++) {   // palms around the shore
                const double a = rnd() * 2 * PI, r = 180 * l.size + 30 + rnd() * 40;
                out.push_back(MakePalm(l.x + std::cos(a) * r, l.y + std::sin(a) * r, rnd));
            }
        } else if (l.type == "salt") add(SC_SALT, at, SaltRadius(l.size), 0, 0xf6f1e8);
        else if (l.type == "hoodoo") add(SC_HOODOO, at, 30 * l.size, 0.13, HOODOOS[1]);
        else if (l.type == "ruin") add(SC_RUIN, at, 15 * l.size, 0.1, STONEWORK[0]);
        else if (l.type == "wall") { Scen* w = add(SC_WALL, at, 0, 0.06, STONEWORK[1]); w->w = 120 * l.size; w->h = 20; w->a = l.angle; }
        else if (l.type == "lantern") add(SC_LANTERN, at, 5, 0.09, 0x5a4636);
        else if (l.type == "rock") add(SC_ROCK, at, 30 * l.size, 0.05, ROCKS[0]);
        else if (l.type == "palm") out.push_back(MakePalm(l.x, l.y, rnd));
        else if (l.type == "balloon") { Scen* b = add(SC_BALLOON, at, 42 * l.size, 0.6, PASTELS[0]); b->col2 = PASTELS[4]; }
        else if (l.type == "flags") {
            const double len = 200 * l.size;
            Scen* f = add(SC_FLAGS, at, 0, 0.09, PASTELS[1]);
            f->x2 = l.x + std::cos(l.angle) * len; f->y2 = l.y + std::sin(l.angle) * len;
            f->cx = (l.x + f->x2) / 2; f->cy = (l.y + f->y2) / 2;
        }
    }
    return out;
}
