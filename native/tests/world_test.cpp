// world_test.cpp - headless checks: the stages, the open desert and the car handling.
// Run from anywhere; the stage files are read from the source tree (STAGES_DIR).
#include "game/car.h"
#include "game/desert.h"
#include "game/terrain.h"
#include "game/tracks.h"
#include <algorithm>
#include <cstdio>
#include <string>

static int failures = 0;
static void Check(bool ok, const std::string& what) {
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) failures++;
}

// ---- stages: every file loads, and every track is drivable and has no shortcuts ----
static void StageChecks() {
    std::string err;
    Check(LoadStages(STAGES_DIR, &err), "stage files load" + (err.empty() ? "" : ": " + err));
    Check(STAGES.size() == 8, "8 stages (found " + std::to_string(STAGES.size()) + ")");
    for (size_t i = 0; i < STAGES.size(); i++) {
        const StageDef& d = STAGES[i];
        Track T = BuildTrack(d);
        StageGoals(T, (int)i);
        double mk = 0;
        for (double k : T.k) mk = std::max(mk, std::fabs(k));
        // closest approach between sections far apart along the lap (a shortcut would skip them)
        const int skip = (int)std::ceil(T.edge * 6 / 18);
        double clear = 1e9;
        for (int a = 0; a < T.N; a++)
            for (int b = a + skip; b < T.N; b++)
                if (T.N - (b - a) >= skip) clear = std::min(clear, Hypot(T.pts[a].x - T.pts[b].x, T.pts[a].y - T.pts[b].y) - 2 * T.edge);
        StageDef again;
        std::string text;
        const bool ok = T.zones.size() >= 3 && T.limit >= 40 && T.limit <= 200 && 1 / mk >= 0.4 * T.w && clear >= 30;
        if (!ok) std::printf("      zones %zu  limit %d s  tightest radius %.0f (width %.0f)  clearance %.0f\n", T.zones.size(), T.limit, 1 / mk, T.w, clear);
        Check(ok, "stage " + d.id + ": zones, time limit, corners and no shortcuts");
    }
    // desert scenery: the same every time, and never on the road or run-off
    for (size_t i = 0; i < STAGES.size(); i++) {
        Track T = BuildTrack(STAGES[i]);
        const auto s1 = ScatterDesert(T, STAGES[i]), s2 = ScatterDesert(T, STAGES[i]);
        bool same = s1.size() == s2.size(), clear = true;
        for (size_t k = 0; same && k < s1.size(); k++) same = s1[k].x == s2[k].x && s1[k].y == s2[k].y && s1[k].t == s2[k].t;
        for (const Scen& d : s1) {
            if (d.t == SC_BALLOON || d.t == SC_STONE || d.t == SC_SALT) continue;   // balloons fly over; stones mark the edge; salt crosses the road
            auto near = [&](double x, double y) { double n = 1e18; for (const V2& p : T.pts) n = std::min(n, Hypot(p.x - x, p.y - y)); return n; };
            const double r = d.t == SC_OASIS ? d.r : 0;   // water must stay clear of the run-off
            if (near(d.x, d.y) < T.edge + r) clear = false;
            if ((d.t == SC_ARCH || d.t == SC_FLAGS) && near(d.x2, d.y2) < T.edge) clear = false;   // both legs / poles
        }
        Check(same && clear && s1.size() > 50, "stage " + STAGES[i].id + ": scenery is repeatable and off the track (" + std::to_string(s1.size()) + " pieces)");
    }
    Check(DuneHeight(1234, 567, 9) == DuneHeight(1234, 567, 9) && DuneHeight(1234, 567, 9) != DuneHeight(1234, 567, 10), "dunes depend only on position and seed");

    StageDef a, b;
    ParseStage("name = X\npt 0 0\npt 100 0\npt 100 100\npt 0 100\n", "x", a, nullptr);
    ParseStage("name = X\npt 0 0\npt 100 0\npt 100 100\npt 0 100\n", "x", b, nullptr);
    StageDef c;
    ParseStage("name = X\npt 0 0\npt 100 0\npt 100 100\npt 0 101\n", "x", c, nullptr);
    Check(a.fingerprint == b.fingerprint && a.fingerprint != c.fingerprint, "stage fingerprints change only when the file does");
    std::string why;
    Check(!ParseStage("name = X\nwidht = 100\npt 0 0\n", "bad", c, &why) && why.find("widht") != std::string::npos, "a typo in a stage file is reported");
    Check(!ParseStage("name = X\npt 0 0\npt 100 0\npt 100 100\npt 0 100\nlandmark hodoo 5 5\n", "bad", c, &why) && why.find("hodoo") != std::string::npos, "an unknown landmark is reported");
}

// ---- the open desert (free roam) ----
static void DesertChecks() {
    DesertWorld w1, w2;
    bool same = true;
    for (int j = -3; j <= 3; j++)
        for (int i = -3; i <= 3; i++) {   // generated in a different order, the chunks still come out the same
            const DesertChunk* a = w1.GetChunk(i, j);
            const DesertChunk* b = w2.GetChunk(-i, -j) ? w2.GetChunk(i, j) : nullptr;
            same = same && a->scen.size() == b->scen.size() && a->solids.size() == b->solids.size();
            for (size_t k = 0; same && k < a->scen.size(); k++) same = a->scen[k].x == b->scen[k].x && a->scen[k].t == b->scen[k].t;
        }
    Check(same, "desert chunks are the same whatever order they are made in");
    Hit h;
    bool startClear = true;
    w1.ForSolidsNear(0, 0, 300, [&](const Solid& s) { if (CircleHit(s, 0, 0, 300, h)) startClear = false; return false; });
    Check(startClear, "the desert start is clear of anything solid");
    int lakes = 0, salts = 0, plates = 0, lakeWet = 0, saltSlips = 0, plateGrips = 0;
    for (int j = -12; j <= 12; j++)
        for (int i = -12; i <= 12; i++)
            for (const Scen& s : w1.GetChunk(i, j)->scen) {
                if (s.t == SC_OASIS) {
                    lakes++;
                    bool hit = false;   // drivable: nothing solid in the middle, deep water there, dry beyond the shore
                    w1.ForSolidsNear(s.x, s.y, 10, [&](const Solid& b) { return hit = CircleHit(b, s.x, s.y, 10, h); });
                    const Ground mid = w1.GroundAt(s.x, s.y), out = w1.GroundAt(s.x + s.r * 1.2, s.y);
                    lakeWet += !hit && mid.water == 1 && mid.drag > 1 && out.water == 0;
                }
                if (s.t == SC_SALT) { salts++; saltSlips += w1.GroundAt(s.x, s.y).grip < 0.8; }
                if (s.t == SC_PLATE) { plates++; plateGrips += w1.GroundAt(s.x, s.y).grip > 1; }
            }
    std::printf("      625 chunks: %d lakes, %d salt flats, %d rock plates\n", lakes, salts, plates);
    Check(lakes > 20 && salts > 20 && plates > 20, "the desert has lakes, salt flats and rock plates");
    Check(lakeWet == lakes, "lakes are drivable water: deep in the middle, dry past the shore");
    Check(saltSlips == salts && plateGrips == plates, "salt flats are slippery, rock plates grip");
}

// ---- handling (car.cpp): the arcade drift model ----
namespace {
const double DT = 1.0 / 60, PI_T = 3.14159265358979;
double SlipDeg(const CarState& c) {   // angle between heading and travel
    double fx = std::cos(c.a), fy = std::sin(c.a), u = c.vx * fx + c.vy * fy, v = -c.vx * fy + c.vy * fx;
    return std::hypot(u, v) < 20 ? 0 : std::fabs(std::atan2(v, u)) * 180 / PI_T;
}
template <class F> double MaxSlip(CarState c, double secs, F input, double* endSpeed = nullptr) {
    double mx = 0;
    for (double t = 0; t < secs; t += DT) { StepCar(c, input(t), CarSurface{}, 1, 1, DT); mx = std::max(mx, SlipDeg(c)); }
    if (endSpeed) *endSpeed = std::hypot(c.vx, c.vy);
    return mx;
}
CarState Rolling(double speed) { CarState c; c.vx = speed; return c; }
void HandlingChecks() {
    double speed = 0;
    const double launch = MaxSlip(CarState{}, 4, [](double) { CarInput i; i.throttle = 1; return i; }, &speed);
    Check(launch < 1, "handling: a full-throttle launch goes straight");
    Check(std::fabs(speed - Handling{}.topSpeed) < 5, "handling: top speed is " + std::to_string((int)Handling{}.topSpeed) + " px/s");
    auto flick = [](double hand) {
        return [=](double t) { CarInput i; i.throttle = 1; i.steer = t < 0.5 ? 1 : 0.5; i.handbrake = t < 0.4 ? hand : 0; return i; };
    };
    const double full = MaxSlip(Rolling(350), 1.5, flick(1)), light = MaxSlip(Rolling(350), 1.5, flick(0.3)), none = MaxSlip(Rolling(350), 1.5, flick(0));
    std::printf("      handbrake flick at 350: full %.0f deg, light L2 pull %.0f deg, none %.0f deg\n", full, light, none);
    Check(full > 25, "handling: a handbrake flick at speed kicks the rear out");
    Check(light < full && light > none, "handling: a light L2 pull does less than a full one");
    // holding a drift: gas and steering into the turn keep the slide going
    CarState c = Rolling(350);
    double held = 0;
    for (double t = 0; t < 4; t += DT) {
        CarInput i; i.throttle = 1; i.steer = 1; i.handbrake = t < 0.3 ? 1 : 0;
        StepCar(c, i, CarSurface{}, 1, 1, DT);
        if (t > 1 && SlipDeg(c) > 15) held += DT;
    }
    Check(held > 2.5, "handling: gas and steering hold a drift");
}
}  // namespace

int main() {
    StageChecks();

    DesertChecks();

    HandlingChecks();
    std::printf(failures ? "\n%d check(s) failed\n" : "\nall checks passed\n", failures);
    return failures ? 1 : 0;
}
