// render.cpp - drawing the world: Miami, the endless city and the tracks, with buildings and
// palms in a fake perspective (tops pushed away from the camera), the car, and the minimaps.
#include "game.h"
#include "rlgl.h"
#include <algorithm>

namespace {
const uint32_t C_ROAD = 0x3d3f46, C_WALK = 0xd9cfbe, C_GRASS = 0x5c9a45, C_LOT = 0x4a4c53, C_SAND = 0xeedba9, C_WATER = 0x2c8db0;
const uint32_t SEA = 0x2690b8;
uint32_t SurfaceColor(int t) {
    switch (t) {
        case ROAD: return C_ROAD; case WALK: return C_WALK; case GRASS: return C_GRASS; case LOT: return C_LOT;
        case SAND: return C_SAND; default: return C_WATER;
    }
}
inline Vector2 F(V2 p) { return {(float)p.x, (float)p.y}; }
inline Vector2 F(double x, double y) { return {(float)x, (float)y}; }
std::vector<Vector2> F(const Pts& p) {
    std::vector<Vector2> v(p.size());
    for (size_t i = 0; i < p.size(); i++) v[i] = F(p[i]);
    return v;
}
void Push(double x, double y, double a = 0, double s = 1) {
    rlPushMatrix();
    rlTranslatef((float)x, (float)y, 0);
    if (a) rlRotatef((float)(a * RAD2DEG), 0, 0, 1);
    if (s != 1) rlScalef((float)s, (float)s, 1);
}
void Pop() { rlPopMatrix(); }
void Rect(float x, float y, float w, float h, Color c) { draw::FillRect(x, y, w, h, c); }
void Poly(std::initializer_list<Vector2> p, Color c) { draw::FillConvex(p.begin(), (int)p.size(), c); }

// car body outline, flattened once
const std::vector<Vector2>& BodyPath() {
    static std::vector<Vector2> p;
    if (!p.empty()) return p;
    Vector2 cur{24, -7};
    p.push_back(cur);
    auto q = [&](float cx, float cy, float x, float y) { draw::QuadTo(p, cur, {cx, cy}, {x, y}, 6); cur = {x, y}; };
    auto l = [&](float x, float y) { p.push_back({x, y}); cur = {x, y}; };
    q(25.2f, 0, 24, 7); q(21, 10.5f, 15, 10.5f); l(-9, 10.5f); q(-14.5f, 12, -20, 11); q(-24, 10, -23.6f, 0);
    q(-24, -10, -20, -11); q(-14.5f, -12, -9, -10.5f); l(15, -10.5f); q(21, -10.5f, 24, -7);
    p.pop_back();   // back at the start
    return p;
}

// blend state for drawing into a minimap texture: premultiplied colour, correct alpha
void BeginTextureBlend() {
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
}
void DrawTextureRT(const RenderTexture2D& rt, float x, float y, float w, float h) {
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    DrawTexturePro(rt.texture, {0, 0, (float)rt.texture.width, -(float)rt.texture.height}, {x, y, w, h}, {0, 0}, 0, WHITE);
    EndBlendMode();
}
}  // namespace

// ---------- car ----------
void Game::DrawCar(double x, double y, double a, double w, bool braking, const Paint& p, float alpha) {
    auto A = [&](Color c) { return Alpha(c, alpha); };
    const auto& body = BodyPath();
    const int n = (int)body.size();
    Push(x, y, a);
    std::vector<Vector2> sh(body);
    for (auto& v : sh) { v.x += 2; v.y += 3; }
    draw::FillFan({2, 3}, sh.data(), n, A(Rgba(0, 0, 0, .35f)));
    // tyres peek out from under the wide arches; fronts turn with the steering
    const Color tyre = A(Hex(0x141414));
    Rect(-18, -13, 10, 3, tyre); Rect(-18, 10, 10, 3, tyre);
    Push(12.5, 0, w * 0.14);
    Rect(-4.5f, -12.5f, 9, 3, tyre); Rect(-4.5f, 9.5f, 9, 3, tyre);
    Pop();
    // metallic paint: a vertical gradient light -> base -> dark
    auto grad = [&](Vector2 v) {
        float u = (v.y + 12) / 24;
        return A(u < 0.45f ? Mix(p.light, p.base, std::max(0.0f, u / 0.45f)) : Mix(p.base, p.dark, std::min(1.0f, (u - 0.45f) / 0.55f)));
    };
    rlBegin(RL_TRIANGLES);
    Color c0 = grad({0, 0});
    for (int i = 0; i < n; i++) {
        Vector2 v1 = body[i], v2 = body[(i + 1) % n];
        Color a1 = grad(v1), a2 = grad(v2);
        rlColor4ub(c0.r, c0.g, c0.b, c0.a); rlVertex2f(0, 0);
        rlColor4ub(a1.r, a1.g, a1.b, a1.a); rlVertex2f(v1.x, v1.y);
        rlColor4ub(a2.r, a2.g, a2.b, a2.a); rlVertex2f(v2.x, v2.y);
    }
    rlEnd();
    draw::StrokePoly(body.data(), n, 0.8f, A(Rgba(0, 0, 0, .35f)));
    // mirrors
    Rect(4, -12.6f, 3, 2.4f, A(p.trim)); Rect(4, 10.2f, 3, 2.4f, A(p.trim));
    // glass: wraparound windshield, side windows, rear hatch
    const Color glass = A(Hex(0x161b22));
    Poly({{11.5f, -7.2f}, {11.5f, 7.2f}, {3, 8.8f}, {3, -8.8f}}, glass);
    Poly({{3, -8.8f}, {-9, -8.4f}, {-13.5f, -6.8f}, {-13.5f, 6.8f}, {-9, 8.4f}, {3, 8.8f}}, glass);
    // roof panel over the glass
    draw::FillRoundRect(-8.5f, -7.2f, 11, 14.4f, 2, A(p.roof));
    Rect(9, -6, 1.5f, 5, A(Rgba(255, 255, 255, .18f)));   // windshield glint
    // twin stripes on hood, roof and deck
    for (auto [x0, x1] : {std::pair<float, float>{11.5f, 24.4f}, {-8.5f, 2.5f}, {-23.2f, -13.5f}}) {
        Rect(x0, -3.4f, x1 - x0, 2, A(p.stripe)); Rect(x0, 1.4f, x1 - x0, 2, A(p.stripe));
    }
    // hood vents
    Rect(15, -7, 5, 1.4f, A(p.vent)); Rect(15, 5.6f, 5, 1.4f, A(p.vent));
    // slim headlights
    Rect(22.3f, -8, 2, 3.2f, A(Hex(0xfff3cc))); Rect(22.3f, 4.8f, 2, 3.2f, A(Hex(0xfff3cc)));
    // low lip spoiler on the tail
    Rect(-22.8f, -9.5f, 1.6f, 19, A(Hex(0x0e1215)));
    // tail lights
    const Color tail = A(braking ? Hex(0xff3b2f) : Hex(0x8a1f1a));
    Rect(-24, -9, 1.3f, 4.5f, tail); Rect(-24, 4.5f, 1.3f, 4.5f, tail);
    Pop();
}

// ---------- scenery with fake perspective ----------
void Game::DrawBlock(const Scen& b) {
    const double k = b.ht;
    auto P = [&](double px, double py) { return F(px + (px - cam.x) * k, py + (py - cam.y) * k); };
    const Vector2 c[4] = {F(b.x, b.y), F(b.x + b.w, b.y), F(b.x + b.w, b.y + b.h), F(b.x, b.y + b.h)};
    const Vector2 r[4] = {P(b.x, b.y), P(b.x + b.w, b.y), P(b.x + b.w, b.y + b.h), P(b.x, b.y + b.h)};
    const double wallShades[4] = {0.55, 0.72, 0.62, 0.8};
    for (int i = 0; i < 4; i++) {
        int j = (i + 1) % 4;
        const Vector2 q[4] = {c[i], c[j], r[j], r[i]};
        draw::FillConvex(q, 4, Shade(b.col, wallShades[i]));
    }
    draw::FillConvex(r, 4, Hex(b.col));
    const float rw = r[1].x - r[0].x, rh = r[3].y - r[0].y;
    draw::StrokeRect(r[0].x + 6, r[0].y + 6, rw - 12, rh - 12, b.hasAcc ? 4 : 3, b.hasAcc ? Hex(b.acc) : Shade(b.col, 0.8));
    if (b.pool) {   // rooftop pool with a deck
        float pw = rw * 0.5f, ph = rh * 0.3f, px = r[0].x + rw * (0.15f + (float)b.s * 0.2f), py = r[0].y + rh * (0.18f + (float)std::fmod(b.s * 5, 1) * 0.4f);
        Rect(px - 6, py - 6, pw + 12, ph + 12, Hex(0xf5efe2));
        Rect(px, py, pw, ph, Hex(0x4fd0e3));
        Rect(px + pw * 0.1f, py + ph * 0.2f, pw * 0.5f, 3, Rgba(255, 255, 255, .35f));
        Rect(px + pw * 0.35f, py + ph * 0.6f, pw * 0.45f, 3, Rgba(255, 255, 255, .35f));
    }
    const Color vc = Shade(b.col, 0.7);
    for (int v = 0; v < b.vents; v++) {
        float vx = r[0].x + rw * (0.2f + (float)std::fmod(b.s * 7 + v * 0.31, 0.6)), vy = r[0].y + rh * (0.2f + (float)std::fmod(b.s * 3 + v * 0.43, 0.6));
        Rect(vx, vy, 14, 14, vc);
    }
}

void Game::DrawBuildingPoly(const Scen& b) {
    const double k = b.ht;
    Vector2 c[4], r[4];
    for (int i = 0; i < 4; i++) { c[i] = F(b.pts[i]); r[i] = F(b.pts[i].x + (b.pts[i].x - cam.x) * k, b.pts[i].y + (b.pts[i].y - cam.y) * k); }
    for (int i = 0; i < 4; i++) {   // walls, shaded by which way they face
        int j = (i + 1) % 4;
        double ex = c[j].x - c[i].x, ey = c[j].y - c[i].y, L = Hypot(ex, ey);
        if (L == 0) L = 1;
        double nx = ey / L, ny = -ex / L;
        const Vector2 q[4] = {c[i], c[j], r[j], r[i]};
        draw::FillConvex(q, 4, Shade(b.col, 0.67 - 0.04 * nx - (ny < 0 ? -0.12 * ny : 0.03 * ny)));
    }
    draw::FillConvex(r, 4, Hex(b.col));
    auto Q = [&](double u, double v) {
        double ax = r[0].x + (r[1].x - r[0].x) * u, ay = r[0].y + (r[1].y - r[0].y) * u;
        double bx = r[3].x + (r[2].x - r[3].x) * u, by = r[3].y + (r[2].y - r[3].y) * u;
        return F(ax + (bx - ax) * v, ay + (by - ay) * v);
    };
    const double iu = std::min(0.12, 7 / (b.hw * 2)), iv = std::min(0.12, 7 / (b.hh * 2));
    const Vector2 in[4] = {Q(iu, iv), Q(1 - iu, iv), Q(1 - iu, 1 - iv), Q(iu, 1 - iv)};
    draw::StrokePoly(in, 4, b.hasAcc ? 4 : 3, b.hasAcc ? Hex(b.acc) : Shade(b.col, 0.8));
    if (b.pool) {
        double u0 = 0.18 + b.s * 0.14, u1 = u0 + 0.46, v0 = 0.2 + std::fmod(b.s * 5, 1) * 0.3, v1 = v0 + 0.34;
        Poly({Q(u0 - 0.05, v0 - 0.06), Q(u1 + 0.05, v0 - 0.06), Q(u1 + 0.05, v1 + 0.06), Q(u0 - 0.05, v1 + 0.06)}, Hex(0xf5efe2));
        Poly({Q(u0, v0), Q(u1, v0), Q(u1, v1), Q(u0, v1)}, Hex(0x4fd0e3));
    }
    const Color vc = Shade(b.col, 0.7);
    for (int v = 0; v < b.vents; v++) {
        double u = 0.2 + std::fmod(b.s * 7 + v * 0.31, 0.6), w = 0.2 + std::fmod(b.s * 3 + v * 0.43, 0.6);
        Poly({Q(u, w), Q(u + 0.1, w), Q(u + 0.1, w + 0.1), Q(u, w + 0.1)}, vc);
    }
}

void Game::DrawPalm(const Scen& p) {
    const float ox = (float)(p.x + (p.x - cam.x) * p.ht), oy = (float)(p.y + (p.y - cam.y) * p.ht);
    draw::FillEllipse((float)p.x + 7, (float)p.y + 9, (float)p.r * 0.55f, (float)p.r * 0.4f, 0.6f, Rgba(0, 0, 0, .12f));
    draw::Line((float)p.x, (float)p.y, ox, oy, 6, Hex(0x8a6a45), draw::ROUND);
    std::vector<Vector2> leaf;
    for (int i = 0; i < p.n; i++) {
        const double a = p.a + i * PI * 2 / p.n, ca = std::cos(a), sa = std::sin(a), L = p.r;
        leaf.clear();
        const Vector2 o{ox, oy}, tip = F(ox + ca * L, oy + sa * L);
        leaf.push_back(o);
        draw::QuadTo(leaf, o, F(ox + ca * L * .5 - sa * L * .3, oy + sa * L * .5 + ca * L * .3), tip, 6);
        draw::QuadTo(leaf, tip, F(ox + ca * L * .5 + sa * L * .1, oy + sa * L * .5 - ca * L * .1), o, 6);
        leaf.pop_back();
        draw::FillFan(o, leaf.data() + 1, (int)leaf.size() - 1, i % 2 ? Hex(p.col) : Shade(p.col, 0.8));
    }
    draw::FillCircle(ox, oy, 4.5f, Hex(0x6b4a2a));
}

void Game::DrawUmb(const Scen& u) {
    Push(u.x, u.y, u.a);
    Rect((float)u.r * 0.3f, -9, (float)u.r * 1.4f, 18, Hex(u.towel));
    Pop();
    const float ox = (float)(u.x + (u.x - cam.x) * u.ht), oy = (float)(u.y + (u.y - cam.y) * u.ht);
    draw::FillCircle((float)u.x + 6, (float)u.y + 8, (float)u.r, Rgba(0, 0, 0, .18f));
    for (int i = 0; i < 8; i++)
        draw::FillSector(ox, oy, (float)u.r, (float)(u.a + i * PI / 4), (float)(u.a + (i + 1) * PI / 4), Hex(i % 2 ? u.col2 : u.col));
    draw::FillCircle(ox, oy, 2.5f, WHITE);
}

// ---------- ground ----------
static void DrawMiamiGround(Game& g, double hw, double hh) {
    const int TILE = Miami::TILE, MW = Miami::MW, MH = Miami::MH, BLOCK = Miami::BLOCK, ROADW = Miami::ROADW;
    const int x0 = std::max(0, (int)std::floor((g.cam.x - hw) / TILE)), x1 = std::min(MW - 1, (int)std::floor((g.cam.x + hw) / TILE));
    const int y0 = std::max(0, (int)std::floor((g.cam.y - hh) / TILE)), y1 = std::min(MH - 1, (int)std::floor((g.cam.y + hh) / TILE));
    if (x0 > x1 || y0 > y1) return;
    const double now = g.now;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            const int t = g.miami.Tile(x, y);
            const float X = (float)x * TILE, Y = (float)y * TILE;
            Rect(X, Y, TILE + 1, TILE + 1, Hex(t == WATER && x >= Miami::SEA_X ? (x == Miami::SEA_X ? 0x3fbfd2 : 0x2aa3c8) : SurfaceColor(t)));
            if (t == WALK) draw::StrokeRect(X + .5f, Y + .5f, TILE - 1, TILE - 1, 1, Rgba(0, 0, 0, .08f));
            else if (t == SAND && x == Miami::SEA_X - 1) Rect(X + TILE * 0.55f, Y, TILE * 0.45f + 1, TILE + 1, Hex(0xdcc38e));
            else if (t == WATER && (x * 5 + y * 3) % 4 == 0) {   // ripples
                float o = (float)std::sin(now * 0.8 + x + y * 1.7) * 8;
                draw::StrokeArc(X + 32 + o, Y + 40, 12, PI * 1.15f, PI * 1.85f, 2, Rgba(255, 255, 255, .16f));
            } else if (t == ROAD && x < Miami::BAY_X) {   // causeway railings
                if (y % BLOCK == 0) Rect(X, Y, TILE + 1, 5, Hex(0xefe9dc));
                if (y % BLOCK == ROADW - 1) Rect(X, Y + TILE - 5, TILE + 1, 5, Hex(0xefe9dc));
            }
        }
    // seawall along the bay, surf along the beach
    Rect((float)Miami::BAY_X * TILE - 4, (float)y0 * TILE, 5, (float)(y1 - y0 + 1) * TILE, Hex(0xefe9dc));
    if (x1 >= Miami::SEA_X - 1) {
        const double waves[2][3] = {{4, .75, 0}, {26, .35, 2.1}};
        for (auto& wv : waves) {
            std::vector<Vector2> pts;
            for (int yy = y0 * TILE; yy <= (y1 + 1) * TILE; yy += 12)
                pts.push_back(F(Miami::SEA_X * TILE + wv[0] + std::sin(yy * 0.025 + now * 1.3 + wv[2]) * 6 + std::sin(now * 0.9 + wv[2]) * 5, yy));
            draw::Polyline(pts.data(), (int)pts.size(), false, 5, Rgba(255, 255, 255, (float)wv[1]), draw::ROUND, false);
        }
    }
    // road markings
    const Color yel = Hex(0xe8c85a), park = Rgba(240, 236, 220, .55f);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            const int bx = x % BLOCK, by = y % BLOCK, t = g.miami.Tile(x, y);
            const float X = (float)x * TILE, Y = (float)y * TILE;
            if (t == ROAD && bx == 0 && by >= ROADW) for (int k = 0; k < 2; k++) Rect(X + TILE - 2, Y + k * 32 + 6, 4, 18, yel);
            if (t == ROAD && by == 0 && (bx >= ROADW || x < Miami::BAY_X)) for (int k = 0; k < 2; k++) Rect(X + k * 32 + 6, Y + TILE - 2, 18, 4, yel);
            if (t == LOT && ((x - ROADW) % BLOCK + BLOCK) % BLOCK % 2 == 0) Rect(X, Y + 8, 3, 48, park);
        }
}

static void DrawEndlessGround(Game& g, double hw, double hh) {
    auto cells = g.city.CellsIn(g.cam.x - hw, g.cam.y - hh, g.cam.x + hw, g.cam.y + hh);
    Rect((float)(g.cam.x - hw), (float)(g.cam.y - hh), (float)hw * 2, (float)hh * 2, Hex(0xd6ccba));   // paving between streets
    for (CityCell* C : cells)
        for (auto& p : C->parks) { auto v = F(p.pts); draw::FillFan(F(p.ctr), v.data(), (int)v.size(), Hex(C_GRASS)); }
    for (CityCell* C : cells)
        for (auto& q : C->squares) {   // open asphalt squares with a painted drift circle
            auto v = F(q.pts);
            draw::FillFan(F(q.ctr), v.data(), (int)v.size(), Hex(0x45474e));
            draw::DashedCircle((float)q.ctr.x, (float)q.ctr.y, (float)q.r, 5, 26, 22, Rgba(240, 236, 220, .28f));
            draw::FillCircle((float)q.ctr.x, (float)q.ctr.y, 10, Rgba(240, 236, 220, .35f));
        }
    for (CityCell* C : cells)
        for (auto& l : C->lots) {
            Vector2 p[4] = {F(l.pts[0]), F(l.pts[1]), F(l.pts[2]), F(l.pts[3])};
            draw::FillConvex(p, 4, Hex(C_LOT));
            Push(l.cx, l.cy, l.ang);
            for (double x = -l.fw / 2 + 14; x < l.fw / 2 - 8; x += 28) {
                Rect((float)x, (float)(-l.dp / 2 + 6), 2.5f, (float)(l.dp * 0.36), Rgba(240, 236, 220, .55f));
                Rect((float)x, (float)(l.dp / 2 - 6 - l.dp * 0.36), 2.5f, (float)(l.dp * 0.36), Rgba(240, 236, 220, .55f));
            }
            Pop();
        }
    // all kerbs first, then all asphalt, so junctions merge into clean rounded corners
    std::vector<std::vector<Vector2>> paths;
    std::vector<const CityRoad*> roads;
    for (CityCell* C : cells) for (auto& r : C->roads) { roads.push_back(&r); paths.push_back(F(r.pts)); }
    for (size_t k = 0; k < roads.size(); k++) draw::Polyline(paths[k].data(), (int)paths[k].size(), roads[k]->closed, (float)roads[k]->w + 14, Hex(0xebe3d2));
    for (size_t k = 0; k < roads.size(); k++) draw::Polyline(paths[k].data(), (int)paths[k].size(), roads[k]->closed, (float)roads[k]->w, Hex(C_ROAD));
    for (size_t k = 0; k < roads.size(); k++)
        if (roads[k]->main) draw::DashedPolyline(paths[k].data(), (int)paths[k].size(), false, 4, 34, 30, Hex(0xe8c85a), draw::ROUND);
    for (CityCell* C : cells)
        for (auto& s : C->islands) {
            draw::FillCircle((float)s.x, (float)s.y, (float)s.r, Hex(C_GRASS));
            draw::StrokeCircle((float)s.x, (float)s.y, (float)s.r, 6, Hex(0xebe3d2));
        }
}

static void DrawSkids(const std::vector<SkidMark>& skids, Color c) {
    for (const SkidMark& s : skids) draw::Line(s.x0, s.y0, s.x1, s.y1, 4, c);
}

// ---------- scenes ----------
static void RenderCity(Game& g, double hw, double hh) {
    if (g.endless) DrawEndlessGround(g, hw, hh);
    else DrawMiamiGround(g, hw, hh);
    DrawSkids(g.skids, Rgba(18, 18, 20, .38f));
    if (g.surv) g.SurvDrawGround(hw, hh);
    g.DrawCar(g.car.x, g.car.y, g.car.a, g.car.w, g.BrakeIn() > 0.1, g.paint);
    const bool toxic = g.surv && g.surv->up[U_TOXIC];
    for (const SmokePuff& s : g.smoke)
        draw::FillCircle((float)s.x, (float)s.y, (float)s.r, toxic ? Rgba(170, 230, 120, (float)s.life * 0.3f) : Rgba(225, 222, 215, (float)s.life * 0.28f));
    if (g.surv) g.SurvDrawFx();

    // buildings, palms and umbrellas with fake perspective: tops pushed away from the camera, far ones first
    std::vector<std::pair<double, const Scen*>> vis;
    auto add = [&](const Scen& d) {
        if (std::fabs(d.cx - g.cam.x) < hw + 300 && std::fabs(d.cy - g.cam.y) < hh + 300) vis.push_back({Hypot(d.cx - g.cam.x, d.cy - g.cam.y), &d});
    };
    if (g.endless) { for (CityCell* C : g.city.CellsIn(g.cam.x - hw, g.cam.y - hh, g.cam.x + hw, g.cam.y + hh)) for (auto& d : C->scenery) add(d); }
    else for (auto& d : g.miami.scenery) add(d);
    std::stable_sort(vis.begin(), vis.end(), [](auto& a, auto& b) { return a.first > b.first; });
    for (auto& [dist, d] : vis) {
        if (d->t == SC_PALM) g.DrawPalm(*d);
        else if (d->t == SC_UMB) g.DrawUmb(*d);
        else if (d->t == SC_POLY) g.DrawBuildingPoly(*d);
        else g.DrawBlock(*d);
    }
}

static void AcrossBar(const Track& T, int q, Color col, float thick) {
    V2 p = T.pts[q], t = T.tan[q];
    double nx = -t.y, ny = t.x;
    draw::Line((float)(p.x - nx * T.half), (float)(p.y - ny * T.half), (float)(p.x + nx * T.half), (float)(p.y + ny * T.half), thick, col);
}

static void RenderTrack(Game& g, double hw, double hh) {
    const Track& T = *g.T;
    const Theme& th = *g.theme;
    const auto path = F(T.pts);
    const int N = (int)path.size();
    draw::Polyline(path.data(), N, true, (float)(T.edge * 2 + 22), Hex(th.barrier0));
    draw::DashedPolyline(path.data(), N, true, (float)(T.edge * 2 + 22), 30, 30, Hex(th.barrier1));
    draw::Polyline(path.data(), N, true, (float)(T.edge * 2), Hex(th.gravel));
    draw::Polyline(path.data(), N, true, (float)(T.w + 14), Hex(0xe9e5da));
    draw::DashedPolyline(path.data(), N, true, (float)(T.w + 14), 18, 18, Hex(0xc8352b));
    draw::Polyline(path.data(), N, true, (float)T.w, Hex(th.asph));
    for (const Zone& z : T.zones) {
        draw::Polyline(path.data() + z.a, z.b - z.a + 1, false, (float)T.w, Rgba(255, 207, 58, 0.08f), draw::BUTT, false);
        AcrossBar(T, z.a, Rgba(255, 207, 58, .85f), 7);
        AcrossBar(T, z.b, Rgba(255, 207, 58, .45f), 4);
    }
    {   // start / finish line
        V2 p = T.pts[0], t = T.tan[0];
        float sq = (float)T.w / 10;
        Push(p.x, p.y, std::atan2(t.y, t.x));
        for (int c = 0; c < 10; c++)
            for (int r = 0; r < 2; r++) Rect(r * sq - sq, (float)-T.half + c * sq, sq, sq, (c + r) % 2 ? Hex(0x111111) : Hex(0xeeeeee));
        Pop();
    }
    DrawSkids(g.skids, Rgba(18, 18, 20, .4f));
    // clipping points: dashed ring shows how close the rear has to get
    for (size_t zi = 0; zi < T.zones.size(); zi++)
        for (size_t ci = 0; ci < T.zones[zi].clips.size(); ci++) {
            const Clip& c = T.zones[zi].clips[ci];
            const bool hit = g.run && zi < g.run->clipHit.size() && g.run->clipHit[zi][ci];
            draw::DashedCircle((float)c.x, (float)c.y, (float)T.clipR, 2, 8, 8, hit ? Rgba(127, 224, 138, .5f) : Rgba(255, 255, 255, .22f));
            draw::FillCircle((float)c.x + 2, (float)c.y + 3, 10, Rgba(0, 0, 0, .3f));
            draw::FillCircle((float)c.x, (float)c.y, 10, hit ? Hex(0x4fd66a) : Hex(0xff7a1a));
            draw::FillCircle((float)c.x, (float)c.y, 4, WHITE);
        }
    if (g.run) {   // ghost of the best run
        double a;
        bool ok;
        V2 p = g.GhostAt(g.state == ST_COUNT ? 0 : g.run->t, a, ok);
        if (ok) {
            static const Paint ghostPaint = MakePaint(0xbfe9ff);
            g.DrawCar(p.x, p.y, a, 0, false, ghostPaint, 0.42f);
        }
    }
    g.DrawCar(g.car.x, g.car.y, g.car.a, g.car.w, g.BrakeIn() > 0.1, g.paint);
    for (const SmokePuff& s : g.smoke) draw::FillCircle((float)s.x, (float)s.y, (float)s.r, Rgba(225, 222, 215, (float)s.life * 0.28f));
    std::vector<std::pair<double, const Scen*>> vis;
    for (const Scen& d : g.decos)
        if (std::fabs(d.cx - g.cam.x) < hw + 200 && std::fabs(d.cy - g.cam.y) < hh + 200) vis.push_back({Hypot(d.cx - g.cam.x, d.cy - g.cam.y), &d});
    std::stable_sort(vis.begin(), vis.end(), [](auto& a, auto& b) { return a.first > b.first; });
    for (auto& [dist, dp] : vis) {
        const Scen& d = *dp;
        if (d.t == SC_BLOCK) { g.DrawBlock(d); continue; }
        if (d.t == SC_PALM) { g.DrawPalm(d); continue; }
        const float x = (float)d.x, y = (float)d.y, r = (float)d.r;
        const float ox = (float)(d.x + (d.x - g.cam.x) * d.ht), oy = (float)(d.y + (d.y - g.cam.y) * d.ht);
        draw::FillCircle(x + 8, y + 10, r, Rgba(0, 0, 0, .28f));
        if (d.t == SC_TREE) {
            draw::FillCircle(ox, oy, r, Hex(d.col));
            draw::FillCircle(ox - r * .25f, oy - r * .25f, r * .55f, Mix(Hex(d.col), WHITE, .12f));
        } else if (d.t == SC_ROCK) {
            draw::FillCircle(x, y, r, Mix(Hex(d.col), BLACK, .25f));
            draw::FillCircle(ox, oy, r * .85f, Hex(d.col));
        } else {
            draw::FillCircle(x, y, r, Hex(0x161616));
            draw::FillCircle(ox, oy, r, Hex(0x232323));
            draw::StrokeCircle(ox, oy, r * .5f, 3, Hex(0x4a4a4a));
        }
    }
}

void Game::Render(double speed) {
    const float Wc = W / S, Hc = H / S;   // CSS-sized view
    DrawRectangle(0, 0, (int)W, (int)H, Hex(mode == MODE_TRACK ? theme->out : SEA));
    const double dt = std::min(1.0 / 30, (double)GetFrameTime());
    const double scaleBase = std::min(1.15, std::max(0.75, std::min(Wc, Hc) / 620.0)) * (mode == MODE_TRACK ? 0.8 : surv ? 0.85 : 1);
    const double tz = scaleBase * (1.1 - std::min(0.38, speed / 1500));
    const double kz = 1 - std::pow(1 - 0.04, dt * 60), kp = 1 - std::pow(1 - 0.1, dt * 60);   // per-frame easing at 60 fps
    cam.z += (tz - cam.z) * kz;
    const double tx = car.x + car.vx * 0.35, ty = car.y + car.vy * 0.35;
    cam.x += (tx - cam.x) * kp; cam.y += (ty - cam.y) * kp;
    const double sx = (std::rand() / (double)RAND_MAX - .5) * shake, sy = (std::rand() / (double)RAND_MAX - .5) * shake;

    Camera2D c{};
    c.offset = {(float)(W / 2 + sx * S), (float)(H / 2 + sy * S)};
    c.target = {(float)cam.x, (float)cam.y};
    c.zoom = (float)(cam.z * S);
    BeginMode2D(c);
    draw::pxPerUnit = c.zoom;
    const double hw = Wc / 2 / cam.z + Miami::TILE, hh = Hc / 2 / cam.z + Miami::TILE;
    if (mode == MODE_CITY) RenderCity(*this, hw, hh);
    else RenderTrack(*this, hw, hh);
    EndMode2D();
    draw::pxPerUnit = 1;
    if (surv) SurvOverlay();
    if (mode == MODE_TRACK && theme->night) {   // headlights in the dark
        const double fx = car.x + std::cos(car.a) * 70, fy = car.y + std::sin(car.a) * 70;
        const float sx2 = (float)(W / 2 + (fx - cam.x) * cam.z * S), sy2 = (float)(H / 2 + (fy - cam.y) * cam.z * S);
        draw::RadialGradient(sx2, sy2, (float)(70 * cam.z * S), (float)(480 * cam.z * S), Rgba(4, 6, 14, 0), Rgba(4, 6, 14, 0.86f), (W + H) * 2);
    }
}

// ---------- minimap ----------
void Game::BuildMini() {
    if (!miniOk) return;
    BeginTextureMode(miniBg);
    ClearBackground(BLANK);
    BeginTextureBlend();
    draw::pxPerUnit = 1;
    if (mode == MODE_CITY) {
        const double s = 260 / Miami::WORLD_W;
        mm = {s, 10, 10};
        const uint32_t MC[7] = {0x55585e, 0xb3aa9a, 0x5c9a45, 0x55585e, 0x55585e, 0xeedba9, 0x2a9cc2};
        const double ts = Miami::TILE * s;
        for (int y = 0; y < Miami::MH; y++)
            for (int x = 0; x < Miami::MW; x++) Rect((float)(10 + x * ts), (float)(10 + y * ts), (float)ts + .5f, (float)ts + .5f, Hex(MC[miami.Tile(x, y)]));
        for (int bi : miami.buildings) {
            const Scen& b = miami.scenery[bi];
            Rect((float)(10 + b.x * s), (float)(10 + b.y * s), (float)(b.w * s), (float)(b.h * s), Shade(b.col, 0.75));
        }
    } else {
        double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
        for (V2 p : T->pts) { x0 = std::min(x0, p.x); y0 = std::min(y0, p.y); x1 = std::max(x1, p.x); y1 = std::max(y1, p.y); }
        const double pad = T->edge + 60, s = std::min(250 / (x1 - x0 + 2 * pad), 250 / (y1 - y0 + 2 * pad));
        mm = {s, 140 - (x0 + x1) / 2 * s, 140 - (y0 + y1) / 2 * s};
        Push(mm.ox, mm.oy, 0, s);
        draw::pxPerUnit = (float)s;
        const auto path = F(T->pts);
        const float w = (float)std::max(T->w, 5 / s);
        draw::Polyline(path.data(), (int)path.size(), true, w, Rgba(243, 234, 210, .75f), draw::ROUND, false);
        for (const Zone& z : T->zones) draw::Polyline(path.data() + z.a, z.b - z.a + 1, false, w * 0.55f, Hex(0xffcf3a), draw::BUTT, false);
        V2 p = T->pts[0], t = T->tan[0];
        draw::Line((float)(p.x + t.y * T->w * 1.2), (float)(p.y - t.x * T->w * 1.2), (float)(p.x - t.y * T->w * 1.2), (float)(p.y + t.x * T->w * 1.2), (float)(5 / s), WHITE);
        Pop();
        draw::pxPerUnit = 1;
    }
    EndBlendMode();
    EndTextureMode();
}

void Game::BuildRadar(int ci, int cj) {   // endless city: a pre-drawn patch of streets around the car
    const double CH = EndlessCity::CH, RS = 0.05;
    const int n = 7;
    const int size = (int)std::ceil(n * CH * RS);
    if (!radarBg.id) radarBg = LoadRenderTexture(size, size);
    radarO[0] = (ci - 3) * CH; radarO[1] = (cj - 3) * CH;
    BeginTextureMode(radarBg);
    ClearBackground(Hex(0xb3aa9a));
    BeginTextureBlend();
    Push(-radarO[0] * RS, -radarO[1] * RS, 0, RS);
    draw::pxPerUnit = (float)RS;
    auto cells = city.CellsIn(radarO[0], radarO[1], radarO[0] + n * CH, radarO[1] + n * CH);
    for (CityCell* C : cells) for (auto& p : C->parks) { auto v = F(p.pts); draw::FillFan(F(p.ctr), v.data(), (int)v.size(), Hex(0x5c9a45)); }
    for (CityCell* C : cells) for (auto& q : C->squares) { auto v = F(q.pts); draw::FillFan(F(q.ctr), v.data(), (int)v.size(), Hex(0x55585e)); }
    for (CityCell* C : cells) for (auto& l : C->lots) { Vector2 p[4] = {F(l.pts[0]), F(l.pts[1]), F(l.pts[2]), F(l.pts[3])}; draw::FillConvex(p, 4, Hex(0x55585e)); }
    for (CityCell* C : cells) for (auto& r : C->roads) { auto v = F(r.pts); draw::Polyline(v.data(), (int)v.size(), r.closed, (float)r.w, Hex(0x55585e)); }
    for (CityCell* C : cells)
        for (auto& b : C->scenery) if (b.t == SC_POLY) { Vector2 p[4] = {F(b.pts[0]), F(b.pts[1]), F(b.pts[2]), F(b.pts[3])}; draw::FillConvex(p, 4, Shade(b.col, 0.75)); }
    Pop();
    draw::pxPerUnit = 1;
    EndBlendMode();
    EndTextureMode();
}

void Game::DrawMini(Rectangle r) {
    const float f = r.width / 280;
    if (mode == MODE_CITY && endless) {
        const int ci = (int)std::floor(car.x / EndlessCity::CH), cj = (int)std::floor(car.y / EndlessCity::CH);
        const long long key = ((long long)ci << 32) ^ (unsigned)cj;
        if (key != radarKey) {
            radarKey = key;
            rlDrawRenderBatchActive();
            BuildRadar(ci, cj);
        }
        mm = {0.05, 140 - car.x * 0.05, 140 - car.y * 0.05};
    }
    BeginScissorMode((int)(r.x + 10 * f), (int)(r.y + 10 * f), (int)(260 * f), (int)(260 * f));
    if (mode == MODE_CITY && endless) {
        const float x = (float)(radarO[0] * 0.05 + mm.ox), y = (float)(radarO[1] * 0.05 + mm.oy);
        DrawTextureRT(radarBg, r.x + x * f, r.y + y * f, radarBg.texture.width * f, radarBg.texture.height * f);
    }
    EndScissorMode();
    if (!(mode == MODE_CITY && endless) && miniOk) DrawTextureRT(miniBg, r.x, r.y, r.width, r.height);
    Push(r.x, r.y, 0, f);
    if (surv) {
        rlDrawRenderBatchActive();
        BeginScissorMode((int)(r.x + 10 * f), (int)(r.y + 10 * f), (int)(260 * f), (int)(260 * f));
        for (const Enemy& e : surv->enemies) {
            float s = e.type == E_BOSS ? 5.0f : 2.0f;
            Rect((float)(e.x * mm.s + mm.ox) - s / 2, (float)(e.y * mm.s + mm.oy) - s / 2, s, s, Hex(0xff4f4f));
        }
        rlDrawRenderBatchActive();
        EndScissorMode();
    }
    if (mode == MODE_TRACK && run) {
        double a;
        bool ok;
        V2 gp = GhostAt(state == ST_COUNT ? 0 : run->t, a, ok);
        if (ok) draw::FillCircle((float)(gp.x * mm.s + mm.ox), (float)(gp.y * mm.s + mm.oy), 6, Rgba(191, 233, 255, .85f));
    }
    Push(car.x * mm.s + mm.ox, car.y * mm.s + mm.oy, car.a, 1.4);
    const Vector2 arrow[4] = {{12, 0}, {-8, -8}, {-4, 0}, {-8, 8}};
    draw::FillFan({0, 0}, arrow, 4, paint.base);
    draw::StrokePoly(arrow, 4, 2.5f, WHITE);
    Pop();
    Pop();
}

// ---------- menu previews ----------
void Game::DrawGarageCar(Rectangle r) {   // showroom: the car on a perspective floor grid
    const float s = r.width / 720, W2 = 360, H2 = 180;
    Push(r.x, r.y, 0, s);
    draw::pxPerUnit = s;
    const Color grid = Rgba(47, 214, 255, .12f);
    for (int k = -10; k <= 10; k++) draw::Line(W2 + k * 30, H2 - 40, W2 + k * 110, 360, 1.5f, grid);
    for (int k = 0; k < 6; k++) { float y = H2 - 40 + std::pow(k / 5.0f, 1.6f) * (H2 + 40); draw::Line(0, y, 720, y, 1.5f, grid); }
    // soft light pool under the car
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < 64; i++) {
        float t0 = 2 * PI * i / 64, t1 = 2 * PI * (i + 1) / 64;
        Color c0 = Rgba(255, 255, 255, .15f), c1 = Rgba(255, 255, 255, 0);
        rlColor4ub(c0.r, c0.g, c0.b, c0.a); rlVertex2f(W2, H2 + 30);
        rlColor4ub(c1.r, c1.g, c1.b, c1.a); rlVertex2f(W2 + std::cos(t0) * W2 * 0.75f, H2 + 30 + std::sin(t0) * H2 * 0.55f);
        rlColor4ub(c1.r, c1.g, c1.b, c1.a); rlVertex2f(W2 + std::cos(t1) * W2 * 0.75f, H2 + 30 + std::sin(t1) * H2 * 0.55f);
    }
    rlEnd();
    Pop();
    Push(r.x + W2 * s, r.y + (H2 + 10) * s, 0, 5.6 * s);
    draw::pxPerUnit = 5.6f * s;
    DrawCar(0, 0, -0.32, 0, false, paint);
    Pop();
    draw::pxPerUnit = 1;
}

void Game::DrawTrackPreview(int i, Rectangle r) {   // stage select: the track outline with its drift zones
    static std::vector<std::unique_ptr<Track>> cache(STAGES.size());
    if (!cache[i]) { cache[i] = std::make_unique<Track>(BuildTrack(STAGES[i])); StageGoals(*cache[i], i); }
    const Track& T = *cache[i];
    double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (V2 p : T.pts) { x0 = std::min(x0, p.x); y0 = std::min(y0, p.y); x1 = std::max(x1, p.x); y1 = std::max(y1, p.y); }
    const double k = r.width / 520;   // the web preview canvas was 520 px wide
    const double pad = T.w, sc = std::min((r.width - 40 * k) / (x1 - x0 + pad * 2), (r.height - 40 * k) / (y1 - y0 + pad * 2));
    Push(r.x + r.width / 2 - (x0 + x1) / 2 * sc, r.y + r.height / 2 - (y0 + y1) / 2 * sc, 0, sc);
    draw::pxPerUnit = (float)sc;
    const auto path = F(T.pts);
    const int N = (int)path.size();
    draw::Polyline(path.data(), N, true, (float)(T.w + 30 * k / sc), Rgba(47, 214, 255, .25f), draw::ROUND, false);
    draw::Polyline(path.data(), N, true, (float)(T.w * 0.55), Hex(0xe9edf5));
    for (const Zone& z : T.zones) draw::Polyline(path.data() + z.a, z.b - z.a + 1, false, (float)(T.w * 0.55), Hex(0xffcf3a), draw::ROUND);
    V2 p = T.pts[0], t = T.tan[0];
    draw::Line((float)(p.x + t.y * T.w), (float)(p.y - t.x * T.w), (float)(p.x - t.y * T.w), (float)(p.y + t.x * T.w), (float)(8 * k / sc), Hex(0xff4f4f), draw::ROUND);
    Pop();
    draw::pxPerUnit = 1;
}
