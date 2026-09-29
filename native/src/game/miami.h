// miami.h - the fixed free-drive map: a little Miami Beach. Biscayne Bay to the west with two
// causeways, art deco blocks in the middle, the beach and the ocean to the east.
#pragma once
#include "world.h"

struct Miami {
    static constexpr int TILE = 64, BLOCK = 8, ROADW = 2, NB = 8, MW = NB * BLOCK, MH = NB * BLOCK;
    static constexpr double WORLD_W = MW * TILE, WORLD_H = MH * TILE;
    // tile columns: Biscayne Bay west of BAY_X, promenade at BEACH_X, then sand, ocean from SEA_X
    static constexpr int BAY_X = 7, BEACH_X = 58, SEA_X = 62;

    std::vector<uint8_t> map;       // Surface per tile
    std::vector<Scen> scenery;      // buildings, huts, palms, umbrellas
    std::vector<int> buildings;     // indices into scenery, for the minimap
    std::vector<Solid> solids;      // buildings, huts and the seawalls
    std::vector<std::vector<int>> solidGrid;   // walls by tile, so collision only tests what is nearby

    void Generate();
    int Tile(int x, int y) const { return map[y * MW + x]; }
    int TileAt(double px, double py) const {
        int x = (int)std::floor(px / TILE), y = (int)std::floor(py / TILE);
        if (x < 0 || y < 0 || x >= MW || y >= MH) return ROAD;
        return map[y * MW + x];
    }
    template <class F> void ForSolidsNear(double x, double y, double r, F fn) const {   // may repeat a wall; fn returns true to stop
        int x0 = std::max(0, (int)std::floor((x - r) / TILE)), x1 = std::min(MW - 1, (int)std::floor((x + r) / TILE));
        int y0 = std::max(0, (int)std::floor((y - r) / TILE)), y1 = std::min(MH - 1, (int)std::floor((y + r) / TILE));
        for (int ty = y0; ty <= y1; ty++)
            for (int tx = x0; tx <= x1; tx++)
                for (int b : solidGrid[ty * MW + tx]) if (fn(solids[b])) return;
    }
};
