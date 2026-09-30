// world.h - types shared by the drift stages and the open desert.
// No raylib here, so the headless tests can build the same worlds.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

#ifndef PI
#define PI 3.14159265358979323846   // raylib.h defines the same, as a float
#endif

struct V2 { double x = 0, y = 0; };
using Pts = std::vector<V2>;

inline double Hypot(double x, double y) { return std::sqrt(x * x + y * y); }
inline double Clamp(double v, double a, double b) { return v < a ? a : v > b ? b : v; }
inline double Sign(double v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }
inline long long JsRound(double v) { return (long long)std::floor(v + 0.5); }   // Math.round
inline double Mod(double a, double n) { return std::fmod(std::fmod(a, n) + n, n); }

// Surfaces under the car. 4 is GRAVEL, used by the tracks.
enum Surface { ROAD = 0, WALK = 1, GRASS = 2, LOT = 3, GRAVEL = 4, SAND = 5, WATER = 6 };

// Colors are 0xRRGGBB.
extern const std::vector<uint32_t> FRONDS;   // palm leaf greens

// Buildings, palms, umbrellas and track decorations. Drawn with a fake perspective:
// the top is pushed away from the camera by `ht`.
enum ScenKind {
    SC_PALM, SC_UMB, SC_BLOCK, SC_TREE, SC_ROCK, SC_TIRES,
    // the desert stages
    SC_HOODOO,    // a tall stack of rock
    SC_ARCH,      // a stone arch from (x, y) to (x2, y2), drawn above the car
    SC_RUIN,      // a broken column
    SC_WALL,      // a low ruined wall, w x h, turned by a
    SC_LANTERN,   // a post with a lamp that glows at night
    SC_GRASS,     // a tuft of dune grass
    SC_BUSH,      // a low desert shrub
    SC_BALLOON,   // a hot-air balloon drifting high above
    SC_FLAGS,     // a line of small flags from (x, y) to (x2, y2)
    SC_OASIS,     // still water, radius r
    SC_STONE,     // a low stone marking the edge of the track
    SC_SALT,      // a salt flat, radius r: pale crust with less grip (it can cross the track)
    SC_PLATE,     // a flat plate of rock, radius r: more grip
    SC_TRAIL,     // faint old tyre tracks from (x, y) to (x2, y2), bending by a
};
struct Scen {
    ScenKind t = SC_PALM;
    double x = 0, y = 0, cx = 0, cy = 0, ht = 0;
    double r = 0, a = 0;              // palm, umbrella, tree, rock, tyres
    int n = 0;                        // palm fronds
    uint32_t col = 0, col2 = 0, towel = 0, acc = 0;
    bool hasAcc = false;
    double w = 0, h = 0;              // SC_BLOCK: axis-aligned x, y, w, h
    double x2 = 0, y2 = 0;            // SC_ARCH, SC_FLAGS: the other end
    int vents = 0;
    bool pool = false;
    double s = 0;                     // per-building random for roof details
    double clear = 0;                 // track decorations: distance kept from the track edge
};

// Walls: axis-aligned rectangles, rotated boxes, or circles (rocks, lakes: centre cx, cy and radius hw).
struct Solid {
    bool rot = false, circle = false;
    double x = 0, y = 0, w = 0, h = 0;
    double cx = 0, cy = 0, hw = 0, hh = 0, ang = 0;
    double bb[4] = {0, 0, 0, 0};
};
struct Hit { double nx = 0, ny = 0, pen = 0; };
// Pushes a circle out of a wall. Returns false when they do not touch.
bool CircleHit(const Solid& b, double cx, double cy, double r, Hit& out);

// Seeded generator (Park-Miller), so the desert and the stage scenery are the same every run.
struct ParkMiller {
    long long seed = 1337;
    double operator()() { seed = (seed * 16807) % 2147483647; return (double)(seed - 1) / 2147483646.0; }
};

template <class T> const T& Pick(const std::vector<T>& a, double r) { return a[(size_t)std::floor(r * a.size())]; }

// A palm tree. Keep the order of the random calls: the city and the map depend on it.
template <class R> Scen MakePalm(double x, double y, R& rnd) {
    Scen p;
    p.t = SC_PALM; p.x = p.cx = x; p.y = p.cy = y;
    p.r = 22 + rnd() * 10;
    p.ht = 0.07 + rnd() * 0.05;
    p.n = 7 + (int)std::floor(rnd() * 2);
    p.a = rnd() * 7;
    p.col = Pick(FRONDS, rnd());
    return p;
}
