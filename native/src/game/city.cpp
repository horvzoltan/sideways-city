#include "city.h"
#include <algorithm>

namespace {
constexpr double CH = EndlessCity::CH, RING_R = 130, RING_W = 110, SIDE_W = 96, STEP = 24, COARSE = 40;

double Hash(int i, int j, int k) {
    uint32_t h = ((uint32_t)i * 374761393u + (uint32_t)j * 668265263u + (uint32_t)(k + 1) * 1274126177u) ^ 0x5bf03635u;
    h = (h ^ (h >> 15)) * 2246822519u;
    h = (h ^ (h >> 13)) * 3266489917u;
    h ^= h >> 16;
    return h / 4294967296.0;
}
struct CellRng {
    int i, j, n = 0;
    double operator()() { return Hash(i, j, 1000 + (n++)); }
};

// the avenue network, shared by neighbouring cells
bool IsOrigin(int i, int j) { return i == 0 && j == 0; }
V2 Node(int i, int j) {
    if (IsOrigin(i, j)) return {0, 0};
    return {i * CH + (Hash(i, j, 1) - 0.5) * CH * 0.34, j * CH + (Hash(i, j, 2) - 0.5) * CH * 0.34};
}
bool Ring(int i, int j) { return !IsOrigin(i, j) && Hash(i, j, 3) < 0.16; }
bool EdgeOn(char k, int i, int j) {   // a few avenues are missing: bigger blocks and T-junctions (never next to the start)
    if (k == 'h' ? (j == 0 && (i == 0 || i == -1)) : (i == 0 && (j == 0 || j == -1))) return true;
    return Hash(i, j, k == 'h' ? 4 : 5) > 0.1;
}
double EdgeW(char k, int i, int j) { return ((k == 'h' ? j : i) % 3 == 0) ? 180 : 146; }   // every third line is a wide boulevard
struct Bez { V2 a, c, b; };
Bez Edge(char k, int i, int j) {
    V2 a = Node(i, j), b = k == 'h' ? Node(i + 1, j) : Node(i, j + 1);
    double dx = b.x - a.x, dy = b.y - a.y, L = Hypot(dx, dy);
    bool strong = Hash(i, j, k == 'h' ? 8 : 9) < 0.4;
    double bend = (Hash(i, j, k == 'h' ? 6 : 7) - 0.5) * CH * (strong ? 0.5 : 0.08);
    return {a, {(a.x + b.x) / 2 - dy / L * bend, (a.y + b.y) / 2 + dx / L * bend}, b};
}

// geometry helpers
V2 BezAt(const Bez& e, double t) {
    double u = 1 - t;
    return {u * u * e.a.x + 2 * u * t * e.c.x + t * t * e.b.x, u * u * e.a.y + 2 * u * t * e.c.y + t * t * e.b.y};
}
Pts BezPts(const Bez& e, double step) {
    int n = std::max(3, (int)std::ceil(Hypot(e.b.x - e.a.x, e.b.y - e.a.y) / step));
    Pts p;
    for (int q = 0; q <= n; q++) p.push_back(BezAt(e, (double)q / n));
    return p;
}
Pts CirclePts(double x, double y, double r, int n) {
    Pts p;
    for (int q = 0; q < n; q++) { double t = (double)q / n * PI * 2; p.push_back({x + std::cos(t) * r, y + std::sin(t) * r}); }
    return p;
}
void BBox(const Pts& pts, double m, double out[4]) {
    double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (V2 p : pts) { x0 = std::min(x0, p.x); x1 = std::max(x1, p.x); y0 = std::min(y0, p.y); y1 = std::max(y1, p.y); }
    out[0] = x0 - m; out[1] = y0 - m; out[2] = x1 + m; out[3] = y1 + m;
}
struct Quad { V2 p[4]; };
Quad Obb(double cx, double cy, double hw, double hh, double ang) {   // corners clockwise on screen
    double c = std::cos(ang), s = std::sin(ang);
    const double L[4][2] = {{-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}};
    Quad q;
    for (int k = 0; k < 4; k++) q.p[k] = {cx + L[k][0] * c - L[k][1] * s, cy + L[k][0] * s + L[k][1] * c};
    return q;
}
bool QuadsOverlap(const Quad& A, const Quad& B) {   // separating axis test for two convex quads
    for (const Quad* P : {&A, &B})
        for (int q = 0; q < 4; q++) {
            V2 a = P->p[q], b = P->p[(q + 1) % 4];
            double nx = b.y - a.y, ny = a.x - b.x;
            double amin = 1e18, amax = -1e18, bmin = 1e18, bmax = -1e18;
            for (V2 p : A.p) { double v = p.x * nx + p.y * ny; amin = std::min(amin, v); amax = std::max(amax, v); }
            for (V2 p : B.p) { double v = p.x * nx + p.y * ny; bmin = std::min(bmin, v); bmax = std::max(bmax, v); }
            if (amax < bmin || bmax < amin) return false;
        }
    return true;
}

struct Obs { Pts pts; double w; };   // everything buildings must keep clear of
struct Clear { LineInfo li; double c, half; };

CityCell* GenCell(int i, int j) {
    CellRng R{i, j};
    auto* C = new CityCell;
    C->i = i; C->j = j;
    std::vector<Obs> obs;
    auto addRoad = [&](const Pts& pts, double w, bool main, bool closed) {
        CityRoad r{pts, w, main, closed, {}};
        BBox(pts, w / 2 + 4, r.bb);
        C->roads.push_back(r);
    };
    enum { TOP, RIGHT, BOTTOM, LEFT };
    const Bez E[4] = {Edge('h', i, j), Edge('v', i + 1, j), Edge('h', i, j + 1), Edge('v', i, j)};
    const bool on[4] = {EdgeOn('h', i, j), EdgeOn('v', i + 1, j), EdgeOn('h', i, j + 1), EdgeOn('v', i, j)};
    const double W[4] = {EdgeW('h', i, j), EdgeW('v', i + 1, j), EdgeW('h', i, j + 1), EdgeW('v', i, j)};
    // this cell draws its top and left avenues and its own corner roundabout; neighbours draw the rest
    if (on[TOP]) addRoad(BezPts(E[TOP], STEP), W[TOP], true, false);
    if (on[LEFT]) addRoad(BezPts(E[LEFT], STEP), W[LEFT], true, false);
    if (Ring(i, j)) {
        V2 n = Node(i, j);
        addRoad(CirclePts(n.x, n.y, RING_R, 40), RING_W, false, true);
        C->islands.push_back({n.x, n.y, RING_R - RING_W / 2});
        C->scenery.push_back(MakePalm(n.x, n.y, R));
    }
    for (int k = 0; k < 4; k++) obs.push_back({BezPts(E[k], COARSE), on[k] ? W[k] : 0});   // a missing avenue still splits cells
    const int corners[4][2] = {{i, j}, {i + 1, j}, {i + 1, j + 1}, {i, j + 1}};
    for (auto& ab : corners)
        if (Ring(ab[0], ab[1])) { V2 n = Node(ab[0], ab[1]); obs.push_back({{n, n}, (RING_R + RING_W / 2) * 2}); }

    Pts boundary;
    auto append = [&](Pts p, bool rev) { if (rev) std::reverse(p.begin(), p.end()); boundary.insert(boundary.end(), p.begin(), p.end()); };
    append(BezPts(E[TOP], COARSE), false); append(BezPts(E[RIGHT], COARSE), false);
    append(BezPts(E[BOTTOM], COARSE), true); append(BezPts(E[LEFT], COARSE), true);
    const V2 n00 = Node(i, j), n10 = Node(i + 1, j), n11 = Node(i + 1, j + 1), n01 = Node(i, j + 1);
    const V2 ctr{(n00.x + n10.x + n11.x + n01.x) / 4, (n00.y + n10.y + n11.y + n01.y) / 4};
    const double kr = R();
    enum Kind { PARK, SQUARE, CITY };
    const Kind kind = IsOrigin(i, j) ? SQUARE : kr < 0.1 ? PARK : kr < 0.45 ? SQUARE : CITY;   // lots of open asphalt to fight on
    auto street = [&](const Pts& pts, double w) { addRoad(pts, w, false, false); obs.push_back({pts, w}); };
    auto curve = [&](V2 a, V2 c, V2 b) { return BezPts({a, c, b}, STEP); };

    if (kind == CITY) {   // side streets
        const double v = R();
        auto mid = [&](int k) { return BezAt(E[k], 0.5); };
        auto through = [&](int k1, int k2) {
            V2 a = mid(k1), b = mid(k2);
            double cx = ctr.x + (R() - 0.5) * CH * 0.3;
            double cy = ctr.y + (R() - 0.5) * CH * 0.3;
            street(curve(a, {cx, cy}, b), SIDE_W);
        };
        if (v < 0.22 && on[TOP] && on[BOTTOM]) through(TOP, BOTTOM);
        else if (v < 0.44 && on[LEFT] && on[RIGHT]) through(LEFT, RIGHT);
        else if (v < 0.56 && on[TOP] && on[BOTTOM] && on[LEFT] && on[RIGHT]) { through(TOP, BOTTOM); through(LEFT, RIGHT); }
        else if (v < 0.74) {   // crescent: a round street cutting off one corner
            struct Cr { int k1; double t1; int k2; double t2; V2 cn; };
            const std::vector<Cr> opts = {{TOP, .38, LEFT, .38, n00}, {TOP, .62, RIGHT, .38, n10}, {RIGHT, .62, BOTTOM, .62, n11}, {BOTTOM, .38, LEFT, .62, n01}};
            const Cr c = Pick(opts, R());
            if (on[c.k1] && on[c.k2])
                street(curve(BezAt(E[c.k1], c.t1), {c.cn.x + (ctr.x - c.cn.x) * 0.6, c.cn.y + (ctr.y - c.cn.y) * 0.6}, BezAt(E[c.k2], c.t2)), SIDE_W);
        } else if (v < 0.88) {   // cul-de-sac with a turning circle
            std::vector<int> ks;
            for (int k = 0; k < 4; k++) if (on[k]) ks.push_back(k);
            if (!ks.empty()) {
                V2 a = mid(Pick(ks, R())), end{a.x + (ctr.x - a.x) * 0.6, a.y + (ctr.y - a.y) * 0.6};
                double cx = (a.x + end.x) / 2 + (R() - 0.5) * 90;
                double cy = (a.y + end.y) / 2 + (R() - 0.5) * 90;
                street(curve(a, {cx, cy}, end), SIDE_W);
                addRoad(CirclePts(end.x, end.y, 28, 16), 56, false, true);
                obs.push_back({{end, end}, 112});
            }
        }
    }

    auto clear = [&](double x, double y) {   // distance from (x,y) to the nearest road edge, and which road
        Clear best{};
        bool have = false;
        for (const Obs& ob : obs) {
            LineInfo li = LineInfoAt(x, y, ob.pts, false);
            double c = li.d - ob.w / 2;
            if (!have || c < best.c) { best = {li, c, ob.w / 2}; have = true; }
        }
        return best;
    };
    std::vector<Quad> placed;
    if (kind == PARK) C->parks.push_back({boundary, ctr, 0});
    if (kind == SQUARE) C->squares.push_back({boundary, ctr, 140 + R() * 80});
    if (kind == CITY) {   // buildings and parking lots, fronting the nearest street
        double bb[4];
        BBox(boundary, 0, bb);
        for (double gy = bb[1] + 60; gy < bb[3]; gy += 160)
            for (double gx = bb[0] + 60; gx < bb[2]; gx += 160) {
                double px = gx + (R() - 0.5) * 60;
                double py = gy + (R() - 0.5) * 60;
                if (R() < 0.3 || !InPoly(px, py, boundary)) continue;
                Clear nr = clear(px, py);
                if (nr.c < 30) continue;
                const bool lot = R() < 0.4;
                const bool tall = !lot && R() < 0.2;
                const double fw = lot ? 200 + R() * 120 : tall ? 100 + R() * 50 : 60 + R() * 70;   // frontage
                const double dp = lot ? 130 + R() * 70 : tall ? 100 + R() * 50 : 55 + R() * 55;   // depth
                double nx = px - nr.li.x, ny = py - nr.li.y, nl = Hypot(nx, ny);
                if (nl == 0) nl = 1;
                nx /= nl; ny /= nl;
                const double off = nr.half + 16 + dp / 2, cx = nr.li.x + nx * off, cy = nr.li.y + ny * off, ang = std::atan2(nr.li.ty, nr.li.tx);
                const Quad q = Obb(cx, cy, fw / 2, dp / 2, ang);
                bool ok = true;
                for (V2 p : q.p) if (!InPoly(p.x, p.y, boundary) || clear(p.x, p.y).c < 12) { ok = false; break; }
                if (!ok) continue;
                if (clear(cx, cy).c < std::min(fw, dp) / 2) continue;
                const Quad grown = Obb(cx, cy, fw / 2 + 10, dp / 2 + 10, ang);
                if (std::any_of(placed.begin(), placed.end(), [&](const Quad& o) { return QuadsOverlap(grown, o); })) continue;
                placed.push_back(q);
                if (lot) {
                    CityLot l;
                    for (int k = 0; k < 4; k++) l.pts[k] = q.p[k];
                    l.ang = ang; l.cx = cx; l.cy = cy; l.fw = fw; l.dp = dp;
                    C->lots.push_back(l);
                    continue;
                }
                const bool pool = (tall || fw * dp > 9000) && R() < 0.5;
                const double rad = Hypot(fw, dp) / 2;
                Scen b;
                b.t = SC_POLY;
                for (int k = 0; k < 4; k++) b.pts[k] = q.p[k];
                b.ang = ang; b.x = b.cx = cx; b.y = b.cy = cy; b.hw = fw / 2; b.hh = dp / 2;
                b.ht = tall ? 0.38 + R() * 0.26 : 0.1 + R() * 0.14;
                b.col = Pick(tall ? TOWERS : DECO, R());
                if (!tall) { b.acc = Pick(ACCENT, R()); b.hasAcc = true; }
                b.vents = pool ? 0 : (int)std::floor(R() * 3);
                b.pool = pool;
                b.s = R();
                Solid s;
                s.rot = true; s.cx = cx; s.cy = cy; s.hw = fw / 2; s.hh = dp / 2; s.ang = ang;
                s.bb[0] = cx - rad; s.bb[1] = cy - rad; s.bb[2] = cx + rad; s.bb[3] = cy + rad;
                C->solids.push_back(s);
                C->scenery.push_back(b);
            }
    }
    // palms along every street on this side, then scattered through parks and the start plaza
    auto palmOk = [&](double x, double y) {
        if (!InPoly(x, y, boundary)) return false;
        if (clear(x, y).c < 8) return false;
        const Quad sq = Obb(x, y, 10, 10, 0);
        return std::none_of(placed.begin(), placed.end(), [&](const Quad& o) { return QuadsOverlap(sq, o); });
    };
    for (const Obs& ob : obs) {
        if (!ob.w || ob.pts.size() < 3) continue;
        double acc = R() * 120;
        for (size_t q = 1; q < ob.pts.size(); q++) {
            V2 a = ob.pts[q - 1], b = ob.pts[q];
            double L = Hypot(b.x - a.x, b.y - a.y);
            acc += L;
            if (acc < 140 + R() * 60) continue;
            acc = 0;
            double tx = (b.x - a.x) / L, ty = (b.y - a.y) / L, off = ob.w / 2 + 22;
            for (int sgn : {1, -1}) {
                double x = b.x - ty * off * sgn, y = b.y + tx * off * sgn;
                if (palmOk(x, y)) C->scenery.push_back(MakePalm(x, y, R));
            }
        }
    }
    if (kind == PARK) {
        double bb[4];
        BBox(boundary, 0, bb);
        for (int k = 0; k < 10; k++) {
            double x = bb[0] + R() * (bb[2] - bb[0]);
            double y = bb[1] + R() * (bb[3] - bb[1]);
            if (palmOk(x, y) && clear(x, y).c > 30) C->scenery.push_back(MakePalm(x, y, R));
        }
    }
    return C;
}
}  // namespace

LineInfo LineInfoAt(double px, double py, const Pts& pts, bool closed) {   // nearest point on a polyline, with the segment direction
    double best = 1e9, bx = pts[0].x, by = pts[0].y, tx = 1, ty = 0;
    const int n = (int)pts.size(), m = closed ? n : n - 1;
    for (int q = 0; q < std::max(1, m); q++) {
        V2 a = pts[q], b = pts[(q + 1) % n];
        double dx = b.x - a.x, dy = b.y - a.y, L2 = dx * dx + dy * dy;
        double u = L2 ? ((px - a.x) * dx + (py - a.y) * dy) / L2 : 0;
        u = u < 0 ? 0 : u > 1 ? 1 : u;
        double qx = a.x + dx * u, qy = a.y + dy * u, d = Hypot(px - qx, py - qy);
        if (d < best) { best = d; bx = qx; by = qy; if (L2) { tx = dx; ty = dy; } }
    }
    double L = Hypot(tx, ty);
    if (L == 0) L = 1;
    return {best, bx, by, tx / L, ty / L};
}

bool InPoly(double x, double y, const Pts& P) {
    bool c = false;
    for (size_t q = 0, k = P.size() - 1; q < P.size(); k = q++) {
        V2 a = P[q], b = P[k];
        if ((a.y > y) != (b.y > y) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x) c = !c;
    }
    return c;
}

CityCell* EndlessCity::GetCell(int i, int j) {
    long long key = ((long long)i << 32) ^ (unsigned)j;
    auto it = cells.find(key);
    if (it != cells.end()) return it->second.get();
    CityCell* c = GenCell(i, j);
    cells.emplace(key, std::unique_ptr<CityCell>(c));
    return c;
}

std::vector<CityCell*> EndlessCity::CellsIn(double x0, double y0, double x1, double y1) {   // cells whose contents can reach into the rectangle
    std::vector<CityCell*> out;
    for (int j = (int)std::floor(y0 / CH) - 1; j <= (int)std::floor(y1 / CH) + 1; j++)
        for (int i = (int)std::floor(x0 / CH) - 1; i <= (int)std::floor(x1 / CH) + 1; i++) out.push_back(GetCell(i, j));
    return out;
}

int EndlessCity::SurfaceAt(double x, double y) {
    auto near = CellsIn(x, y, x, y);
    for (CityCell* C : near) for (auto& s : C->islands) if ((x - s.x) * (x - s.x) + (y - s.y) * (y - s.y) < s.r * s.r) return GRASS;
    for (CityCell* C : near)
        for (auto& r : C->roads) {
            if (x < r.bb[0] || x > r.bb[2] || y < r.bb[1] || y > r.bb[3]) continue;
            if (LineInfoAt(x, y, r.pts, r.closed).d < r.w / 2) return ROAD;
        }
    for (CityCell* C : near) for (auto& p : C->parks) if (InPoly(x, y, p.pts)) return GRASS;
    for (CityCell* C : near) for (auto& q : C->squares) if (InPoly(x, y, q.pts)) return ROAD;
    for (CityCell* C : near)
        for (auto& l : C->lots) { Pts p(l.pts, l.pts + 4); if (InPoly(x, y, p)) return LOT; }
    return WALK;
}

RoadSpot EndlessCity::NearestRoad(double x, double y) {
    bool have = false;
    LineInfo best{};
    for (CityCell* C : CellsIn(x, y, x, y))
        for (auto& r : C->roads) {
            if (r.w < SIDE_W) continue;
            LineInfo li = LineInfoAt(x, y, r.pts, r.closed);
            if (!have || li.d < best.d) { best = li; have = true; }
        }
    return {best.x, best.y, std::atan2(best.ty, best.tx)};
}

RoadSpot EndlessCity::Start() {
    Bez e = Edge('h', 0, 0);
    V2 p = BezAt(e, 0.1), q = BezAt(e, 0.12);
    return {p.x, p.y, std::atan2(q.y - p.y, q.x - p.x)};
}

void EndlessCity::Evict(double x, double y) {
    if (cells.size() < 140) return;
    int ci = (int)std::floor(x / CH), cj = (int)std::floor(y / CH);
    for (auto it = cells.begin(); it != cells.end();) {
        if (std::abs(it->second->i - ci) > 4 || std::abs(it->second->j - cj) > 4) it = cells.erase(it);
        else ++it;
    }
}
