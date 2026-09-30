// desert.h - the open desert for free roam: endless, generated in chunks around the car. Each chunk
// comes from a hash of its coordinates, so it is the same every time and the world never ends.
// No raylib here, so the tests can check it; desert_scene.cpp draws it.
#pragma once
#include "world.h"
#include <memory>
#include <unordered_map>

struct DesertChunk {
    int i = 0, j = 0;
    std::vector<Scen> scen;     // everything drawn: lakes, salt, rock plates, trails, rocks, ruins...
    std::vector<Solid> solids;  // what the car hits: rocks, hoodoos, ruins, walls, arch legs
};

// What the ground does to the car at a point.
struct Ground { double grip = 0.95, drag = 0, water = 0; bool soft = false; };   // water: depth 0..1

class DesertWorld {
public:
    static constexpr double CH = 1024;   // chunk size in world pixels
    static constexpr uint32_t SEED = 2024;
    DesertChunk* GetChunk(int i, int j);
    std::vector<DesertChunk*> ChunksIn(double x0, double y0, double x1, double y1);
    Ground GroundAt(double x, double y);
    template <class F> void ForSolidsNear(double x, double y, double r, F fn) {   // fn returns true to stop
        for (DesertChunk* C : ChunksIn(x - r, y - r, x + r, y + r))
            for (const Solid& b : C->solids) {
                if (x + r < b.bb[0] || x - r > b.bb[2] || y + r < b.bb[1] || y - r > b.bb[3]) continue;
                if (fn(b)) return;
            }
    }
    void Evict(double x, double y);   // keep memory flat however far the car goes
    void Clear() { chunks.clear(); }
    size_t Size() const { return chunks.size(); }
private:
    std::unordered_map<long long, std::unique_ptr<DesertChunk>> chunks;
};
