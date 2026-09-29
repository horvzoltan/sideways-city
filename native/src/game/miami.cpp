#include "miami.h"
#include <algorithm>

namespace {
const std::vector<std::pair<uint32_t, uint32_t>> UMB = {
    {0xe2598b, 0xf6f1e6}, {0x2bb5b0, 0xf6f1e6}, {0xf39a4a, 0xfff3c4}, {0x5b8fd6, 0xf6f1e6}, {0xf3d34a, 0xe2598b}};
}

void Miami::Generate() {
    ParkMiller rnd;   // seed 1337, like the web version
    const int CAUSEWAYS[2] = {2, 5};   // block rows whose street crosses the bay
    map.assign(MW * MH, WALK);
    scenery.clear(); buildings.clear(); solids.clear();
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            bool streetRow = y % BLOCK < ROADW;
            int t = (x % BLOCK < ROADW || streetRow) ? ROAD : WALK;
            int by = y / BLOCK;
            if (x <= BAY_X) t = streetRow && (by == CAUSEWAYS[0] || by == CAUSEWAYS[1]) ? ROAD : x < BAY_X ? WATER : WALK;
            else if (x >= SEA_X) t = WATER;
            else if (x > BEACH_X) t = SAND;
            else if (x == BEACH_X) t = WALK;
            map[y * MW + x] = (uint8_t)t;
        }
    auto addPalm = [&](double x, double y) { scenery.push_back(MakePalm(x, y, rnd)); };
    auto addBuilding = [&](int tx, int ty, int tw, int th, bool tall) {
        const double pad = 6;
        const bool pool = (tall || tw * th >= 8) && rnd() < 0.5;
        Scen b;
        b.t = SC_BLOCK;
        b.x = tx * TILE + pad; b.y = ty * TILE + pad; b.w = tw * TILE - pad * 2; b.h = th * TILE - pad * 2;
        b.ht = tall ? 0.38 + rnd() * 0.26 : 0.1 + rnd() * 0.14;
        b.col = Pick(tall ? TOWERS : DECO, rnd());
        if (!tall) { b.acc = Pick(ACCENT, rnd()); b.hasAcc = true; }
        b.vents = pool ? 0 : (int)std::floor(rnd() * 3);
        b.pool = pool;
        b.s = rnd();
        b.cx = b.x + b.w / 2; b.cy = b.y + b.h / 2;
        buildings.push_back((int)scenery.size());
        scenery.push_back(b);
        Solid s; s.x = b.x; s.y = b.y; s.w = b.w; s.h = b.h;
        solids.push_back(s);
    };
    for (int j = 0; j < NB; j++)
        for (int i = 1; i < NB - 1; i++) {
            const int x0 = i * BLOCK + ROADW, y0 = j * BLOCK + ROADW, S = BLOCK - ROADW;
            const double r = rnd();
            const bool tall = rnd() < (i >= 5 ? 0.5 : 0.15);   // high-rises cluster near the ocean
            if (r < 0.14) {
                for (int y = 1; y < S - 1; y++) for (int x = 1; x < S - 1; x++) map[(y0 + y) * MW + x0 + x] = GRASS;
                for (int k = 0; k < 7; k++) {
                    double px = (x0 + 1.4 + rnd() * (S - 2.8)) * TILE;
                    double py = (y0 + 1.4 + rnd() * (S - 2.8)) * TILE;
                    addPalm(px, py);
                }
            } else if (r < 0.28) {
                for (int y = 0; y < S; y++) for (int x = 0; x < S; x++) map[(y0 + y) * MW + x0 + x] = LOT;
            } else {
                const double q = rnd();
                if (q < 0.35) addBuilding(x0 + 1, y0 + 1, 4, 4, tall);
                else if (q < 0.65) { addBuilding(x0 + 1, y0 + 1, 2, 4, tall); addBuilding(x0 + 3, y0 + 1, 2, 4, tall && rnd() < 0.5); }
                else if (q < 0.85) { addBuilding(x0 + 1, y0 + 1, 4, 2, tall); addBuilding(x0 + 1, y0 + 3, 4, 2, false); }
                else { addBuilding(x0 + 1, y0 + 1, 2, 2, false); addBuilding(x0 + 3, y0 + 1, 2, 2, false); addBuilding(x0 + 1, y0 + 3, 4, 2, tall); }
                // palms on the sidewalk ring
                for (int k = 0; k < S; k += 2) {
                    if (rnd() < 0.45) addPalm((x0 + k + 0.5) * TILE, (y0 + 0.5) * TILE);
                    if (rnd() < 0.45) addPalm((x0 + k + 0.5) * TILE, (y0 + S - 0.5) * TILE);
                }
            }
        }
    // palms along the bayfront walk and the beach promenade
    for (double py = 40; py < WORLD_H; py += 110 + rnd() * 40) {
        if (map[(int)std::floor(py / TILE) * MW + BAY_X] == WALK) addPalm((BAY_X + 0.5) * TILE, py);
        double px = (BEACH_X + 0.5) * TILE + (rnd() - .5) * 12;
        double pyy = py + rnd() * 30;
        addPalm(px, pyy);
    }
    // beach: umbrellas and the lifeguard stands
    for (double py = 90; py < WORLD_H - 90; py += 70 + rnd() * 110) {
        const double px = (BEACH_X + 1.3 + rnd() * 2.2) * TILE;
        if (std::fabs(std::fmod(py, BLOCK * TILE) - 4.4 * TILE) < 90) continue;   // keep clear of the lifeguard stands
        Scen u;
        u.t = SC_UMB; u.x = u.cx = px; u.y = u.cy = py;
        u.r = 20 + rnd() * 6; u.ht = 0.04;
        auto cols = Pick(UMB, rnd()); u.col = cols.first; u.col2 = cols.second;
        u.a = rnd() * 7;
        u.towel = Pick(ACCENT, rnd());
        scenery.push_back(u);
    }
    for (int j = 0; j < NB; j++) {
        Scen hut;
        hut.t = SC_BLOCK;
        hut.x = (BEACH_X + 2.2) * TILE; hut.y = (j * BLOCK + 4) * TILE; hut.w = 46; hut.h = 46; hut.ht = 0.09;
        hut.col = Pick(DECO, rnd());
        hut.acc = Pick(ACCENT, rnd()); hut.hasAcc = true;
        hut.vents = 0; hut.s = rnd();
        hut.cx = hut.x + 23; hut.cy = hut.y + 23;
        Solid s; s.x = hut.x; s.y = hut.y; s.w = 46; s.h = 46;
        solids.push_back(s);
        scenery.push_back(hut);
    }
    // the sea and the bay are solid seawalls: one rect per row run of water tiles
    for (int y = 0; y < MH; y++) {
        int x = 0;
        while (x < MW) {
            if (map[y * MW + x] != WATER) { x++; continue; }
            int e = x;
            while (e < MW && map[y * MW + e] == WATER) e++;
            Solid s; s.x = x * TILE; s.y = y * TILE; s.w = (e - x) * TILE; s.h = TILE;
            solids.push_back(s);
            x = e;
        }
    }
    solidGrid.assign(MW * MH, {});
    for (int k = 0; k < (int)solids.size(); k++) {
        const Solid& b = solids[k];
        int tx0 = std::max(0, (int)std::floor(b.x / TILE)), tx1 = std::min(MW - 1, (int)std::floor((b.x + b.w) / TILE));
        int ty0 = std::max(0, (int)std::floor(b.y / TILE)), ty1 = std::min(MH - 1, (int)std::floor((b.y + b.h) / TILE));
        for (int y = ty0; y <= ty1; y++) for (int x = tx0; x <= tx1; x++) solidGrid[y * MW + x].push_back(k);
    }
}
