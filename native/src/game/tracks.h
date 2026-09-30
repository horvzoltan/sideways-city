// tracks.h - the drift stages and their track geometry. Stages are plain text files in
// assets/stages/ (see the comment at the top of any of them), loaded in file-name order.
#pragma once
#include "world.h"
#include <cstdint>
#include <string>

// A hand-placed feature near the track: an arch, a ruin, a balloon... (drawn by the renderer).
struct Landmark { std::string type; double x = 0, y = 0, size = 1, angle = 0; };

struct StageDef {
    std::string id;                  // the file name without extension; progress and ghosts are saved under it
    std::string name, desc;
    double w = 180, r = 80;          // road width, run-off width
    int laps = 3;
    double v = 240;                  // pace: sets the time limit (higher = less time)
    std::string sky = "day", weather = "clear";
    double grip = 1;                 // below 1 is slippery
    double passScale = 1;            // scales the target score
    int seed = 1;                    // for the scattered scenery
    Pts pts;                         // control points, driven in this order
    std::vector<Landmark> landmarks;
    uint32_t fingerprint = 0;        // hash of the file: ghosts recorded on another version are ignored
};
// Stages unlock in order.
extern std::vector<StageDef> STAGES;

// Loads every *.stage file in dir, sorted by name. Returns false (and a reason) if a file is bad.
bool LoadStages(const std::string& dir, std::string* error);
bool ParseStage(const std::string& text, const std::string& id, StageDef& out, std::string* error);

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
