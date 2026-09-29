// world.h - types shared by the fixed Miami map, the endless city and the drift tracks.
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
extern const std::vector<uint32_t> DECO, ACCENT, TOWERS, FRONDS;

// Buildings, palms, umbrellas and track decorations. Drawn with a fake perspective:
// the top is pushed away from the camera by `ht`.
enum ScenKind { SC_PALM, SC_UMB, SC_BLOCK, SC_POLY, SC_TREE, SC_ROCK, SC_TIRES };
struct Scen {
    ScenKind t = SC_PALM;
    double x = 0, y = 0, cx = 0, cy = 0, ht = 0;
    double r = 0, a = 0;              // palm, umbrella, tree, rock, tyres
    int n = 0;                        // palm fronds
    uint32_t col = 0, col2 = 0, towel = 0, acc = 0;
    bool hasAcc = false;
    double w = 0, h = 0;              // SC_BLOCK: axis-aligned x, y, w, h
    V2 pts[4];                        // SC_POLY: corners, clockwise on screen
    double ang = 0, hw = 0, hh = 0;   // SC_POLY
    int vents = 0;
    bool pool = false;
    double s = 0;                     // per-building random for roof details
    double clear = 0;                 // track decorations: distance kept from the track edge
};

// Walls: axis-aligned rectangles (Miami) or rotated boxes (endless city buildings).
struct Solid {
    bool rot = false;
    double x = 0, y = 0, w = 0, h = 0;
    double cx = 0, cy = 0, hw = 0, hh = 0, ang = 0;
    double bb[4] = {0, 0, 0, 0};
};
struct Hit { double nx = 0, ny = 0, pen = 0; };
// Pushes a circle out of a wall. Returns false when they do not touch.
bool CircleHit(const Solid& b, double cx, double cy, double r, Hit& out);

// JavaScript's seeded generator (Park-Miller), so the Miami map and track scenery match the web version.
struct ParkMiller {
    long long seed = 1337;
    double operator()() { seed = (seed * 16807) % 2147483647; return (double)(seed - 1) / 2147483646.0; }
};

template <class T> const T& Pick(const std::vector<T>& a, double r) { return a[(size_t)std::floor(r * a.size())]; }

// A palm tree. Random calls happen in the same order as makePalm() in the web version.
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
