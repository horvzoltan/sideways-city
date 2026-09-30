#include "desert.h"
#include "terrain.h"
#include <algorithm>

namespace {
constexpr double CH = DesertWorld::CH;
const uint32_t ROCKS[] = {0xc08a62, 0xb77e58, 0xc99a70};
const uint32_t HOODOOS[] = {0xb9785a, 0xc4855f, 0xad6f52};
const uint32_t STONEWORK[] = {0xd8c2a0, 0xcdb08a, 0xe0cdaa};
const uint32_t GRASSES[] = {0x9aa86a, 0xa9b276, 0x8e9d62};
const uint32_t BUSHES[] = {0x7d8f5a, 0x869763, 0x74864f};
const uint32_t PASTELS[] = {0xe98a7a, 0xf2c46b, 0x7fb7c9, 0x9d8fd0, 0xf3e3c3, 0x86c29a};

uint32_t ChunkSeed(int i, int j) {
    uint32_t h = (uint32_t)i * 374761393u + (uint32_t)j * 668265263u + DesertWorld::SEED * 2246822519u;
    h = (h ^ (h >> 15)) * 2246822519u;
    h = (h ^ (h >> 13)) * 3266489917u;
    return (h ^ (h >> 16)) % 2147483646u + 1;
}
template <class T, size_t N> T Any(const T (&a)[N], double r) { return a[(size_t)(r * N) % N]; }

DesertChunk* Generate(int i, int j) {
    auto* C = new DesertChunk;
    C->i = i; C->j = j;
    ParkMiller rnd;
    rnd.seed = ChunkSeed(i, j);
    const double x0 = i * CH, y0 = j * CH;
    // the start (0, 0) is the corner of four chunks: all of them keep it clear
    const bool origin = (i == 0 || i == -1) && (j == 0 || j == -1);
    std::vector<std::pair<V2, double>> keepOut;
    if (origin) keepOut.push_back({{0, 0}, 360});
    auto clearAt = [&](V2 p, double r) {
        return std::none_of(keepOut.begin(), keepOut.end(), [&](auto& o) { return Hypot(p.x - o.first.x, p.y - o.first.y) < o.second + r; });
    };
    auto rndIn = [&](double margin) { return V2{x0 + margin + rnd() * (CH - 2 * margin), y0 + margin + rnd() * (CH - 2 * margin)}; };
    auto add = [&](ScenKind t, V2 p, double r, double ht, uint32_t col) -> Scen& {
        Scen s;
        s.t = t; s.x = s.cx = p.x; s.y = s.cy = p.y; s.r = r; s.ht = ht; s.col = col;
        s.a = rnd() * 2 * PI; s.s = rnd(); s.n = 5 + (int)(rnd() * 4);
        C->scen.push_back(s);
        return C->scen.back();
    };
    auto solidCircle = [&](V2 p, double r) {
        Solid b;
        b.circle = true; b.cx = p.x; b.cy = p.y; b.hw = r;
        b.bb[0] = p.x - r; b.bb[1] = p.y - r; b.bb[2] = p.x + r; b.bb[3] = p.y + r;
        C->solids.push_back(b);
    };

    // big features first: a lake, a salt flat or a rock plate, so the rest keeps out of the water
    const double feature = rnd();
    if (!origin && feature < 0.13) {
        const V2 p = rndIn(260);
        const double r = 140 + rnd() * 160;
        add(SC_OASIS, p, r, 0, 0x5fa9a4);   // drivable: the car wades through (GroundAt)
        keepOut.push_back({p, r + 40});
        const int palms = 5 + (int)(rnd() * 4);
        for (int k = 0; k < palms; k++) {
            const double a = rnd() * 2 * PI, d = r + 30 + rnd() * 50;
            C->scen.push_back(MakePalm(p.x + std::cos(a) * d, p.y + std::sin(a) * d, rnd));
        }
    } else if (!origin && feature < 0.24) {
        const V2 p = rndIn(200);
        const double r = 250 + rnd() * 230;
        add(SC_SALT, p, r, 0, 0xf6f1e8);
        keepOut.push_back({p, r * 0.9});
    } else if (feature < 0.34) {
        const V2 p = rndIn(200);
        const double r = 200 + rnd() * 180;
        if (clearAt(p, r)) add(SC_PLATE, p, r, 0, 0xa88a6e);
    }
    // a cluster of ruins with a lantern or two
    if (rnd() < 0.09) {
        const V2 c = rndIn(220);
        if (clearAt(c, 200)) {
            const int n = 3 + (int)(rnd() * 3);
            for (int k = 0; k < n; k++) {
                const V2 p{c.x + (rnd() - 0.5) * 300, c.y + (rnd() - 0.5) * 300};
                if (rnd() < 0.35) {
                    Scen& w = add(SC_WALL, p, 0, 0.04 + rnd() * 0.03, Any(STONEWORK, rnd()));
                    w.w = 60 + rnd() * 70; w.h = 16 + rnd() * 6;
                    Solid b;
                    b.rot = true; b.cx = p.x; b.cy = p.y; b.hw = w.w / 2; b.hh = w.h / 2; b.ang = w.a;
                    const double rad = Hypot(b.hw, b.hh);
                    b.bb[0] = p.x - rad; b.bb[1] = p.y - rad; b.bb[2] = p.x + rad; b.bb[3] = p.y + rad;
                    C->solids.push_back(b);
                } else {
                    const double r = 11 + rnd() * 7;
                    add(SC_RUIN, p, r, 0.06 + rnd() * 0.05, Any(STONEWORK, rnd()));
                    solidCircle(p, r);
                }
            }
            for (int k = 0; k < 2; k++) {
                const V2 p{c.x + (rnd() - 0.5) * 360, c.y + (rnd() - 0.5) * 360};
                add(SC_LANTERN, p, 5, 0.09, 0x5a4636);
                solidCircle(p, 5);
            }
            keepOut.push_back({c, 200});
        }
    }
    // a line of flags, a stone arch, faint old tyre tracks
    if (rnd() < 0.1) {
        const V2 p = rndIn(200);
        const double a = rnd() * PI, len = 160 + rnd() * 110;
        if (clearAt(p, 40)) {
            Scen& f = add(SC_FLAGS, p, 0, 0.09, Any(PASTELS, rnd()));
            f.x2 = p.x + std::cos(a) * len; f.y2 = p.y + std::sin(a) * len;
            f.cx = (p.x + f.x2) / 2; f.cy = (p.y + f.y2) / 2;
        }
    }
    if (!origin && rnd() < 0.05) {
        const V2 c = rndIn(260);
        const double a = rnd() * PI, half = 150 + rnd() * 50;
        const V2 l{c.x - std::cos(a) * half, c.y - std::sin(a) * half}, r{c.x + std::cos(a) * half, c.y + std::sin(a) * half};
        if (clearAt(c, half)) {
            Scen& ar = add(SC_ARCH, l, 34, 0.14, HOODOOS[0]);
            ar.x2 = r.x; ar.y2 = r.y; ar.cx = c.x; ar.cy = c.y;
            solidCircle(l, 34); solidCircle(r, 34);
            keepOut.push_back({l, 60}); keepOut.push_back({r, 60});
        }
    }
    if (rnd() < 0.05) {   // a balloon drifting high above
        Scen& b = add(SC_BALLOON, rndIn(100), 34 + rnd() * 16, 0.5 + rnd() * 0.15, Any(PASTELS, rnd()));
        b.col2 = Any(PASTELS, rnd());
    }
    if (rnd() < 0.25) {
        const V2 p = rndIn(0);
        const double a = rnd() * 2 * PI, len = 400 + rnd() * 500;
        Scen& t = add(SC_TRAIL, p, 0, 0, 0x6a5040);
        t.x2 = p.x + std::cos(a) * len; t.y2 = p.y + std::sin(a) * len; t.a = (rnd() - 0.5) * 0.8;
        t.cx = (p.x + t.x2) / 2; t.cy = (p.y + t.y2) / 2;
    }
    // scattered rocks, hoodoos, grass and shrubs
    auto scatter = [&](int n, double clear, auto make) {
        for (int k = 0; k < n; k++) { const V2 p = rndIn(20); if (clearAt(p, clear)) make(p); }
    };
    scatter(3, 50, [&](V2 p) { const double r = 14 + rnd() * 26; add(SC_ROCK, p, r, 0.03 + rnd() * 0.03, Any(ROCKS, rnd())); solidCircle(p, r * 0.85); });
    scatter(rnd() < 0.5 ? 1 : 0, 60, [&](V2 p) { const double r = 22 + rnd() * 18; add(SC_HOODOO, p, r, 0.08 + rnd() * 0.06, Any(HOODOOS, rnd())); solidCircle(p, r * 0.9); });
    scatter(10, 10, [&](V2 p) { add(SC_GRASS, p, 9 + rnd() * 9, 0.01, Any(GRASSES, rnd())); });
    scatter(3, 16, [&](V2 p) { add(SC_BUSH, p, 10 + rnd() * 10, 0.02, Any(BUSHES, rnd())); });
    return C;
}
}  // namespace

DesertChunk* DesertWorld::GetChunk(int i, int j) {
    const long long key = ((long long)i << 32) ^ (unsigned)j;
    auto it = chunks.find(key);
    if (it != chunks.end()) return it->second.get();
    DesertChunk* c = Generate(i, j);
    chunks.emplace(key, std::unique_ptr<DesertChunk>(c));
    return c;
}

std::vector<DesertChunk*> DesertWorld::ChunksIn(double x0, double y0, double x1, double y1) {   // chunks whose contents can reach into the rectangle
    std::vector<DesertChunk*> out;
    for (int j = (int)std::floor(y0 / CH) - 1; j <= (int)std::floor(y1 / CH) + 1; j++)
        for (int i = (int)std::floor(x0 / CH) - 1; i <= (int)std::floor(x1 / CH) + 1; i++) out.push_back(GetChunk(i, j));
    return out;
}

Ground DesertWorld::GroundAt(double x, double y) {
    Ground g;
    for (DesertChunk* C : ChunksIn(x, y, x, y))
        for (const Scen& s : C->scen) {
            if (s.t == SC_OASIS) {   // water: the car wades, slowing hard and sliding as it gets deeper
                const double k = WaterDepth(x, y, s.x, s.y, s.r, s.s);
                if (k > 0) { g.water = k; g.grip = 0.75 - 0.2 * k; g.drag = 0.6 + 1.2 * k; return g; }
                continue;
            }
            if (s.t != SC_SALT && s.t != SC_PLATE) continue;
            const double d = Hypot(x - s.x, y - s.y);
            if (d >= s.r) continue;
            if (s.t == SC_PLATE) { g.grip = 1.05; return g; }   // rock: firm and grippy
            const double k = std::min(1.0, (1 - d / s.r) * 2.5);
            g.grip = std::min(g.grip, 0.95 * (1 - 0.28 * k));   // salt: slippery in the middle
        }
    // soft sand on the dune crests slows the car
    const double h = DuneHeight(x, y, SEED);
    if (h > 0.74) { g.drag = std::min(1.0, (h - 0.74) * 8) * 0.8; g.grip *= 0.85; g.soft = true; }
    return g;
}

void DesertWorld::Evict(double x, double y) {
    if (chunks.size() < 100) return;
    const int ci = (int)std::floor(x / CH), cj = (int)std::floor(y / CH);
    for (auto it = chunks.begin(); it != chunks.end();) {
        if (std::abs(it->second->i - ci) > 4 || std::abs(it->second->j - cj) > 4) it = chunks.erase(it);
        else ++it;
    }
}
