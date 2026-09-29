// city.h - endless city for survival: an organic street network generated cell by cell
// (port of js/city.js). Avenues join jittered lattice nodes with curved (quadratic) roads, some
// nodes become roundabouts, and each cell between four nodes gets side streets (through streets,
// crescents, cul-de-sacs), a park, or buildings that face and follow the nearest street.
// Everything is derived from a hash of the cell coordinates, so a cell always comes out the
// same and neighbours agree on shared edges.
#pragma once
#include "world.h"
#include <memory>
#include <unordered_map>

struct CityRoad { Pts pts; double w; bool main, closed; double bb[4]; };
struct CityArea { Pts pts; V2 ctr; double r = 0; };               // parks, squares (r: drift circle)
struct CityLot { V2 pts[4]; double ang, cx, cy, fw, dp; };
struct CityIsland { double x, y, r; };
struct CityCell {
    int i = 0, j = 0;
    std::vector<CityRoad> roads;
    std::vector<CityArea> parks, squares;
    std::vector<CityIsland> islands;
    std::vector<CityLot> lots;
    std::vector<Solid> solids;
    std::vector<Scen> scenery;
};
struct RoadSpot { double x, y, a; };

class EndlessCity {
public:
    static constexpr double CH = 960;   // cell size
    std::vector<CityCell*> CellsIn(double x0, double y0, double x1, double y1);
    int SurfaceAt(double x, double y);
    template <class F> void ForSolidsNear(double x, double y, double r, F fn) {   // fn returns true to stop
        for (CityCell* C : CellsIn(x - r, y - r, x + r, y + r))
            for (const Solid& b : C->solids) {
                if (x + r < b.bb[0] || x - r > b.bb[2] || y + r < b.bb[1] || y - r > b.bb[3]) continue;
                if (fn(b)) return;
            }
    }
    RoadSpot NearestRoad(double x, double y);   // a spot on the closest street, facing along it
    RoadSpot Start();
    void Evict(double x, double y);             // keep memory flat however far the car goes
    void Clear() { cells.clear(); }
    size_t Size() const { return cells.size(); }
    CityCell* GetCell(int i, int j);
private:
    std::unordered_map<long long, std::unique_ptr<CityCell>> cells;
};

// Helpers shared with the renderer.
struct LineInfo { double d, x, y, tx, ty; };
LineInfo LineInfoAt(double px, double py, const Pts& pts, bool closed);
bool InPoly(double x, double y, const Pts& P);
