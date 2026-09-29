// tracks.h - the drift stages and their track geometry (port of js/tracks.js).
#pragma once
#include "world.h"
#include <string>

struct StageDef {
    std::string name, desc;
    double w, r;       // road width, gravel run-off width
    int laps;
    double v;          // sets the time limit (higher = less time)
    std::string theme;
    bool eight;        // figure eight, generated instead of pts
    Pts pts;           // control points, driven in this order
};
// Stages unlock in order and progress is saved by position, so add new ones at the end.
extern const std::vector<StageDef> STAGES;

struct Clip { int i; double x, y; };
struct Zone { int a, b; double len; std::vector<Clip> clips; };
struct Track {
    const StageDef* def = nullptr;
    int N = 0;                  // samples, 18 px apart
    double L = 0;               // length
    Pts pts, tan;
    std::vector<double> k;      // smoothed curvature
    double w = 0, half = 0, edge = 0;
    std::vector<Zone> zones;    // drift zones with their clipping points
    double bounds[4] = {0, 0, 0, 0};
    int pass = 0, limit = 0;    // target score, time limit in seconds
    double clipR = 52;
};

Track BuildTrack(const StageDef& def);
// Target score and time limit for stage i.
void StageGoals(Track& T, int i);
