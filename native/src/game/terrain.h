// terrain.h - the desert: a dune heightfield and the scenery around a stage. No raylib here, so the
// tests can check the generators; desert_scene.cpp does the drawing.
#pragma once
#include "tracks.h"
#include <algorithm>
#include <cstdint>

// Dune height at a world position, roughly 0..1: soft ridges running across the wind, with slow
// rolling swells underneath. The same seed always gives the same desert.
double DuneHeight(double x, double y, uint32_t seed);

// The scenery around a stage: what the stage's sky calls for (rocks, hoodoos, ruins, lanterns,
// grass, flags, balloons...), kept clear of the track, plus the stage file's landmarks and the low
// stones along both edges of the track.
std::vector<Scen> ScatterDesert(const Track& T, const StageDef& def);

// A lake's (or salt flat's) outline: the radius at angle a is r times this, a circle with a few slow waves.
inline double ShoreWave(double a, double seed) {
    return 1 + 0.07 * std::sin(3 * a + seed * 11) + 0.05 * std::sin(5 * a + seed * 23) + 0.03 * std::sin(8 * a + seed * 7);
}
// How deep a point is in a lake of radius r, 0 on the shore to 1 a third of the way in.
inline double WaterDepth(double x, double y, double cx, double cy, double r, double seed) {
    const double d = Hypot(x - cx, y - cy), R = r * ShoreWave(std::atan2(y - cy, x - cx), seed);
    return d >= R ? 0 : std::min(1.0, (1 - d / R) * 3);
}

// Salt flats (landmark salt): radius for a landmark size, and the grip left at a point (1 = none).
inline double SaltRadius(double size) { return 200 * size; }
inline double SaltGrip(const StageDef& def, double x, double y) {
    double g = 1;
    for (const Landmark& l : def.landmarks) {
        if (l.type != "salt") continue;
        const double r = SaltRadius(l.size), d = Hypot(x - l.x, y - l.y);
        if (d < r) { const double k = 1 - d / r; g = std::min(g, 1 - 0.28 * std::min(1.0, k * 2.5)); }   // 72% in the middle, easing out at the edge
    }
    return g;
}
