// desert_scene.h - how the desert stages look: sand dunes lit by the stage's time of day, a packed
// sand road with soft edges, sparse scenery with long shadows, and light, colour grading and weather.
#pragma once
#include "raylib.h"
#include "world.h"
#include <string>
#include <unordered_map>
#include <vector>

class Game;
class DesertWorld;
struct Track;
struct StageDef;

// The look of one time of day (dawn, morning, noon, afternoon, sunset, storm, night, predawn).
struct Look {
    Color sandLight, sandShadow, sandLow;   // dunes: sunlit side, shaded side, hollows
    Color road, runoff;                     // packed-sand road, soft run-off
    Color skid, dust, zone;                 // tyre marks, drift dust, drift-zone tint
    Color light;                            // light colour multiplied into the scenery
    Color shadow;                           // shadow colour; its alpha is the strength
    double shadowDir = 0, shadowLen = 1;    // direction shadows fall (radians) and their length
    Color gradeTop, gradeBottom, vignette;  // screen colour grade
    Color dark;                             // night: the darkness away from the lights (alpha = how dark)
    bool lights = false;                    // lanterns glow
    int weather = 0;                        // 0 clear, 1 wind, 2 sandstorm
    float wind = 0, storm = 0;              // how much blowing sand and sandstorm haze (0..1)
};
Look LookFor(const std::string& sky, const std::string& weather);
// Free roam's day: 0 = dawn, 0.1 morning, 0.26 noon, 0.42 afternoon, 0.55 sunset, 0.68-0.86 night,
// 0.94 before dawn, back to dawn at 1. storm (0..1) blends a sandstorm in.
Look LookAtTime(double day, float storm);

class DesertScene {
public:
    void Load(const Track& T, const StageDef& def);   // a stage: bakes the ground and places the scenery
    void LoadFree(bool fixedDusk);                     // free roam (or the menu's background, held at dusk)
    void Unload();
    double Day() const { return day; }
    void SetDay(double d) { day = d; }                 // free roam's time of day (for screenshots)
    void Draw(Game& g, double hw, double hh);         // the world, inside the camera
    void DrawScreen(Game& g);                         // light, colour grade and haze, over the screen
    const Look& look() const { return lk; }

private:
    Look lk{};
    Texture2D ground{};
    double gx0 = 0, gy0 = 0, gw = 0, gh = 0;          // world rectangle the ground texture covers
    std::vector<Scen> scen;
    struct Grain { double x, y, len, life; };         // blowing sand streaks
    std::vector<Grain> grains;
    double windA = 0.35;
    void UpdateWeather(const Game& g, double hw, double hh, double dt);
    // free roam
    bool free = false, fixedDusk = false;
    double day = 0.2, stormT = 0, nextStorm = 150;
    float storm = 0;
    std::unordered_map<long long, Texture2D> chunkTex;   // dunes baked per chunk
    void DrawFree(Game& g, double hw, double hh);
    void DrawScenery(Game& g, const std::vector<const Scen*>& items, double hw, double hh);
    std::vector<V2> lamps;   // lantern tops seen this frame, for the night glow
    void BakeChunk(int i, int j);
};
