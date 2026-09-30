#include "desert_scene.h"
#include "game.h"
#include "rlgl.h"
#include "desert.h"
#include "terrain.h"
#include <algorithm>
#include <map>

namespace {
// ---------- the looks ----------
struct Pal {
    uint32_t light, shadow, low, road, runoff, tint, shadowCol;
    float shadowA;
    double dir, len;
    uint32_t gTop; float aTop; uint32_t gBot; float aBot; float vig;
    uint32_t dark; float darkness; bool lights;
};
// dir: the way shadows fall (0 = east, pi/2 = south); len: shadow length per unit of height
const std::map<std::string, Pal> PALS = {
    {"dawn",      {0xf2c9a8, 0xc98f8a, 0xd9a894, 0xf7e4cf, 0xdfb39c, 0xffe6d8, 0x5a3a52, 0.22f, 3.0, 2.2, 0xffc9b0, 0.16f, 0x8a7cc4, 0.14f, 0.18f, 0, 0, false}},
    {"morning",   {0xf3dcb0, 0xd2a877, 0xdeb98f, 0xf8ead0, 0xe2c29a, 0xfff3e2, 0x5a4030, 0.20f, 2.5, 1.3, 0xfff0d8, 0.10f, 0xc8a878, 0.06f, 0.14f, 0, 0, false}},
    {"noon",      {0xf5e6c4, 0xdcc08f, 0xe6d0a4, 0xfaf0dc, 0xe8d3aa, 0xffffff, 0x5a4630, 0.25f, 1.9, 0.5, 0xfff8e8, 0.10f, 0xe8d0a0, 0.08f, 0.12f, 0, 0, false}},
    {"afternoon", {0xf0cf95, 0xc9955f, 0xd9ad76, 0xf6e2bf, 0xdcb27e, 0xfff0d6, 0x5a3a24, 0.24f, 0.3, 1.2, 0xffe7b8, 0.12f, 0xb98a5a, 0.10f, 0.15f, 0, 0, false}},
    {"sunset",    {0xf7b58a, 0xb86a6a, 0xd88e7a, 0xfbd4b4, 0xe39e84, 0xffcfb0, 0x4a2a44, 0.30f, 0.1, 2.6, 0xff9d6e, 0.22f, 0x6a4c9c, 0.22f, 0.22f, 0x1a1030, 0.0f, true}},
    {"storm",     {0xd8b98a, 0xb0915f, 0xc4a577, 0xe6d2ae, 0xccb088, 0xecdcc0, 0x5a4a30, 0.10f, 1.2, 0.6, 0xe0c090, 0.20f, 0xa88858, 0.20f, 0.25f, 0, 0, false}},
    {"night",     {0x6f7fa8, 0x3c4466, 0x505a82, 0x8a95b8, 0x5c6690, 0x8fa0d0, 0x101428, 0.30f, 2.2, 1.6, 0x2a3060, 0.15f, 0x101428, 0.20f, 0.30f, 0x070a1c, 0.72f, true}},
    {"predawn",   {0x9a94bf, 0x5f5d8a, 0x7a76a6, 0xb4add0, 0x8783b0, 0xb8b0dc, 0x2a2448, 0.22f, 3.0, 2.0, 0xf0a8b8, 0.14f, 0x3a3868, 0.20f, 0.26f, 0x1a1638, 0.45f, true}},
};

Color Lerp(Color a, Color b, float t) { return Mix(a, b, Clamp(t, 0, 1)); }
double Smooth01(double t) { t = Clamp(t, 0, 1); return t * t * (3 - 2 * t); }
Color Scale(Color c, float k) { return {(unsigned char)Clamp(c.r * k, 0, 255), (unsigned char)Clamp(c.g * k, 0, 255), (unsigned char)Clamp(c.b * k, 0, 255), c.a}; }
inline Vector2 F(double x, double y) { return {(float)x, (float)y}; }

// ---------- scenery drawing ----------
struct Ctx {
    const Look& lk;
    const Game& g;
    double now;
    Color Tint(uint32_t hex, float k = 1) const {   // a colour under this light
        const Color c = Hex(hex);
        return {(unsigned char)Clamp(c.r * lk.light.r / 255.0 * k, 0, 255), (unsigned char)Clamp(c.g * lk.light.g / 255.0 * k, 0, 255),
                (unsigned char)Clamp(c.b * lk.light.b / 255.0 * k, 0, 255), 255};
    }
    V2 Top(double x, double y, double ht) const { return g.TopOf(x, y, ht); }   // fake perspective, or straight up in isometric
    V2 Shadow(double ht) const { return {std::cos(lk.shadowDir) * lk.shadowLen * ht * 100, std::sin(lk.shadowDir) * lk.shadowLen * ht * 100}; }
    Color ShadowCol(float k = 1) const { return Alpha(lk.shadow, k); }
};

// Round things seen from the side (a balloon, a lamp) must not be squashed by the isometric view:
// draw them at the origin between these, which undo the camera's turn and squash around (x, y).
void BeginUpright(const Ctx& C, double x, double y) {
    rlPushMatrix();
    rlTranslatef((float)x, (float)y, 0);
    if (C.g.up.x || C.g.up.y) { rlRotatef((float)-Game::ISO_ROT, 0, 0, 1); rlScalef(1, (float)(1 / Game::ISO_SQUASH), 1); }
}
void EndUpright() { rlPopMatrix(); }

// a lumpy round outline, the same every frame for a given seed
std::vector<Vector2> Blob(double x, double y, double r, double seed, int n = 10) {
    std::vector<Vector2> p(n);
    for (int i = 0; i < n; i++) {
        const double a = 2 * PI * i / n, k = 0.82 + 0.28 * (0.5 + 0.5 * std::sin(seed * 37.1 + i * 2.3) * std::cos(seed * 11.7 + i * 1.3));
        p[i] = F(x + std::cos(a) * r * k, y + std::sin(a) * r * k);
    }
    return p;
}
void FillBlob(const std::vector<Vector2>& p, double cx, double cy, Color c) { draw::FillFan(F(cx, cy), p.data(), (int)p.size(), c); }
// a large soft shape (water, salt): a circle with a few slow waves in its outline
std::vector<Vector2> Shore(double x, double y, double r, double seed) {
    std::vector<Vector2> p(64);
    for (int i = 0; i < 64; i++) {
        const double a = 2 * PI * i / 64, k = ShoreWave(a, seed);
        p[i] = F(x + std::cos(a) * r * k, y + std::sin(a) * r * k);
    }
    return p;
}

// a shadow cast along the sun from a round thing of radius r, reaching `len` away
void Capsule(const Ctx& C, double x, double y, double r, V2 off, Color c) {
    draw::Line((float)x, (float)y, (float)(x + off.x), (float)(y + off.y), (float)(r * 2), c, draw::ROUND);
}

void DrawRock(const Ctx& C, const Scen& d) {
    const V2 sh = C.Shadow(d.ht), top = C.Top(d.x, d.y, d.ht);
    FillBlob(Blob(d.x + sh.x * 0.6, d.y + sh.y * 0.6, d.r * 1.05, d.s), d.x + sh.x * 0.6, d.y + sh.y * 0.6, C.ShadowCol());
    FillBlob(Blob(d.x, d.y, d.r, d.s), d.x, d.y, C.Tint(d.col, 0.72f));
    FillBlob(Blob(top.x, top.y, d.r * 0.86, d.s), top.x, top.y, C.Tint(d.col));
    const double lx = -std::cos(C.lk.shadowDir) * d.r * 0.25, ly = -std::sin(C.lk.shadowDir) * d.r * 0.25;   // towards the sun
    FillBlob(Blob(top.x + lx, top.y + ly, d.r * 0.45, d.s + 3), top.x + lx, top.y + ly, C.Tint(d.col, 1.12f));
}

void DrawStack(const Ctx& C, const Scen& d, int layers, float taper, bool pale) {   // hoodoos and columns
    const V2 sh = C.Shadow(d.ht), top = C.Top(d.x, d.y, d.ht);
    Capsule(C, d.x, d.y, d.r * 0.8, sh, C.ShadowCol());
    // the body: a solid tapered pillar from the ground to the top, with the banded layers over it
    const double bx = top.x - d.x, by = top.y - d.y, bl = std::max(1.0, Hypot(bx, by)), nx = -by / bl, ny = bx / bl;
    const double r0 = d.r * 0.92, r1 = d.r * (1 - taper) * 0.92;
    const Vector2 body[4] = {F(d.x + nx * r0, d.y + ny * r0), F(top.x + nx * r1, top.y + ny * r1), F(top.x - nx * r1, top.y - ny * r1), F(d.x - nx * r0, d.y - ny * r0)};
    draw::FillConvex(body, 4, C.Tint(d.col, pale ? 0.78f : 0.8f));
    draw::FillCircle((float)d.x, (float)d.y, (float)r0, C.Tint(d.col, pale ? 0.78f : 0.8f));
    for (int i = 0; i <= layers; i++) {
        const double l = (double)i / layers;
        const V2 p = C.Top(d.x, d.y, d.ht * l);
        const double r = d.r * (1 - taper * l) * (i == layers ? 1.15 : 1);
        const float band = pale ? (i == layers ? 1.08f : 0.8f + 0.2f * (float)l) : (i % 2 ? 0.86f : 0.97f) * (0.8f + 0.25f * (float)l);
        FillBlob(Blob(p.x, p.y, r, d.s + i), p.x, p.y, C.Tint(d.col, band));
    }
}

void DrawWall(const Ctx& C, const Scen& d) {
    const double c = std::cos(d.a), s = std::sin(d.a), hw = d.w / 2, hh = d.h / 2;
    V2 base[4], top[4];
    const double L[4][2] = {{-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}};
    for (int k = 0; k < 4; k++) { base[k] = {d.x + L[k][0] * c - L[k][1] * s, d.y + L[k][0] * s + L[k][1] * c}; top[k] = C.Top(base[k].x, base[k].y, d.ht); }
    const V2 sh = C.Shadow(d.ht);
    for (int k = 0; k < 4; k++) {   // shadow: the footprint swept along the sun
        const Vector2 q[4] = {F(base[k].x, base[k].y), F(base[(k + 1) % 4].x, base[(k + 1) % 4].y), F(base[(k + 1) % 4].x + sh.x, base[(k + 1) % 4].y + sh.y), F(base[k].x + sh.x, base[k].y + sh.y)};
        draw::FillConvex(q, 4, C.ShadowCol(0.5f));
    }
    for (int k = 0; k < 4; k++) {
        const int j = (k + 1) % 4;
        const Vector2 q[4] = {F(base[k].x, base[k].y), F(base[j].x, base[j].y), F(top[j].x, top[j].y), F(top[k].x, top[k].y)};
        draw::FillConvex(q, 4, C.Tint(d.col, 0.7f + 0.08f * k));
    }
    const Vector2 t[4] = {F(top[0].x, top[0].y), F(top[1].x, top[1].y), F(top[2].x, top[2].y), F(top[3].x, top[3].y)};
    draw::FillConvex(t, 4, C.Tint(d.col, 1.05f));
    // a broken bite out of one end
    const V2 e = C.Top(d.x + c * hw * 0.8, d.y + s * hw * 0.8, d.ht);
    draw::FillCircle((float)e.x, (float)e.y, (float)d.h * 0.7f, C.Tint(d.col, 0.8f));
}

void DrawLantern(const Ctx& C, const Scen& d) {
    const V2 top = C.Top(d.x, d.y, d.ht), sh = C.Shadow(d.ht);
    draw::Line((float)d.x, (float)d.y, (float)(d.x + sh.x), (float)(d.y + sh.y), 3, C.ShadowCol());
    draw::Line((float)d.x, (float)d.y, (float)top.x, (float)top.y, 3, C.Tint(d.col), draw::ROUND);
    BeginUpright(C, top.x, top.y);
    draw::FillCircle(0, 0, 6, C.lk.lights ? Hex(0xffd58a) : C.Tint(0xe8c890));
    draw::FillCircle(0, 0, 3, C.lk.lights ? Hex(0xfff3d0) : C.Tint(0xf8e8c8));
    EndUpright();
}

void DrawGrass(const Ctx& C, const Scen& d) {
    const Color c = C.Tint(d.col);
    for (int i = 0; i < d.n; i++) {
        const double a = d.a + i * 0.55 - d.n * 0.27, sway = std::sin(C.now * 1.6 + d.s * 9 + i) * 0.08;
        const double ex = d.x + std::cos(a + sway) * d.r, ey = d.y + std::sin(a + sway) * d.r * 0.7 - d.r * 0.3;
        draw::Line((float)d.x, (float)d.y, (float)ex, (float)ey, 2, c, draw::ROUND);
    }
}

void DrawBush(const Ctx& C, const Scen& d) {
    const V2 sh = C.Shadow(0.03);
    draw::FillCircle((float)(d.x + sh.x), (float)(d.y + sh.y), (float)d.r, C.ShadowCol(0.8f));
    for (int i = 0; i < 3; i++) {
        const double a = d.a + i * 2.1, o = d.r * 0.35;
        draw::FillCircle((float)(d.x + std::cos(a) * o), (float)(d.y + std::sin(a) * o), (float)(d.r * 0.7), C.Tint(d.col, 0.85f + 0.1f * i));
    }
}

void DrawFlags(const Ctx& C, const Scen& d) {
    const V2 a = C.Top(d.x, d.y, d.ht), b = C.Top(d.x2, d.y2, d.ht);
    const Color pole = C.Tint(0x6a5040);
    draw::Line((float)d.x, (float)d.y, (float)a.x, (float)a.y, 3, pole, draw::ROUND);
    draw::Line((float)d.x2, (float)d.y2, (float)b.x, (float)b.y, 3, pole, draw::ROUND);
    const uint32_t cols[5] = {0xe98a7a, 0xf2c46b, 0x7fb7c9, 0x9d8fd0, 0xf3e3c3};
    const int n = 9;
    std::vector<Vector2> line;
    for (int i = 0; i <= n; i++) {
        const double t = (double)i / n, sag = std::sin(t * PI) * 18;
        line.push_back(F(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t + sag));
    }
    draw::Polyline(line.data(), (int)line.size(), false, 1.5f, C.Tint(0x4a3c30), draw::BUTT, false);
    for (int i = 1; i < n; i++) {
        const Vector2 p = line[i];
        const float flap = (float)std::sin(C.now * 3 + i + d.s * 7) * 4;
        const Vector2 tri[3] = {{p.x - 5, p.y}, {p.x + 5, p.y}, {p.x + flap, p.y + 12}};
        draw::FillConvex(tri, 3, C.Tint(cols[(i + (int)(d.s * 5)) % 5]));
    }
}

void DrawPuffs(const Ctx& C, const std::vector<SmokePuff>& puffs, const Look& lk) {   // dust, water spray and wake rings
    for (const SmokePuff& s : puffs) {
        const float a = (float)std::max(0.0, s.life);
        if (s.kind == 2) draw::StrokeCircle((float)s.x, (float)s.y, (float)s.r, 2.5f, Alpha(C.Tint(0xffffff), 0.35f * a));
        else if (s.kind == 1) draw::FillCircle((float)s.x, (float)s.y, (float)s.r, Alpha(C.Tint(0xf4fbfb), 0.7f * a));
        else draw::FillCircle((float)s.x, (float)s.y, (float)s.r, Alpha(lk.dust, a * 0.35f));
    }
}

void DrawOasis(const Ctx& C, const Scen& d) {
    FillBlob(Shore(d.x, d.y, d.r + 22, d.s), d.x, d.y, C.Tint(0xb89a70, 0.9f));   // wet sand
    FillBlob(Shore(d.x, d.y, d.r, d.s), d.x, d.y, C.Tint(d.col));
    FillBlob(Shore(d.x - d.r * 0.1, d.y - d.r * 0.1, d.r * 0.72, d.s), d.x - d.r * 0.1, d.y - d.r * 0.1, C.Tint(d.col, 0.85f));   // deeper water
    for (int k = 0; k < 4; k++) {   // slow ripples
        const double t = std::fmod(C.now * 0.25 + k * 0.25, 1.0), rr = d.r * (0.2 + 0.6 * t);
        const double ox = std::sin(d.s * 13 + k) * d.r * 0.3, oy = std::cos(d.s * 7 + k) * d.r * 0.3;
        draw::StrokeCircle((float)(d.x + ox), (float)(d.y + oy), (float)rr, 2, Alpha(C.Tint(0xffffff), 0.25f * (1 - (float)t)));
    }
}

void DrawSalt(const Ctx& C, const Scen& d) {   // pale crust with fine cracks; drawn over the road so you see it coming
    FillBlob(Shore(d.x, d.y, d.r * 1.04, d.s), d.x, d.y, Alpha(C.Tint(d.col, 0.95f), 0.35f));
    FillBlob(Shore(d.x, d.y, d.r * 0.9, d.s + 1), d.x, d.y, Alpha(C.Tint(d.col), 0.6f));
    const Color crack = Alpha(C.Tint(0xb8ad9c), 0.5f);
    for (int k = 0; k < 60; k++) {   // short cracks in a loose cell pattern
        const double a = std::sin(d.s * 91 + k * 12.9898) * 43758.5453, b = std::sin(d.s * 57 + k * 78.233) * 12345.6789;
        const double ra = d.r * 0.85 * std::sqrt(a - std::floor(a)), aa = (b - std::floor(b)) * 2 * PI;
        const double x = d.x + std::cos(aa) * ra, y = d.y + std::sin(aa) * ra, dir = aa * 3.7 + k;
        draw::Line((float)x, (float)y, (float)(x + std::cos(dir) * 26), (float)(y + std::sin(dir) * 26), 1.4f, crack);
    }
}

void DrawPlate(const Ctx& C, const Scen& d) {   // a flat plate of weathered rock
    FillBlob(Shore(d.x, d.y, d.r, d.s), d.x, d.y, Alpha(C.Tint(d.col, 0.95f), 0.85f));
    FillBlob(Shore(d.x + d.r * 0.05, d.y - d.r * 0.05, d.r * 0.8, d.s + 2), d.x, d.y, Alpha(C.Tint(d.col, 1.06f), 0.6f));
    for (int k = 0; k < 18; k++) {   // cracks between the slabs
        const double a = d.s * 40 + k * 2.39996, rr = d.r * 0.85 * std::sqrt((k + 0.5) / 18.0);
        const double x = d.x + std::cos(a) * rr, y = d.y + std::sin(a) * rr, dir = a * 1.7;
        draw::Line((float)x, (float)y, (float)(x + std::cos(dir) * 40), (float)(y + std::sin(dir) * 40), 1.6f, Alpha(C.Tint(d.col, 0.7f), 0.6f));
    }
}

void DrawTrail(const Ctx& C, const Scen& d) {   // faint old tyre tracks curving across the sand
    const double mx = (d.x + d.x2) / 2, my = (d.y + d.y2) / 2, len = Hypot(d.x2 - d.x, d.y2 - d.y);
    const double nx = -(d.y2 - d.y) / len, ny = (d.x2 - d.x) / len, bend = d.a * len * 0.5;
    for (double side : {-8.0, 8.0}) {
        std::vector<Vector2> pts;
        draw::QuadTo(pts, F(d.x + nx * side, d.y + ny * side), F(mx + nx * (bend + side), my + ny * (bend + side)), F(d.x2 + nx * side, d.y2 + ny * side), 16);
        draw::Polyline(pts.data(), (int)pts.size(), false, 3, Alpha(C.Tint(d.col), 0.12f), draw::BUTT, false);
    }
}

void DrawStone(const Ctx& C, const Scen& d) {
    const V2 sh = C.Shadow(0.02);
    draw::FillCircle((float)(d.x + sh.x), (float)(d.y + sh.y), (float)d.r, C.ShadowCol(0.9f));
    draw::FillCircle((float)d.x, (float)d.y, (float)d.r, C.Tint(d.col, 0.8f));
    draw::FillCircle((float)(d.x - d.r * 0.2), (float)(d.y - d.r * 0.25), (float)d.r * 0.65f, C.Tint(d.col, 1.02f));
}

void DrawArch(const Ctx& C, const Scen& d, double carX, double carY) {
    // two rock legs and a span across the top; it fades while the car is underneath so the road stays visible
    const double len = Hypot(d.x2 - d.x, d.y2 - d.y);
    const double under = Clamp(1 - (Hypot(carX - d.cx, carY - d.cy) - len * 0.3) / (len * 0.5), 0, 1);
    const float a = 1 - 0.65f * (float)under;
    const V2 sh = C.Shadow(d.ht);
    for (const V2& leg : {V2{d.x, d.y}, V2{d.x2, d.y2}}) {
        Scen s = d; s.x = leg.x; s.y = leg.y; s.ht = d.ht;
        DrawStack(C, s, 3, 0.1f, false);
    }
    std::vector<Vector2> span, shadow;
    for (int i = 0; i <= 12; i++) {
        const double t = i / 12.0, lift = std::sin(t * PI) * 0.04;
        const double x = d.x + (d.x2 - d.x) * t, y = d.y + (d.y2 - d.y) * t;
        const V2 p = C.Top(x, y, d.ht + lift);
        span.push_back(F(p.x, p.y));
        shadow.push_back(F(x + sh.x * 1.1, y + sh.y * 1.1));
    }
    draw::Polyline(shadow.data(), (int)shadow.size(), false, (float)d.r * 1.3f, C.ShadowCol(0.8f), draw::ROUND, false);
    draw::Polyline(span.data(), (int)span.size(), false, (float)d.r * 1.6f, Alpha(C.Tint(d.col, 0.85f), a), draw::ROUND, false);
    draw::Polyline(span.data(), (int)span.size(), false, (float)d.r * 1.0f, Alpha(C.Tint(d.col, 1.05f), a), draw::ROUND, false);
}

void DrawBalloon(const Ctx& C, const Scen& d, double x, double y, bool shadowOnly) {
    if (shadowOnly) {   // far below on the ground, stretched along the sun
        const V2 sh = C.Shadow(d.ht * 1.5);
        draw::FillEllipse((float)(x + sh.x), (float)(y + sh.y), (float)d.r * 1.1f, (float)d.r * 0.9f, (float)C.lk.shadowDir, C.ShadowCol(0.7f));
        return;
    }
    const V2 p = C.Top(x, y, d.ht);
    const float r = (float)(d.r * (1 + d.ht * 0.5));
    BeginUpright(C, p.x, p.y);
    for (int i = 0; i < 8; i++)
        draw::FillSector(0, 0, r, (float)(i * PI / 4), (float)((i + 1) * PI / 4), C.Tint(i % 2 ? d.col : d.col2));
    draw::FillCircle(-r * 0.25f, -r * 0.25f, r * 0.35f, Alpha(WHITE, 0.18f));
    draw::FillRect(-4, -4, 8, 8, C.Tint(0x7a5a3a));   // the basket, seen from above
    EndUpright();
}
}  // namespace

// ---------- the look for a stage ----------
Look LookFor(const std::string& sky, const std::string& weather) {
    auto it = PALS.find(sky);
    const Pal& p = it != PALS.end() ? it->second : PALS.at("noon");
    Look l;
    l.sandLight = Hex(p.light); l.sandShadow = Hex(p.shadow); l.sandLow = Hex(p.low);
    l.road = Hex(p.road); l.runoff = Hex(p.runoff);
    l.skid = Alpha(Scale(Hex(p.shadow), 0.55f), 0.28f);
    l.dust = Scale(Hex(p.light), 1.05f);
    l.zone = Hex(0xffb43a, 0.06f);
    l.light = Hex(p.tint);
    l.shadow = Hex(p.shadowCol, p.shadowA);
    l.shadowDir = p.dir; l.shadowLen = p.len;
    l.gradeTop = Hex(p.gTop, p.aTop); l.gradeBottom = Hex(p.gBot, p.aBot);
    l.vignette = Rgba(20, 12, 30, p.vig);
    l.dark = Hex(p.dark, p.darkness);
    l.lights = p.lights;
    l.weather = weather == "sandstorm" ? 2 : weather == "wind" ? 1 : 0;
    l.storm = l.weather == 2 ? 1.0f : 0.0f;
    l.wind = l.weather == 1 ? 1.0f : 0.0f;
    return l;
}

static Look Blend(const Look& a, const Look& b, float t) {
    Look l = a;
    Color* ca[] = {&l.sandLight, &l.sandShadow, &l.sandLow, &l.road, &l.runoff, &l.skid, &l.dust, &l.zone, &l.light, &l.shadow, &l.gradeTop, &l.gradeBottom, &l.vignette, &l.dark};
    const Color* cb[] = {&b.sandLight, &b.sandShadow, &b.sandLow, &b.road, &b.runoff, &b.skid, &b.dust, &b.zone, &b.light, &b.shadow, &b.gradeTop, &b.gradeBottom, &b.vignette, &b.dark};
    for (size_t k = 0; k < sizeof(ca) / sizeof(ca[0]); k++) *ca[k] = Mix(*ca[k], *cb[k], t);
    double da = b.shadowDir - a.shadowDir;
    da = std::atan2(std::sin(da), std::cos(da));
    l.shadowDir = a.shadowDir + da * t;
    l.shadowLen = a.shadowLen + (b.shadowLen - a.shadowLen) * t;
    l.lights = a.lights || b.lights;
    return l;
}

Look LookAtTime(double day, float storm) {
    static const std::pair<double, const char*> keys[] = {{0.00, "dawn"}, {0.10, "morning"}, {0.26, "noon"}, {0.42, "afternoon"}, {0.55, "sunset"},
                                                          {0.68, "night"}, {0.86, "night"}, {0.94, "predawn"}, {1.00, "dawn"}};
    day = Mod(day, 1.0);
    int k = 0;
    while (k < 7 && day >= keys[k + 1].first) k++;
    const double t = (day - keys[k].first) / (keys[k + 1].first - keys[k].first);
    Look l = Blend(LookFor(keys[k].second, "clear"), LookFor(keys[k + 1].second, "clear"), (float)Smooth01(t));
    if (storm > 0) {   // the sandstorm browns the light and flattens the shadows
        const Look s = LookFor("storm", "sandstorm");
        const Look mixed = Blend(l, s, storm * 0.7f);
        const Color dark = l.dark;
        l = mixed;
        l.dark = dark;
        l.storm = storm;
        l.wind = 1;
    }
    return l;
}

// ---------- loading: bake the dunes into a texture ----------
void DesertScene::Load(const Track& T, const StageDef& def) {
    Unload();
    lk = LookFor(def.sky, def.weather);
    scen = ScatterDesert(T, def);
    const double m = 1100, cell = 5;   // world pixels per texel
    gx0 = T.bounds[0] - m; gy0 = T.bounds[1] - m;
    const int W = (int)std::ceil((T.bounds[2] + m - gx0) / cell), H = (int)std::ceil((T.bounds[3] + m - gy0) / cell);
    gw = W * cell; gh = H * cell;
    const uint32_t seed = 77 + def.seed * 131;
    std::vector<float> h((size_t)W * H);
    for (int j = 0; j < H; j++)
        for (int i = 0; i < W; i++) h[(size_t)j * W + i] = (float)DuneHeight(gx0 + (i + 0.5) * cell, gy0 + (j + 0.5) * cell, seed);
    // light comes from opposite the shadows, lower in the sky when shadows are long
    const double el = std::atan2(1.0, lk.shadowLen * 0.9), lx = -std::cos(lk.shadowDir) * std::cos(el), ly = -std::sin(lk.shadowDir) * std::cos(el), lz = std::sin(el);
    const double relief = 90;   // dune height in world pixels at h = 1
    const double wc = std::cos(windA), ws = std::sin(windA);
    std::vector<Color> px((size_t)W * H);
    for (int j = 0; j < H; j++)
        for (int i = 0; i < W; i++) {
            auto at = [&](int x, int y) { return h[(size_t)std::clamp(y, 0, H - 1) * W + std::clamp(x, 0, W - 1)]; };
            const double hx = (at(i + 1, j) - at(i - 1, j)) * relief / (2 * cell), hy = (at(i, j + 1) - at(i, j - 1)) * relief / (2 * cell);
            const double nl = std::sqrt(hx * hx + hy * hy + 1), nx = -hx / nl, ny = -hy / nl, nz = 1 / nl;
            const double lit = nx * lx + ny * ly + nz * lz;
            Color c = Lerp(lk.sandShadow, lk.sandLight, (float)(0.55 + (lit - lz) * 6.0));
            c = Lerp(c, lk.sandLow, (float)((1 - at(i, j)) * 0.3));
            // fine wind ripples in the sand, mostly on the crests
            const double wx = gx0 + i * cell, wy = gy0 + j * cell;
            const double crest = std::max(0.0, at(i, j) - 0.45) * 1.8;
            const float rip = (float)(1 + 0.022 * crest * std::sin((wx * wc + wy * ws) * 0.3 + 9 * at(i, j)));
            px[(size_t)j * W + i] = Scale(c, rip);
        }
    Image img{px.data(), W, H, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    ground = LoadTextureFromImage(img);
    SetTextureFilter(ground, TEXTURE_FILTER_BILINEAR);
    grains.clear();
}

void DesertScene::Unload() {
    if (ground.id) UnloadTexture(ground);
    ground = Texture2D{};
    for (auto& kv : chunkTex) UnloadTexture(kv.second);
    chunkTex.clear();
    free = false;
    scen.clear();
    grains.clear();
}

// ---------- free roam ----------
void DesertScene::LoadFree(bool dusk) {
    if (!free) {
        if (ground.id) UnloadTexture(ground);
        ground = Texture2D{};
        scen.clear();
        grains.clear();
    }
    free = true;
    fixedDusk = dusk;
    day = dusk ? 0.55 : 0.16;   // the menu holds at sunset; a drive starts in the morning
    storm = 0; stormT = 0; nextStorm = 150;
    lk = LookAtTime(day, 0);
}

// Dunes for one chunk, in neutral grey: the current light tints them, so the day can turn without re-baking.
void DesertScene::BakeChunk(int i, int j) {
    const double cell = 8, CH = DesertWorld::CH;
    const int N = (int)(CH / cell);   // texels along a side, plus one so neighbouring chunks share their edge texels
    const double x0 = i * CH, y0 = j * CH;
    std::vector<float> h((size_t)(N + 3) * (N + 3));
    for (int y = 0; y < N + 3; y++)
        for (int x = 0; x < N + 3; x++) h[(size_t)y * (N + 3) + x] = (float)DuneHeight(x0 + (x - 1) * cell, y0 + (y - 1) * cell, DesertWorld::SEED);
    const double lx = -0.45, ly = -0.55, lz = 0.7, relief = 90;
    std::vector<Color> px((size_t)(N + 1) * (N + 1));
    for (int y = 0; y <= N; y++)
        for (int x = 0; x <= N; x++) {
            auto at = [&](int xx, int yy) { return h[(size_t)(yy + 1) * (N + 3) + (xx + 1)]; };
            const double hx = (at(x + 1, y) - at(x - 1, y)) * relief / (2 * cell), hy = (at(x, y + 1) - at(x, y - 1)) * relief / (2 * cell);
            const double nl = std::sqrt(hx * hx + hy * hy + 1), lit = (-hx * lx - hy * ly + lz) / nl;
            double v = 0.86 + (lit - lz) * 1.8;
            v *= 0.93 + 0.07 * at(x, y);
            const unsigned char c = (unsigned char)Clamp(v * 255, 120, 255);
            px[(size_t)y * (N + 1) + x] = {c, c, c, 255};
        }
    Image img{px.data(), N + 1, N + 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Texture2D t = LoadTextureFromImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    chunkTex[((long long)i << 32) ^ (unsigned)j] = t;
}

void DesertScene::DrawFree(Game& g, double hw, double hh) {
    const double dt = std::min(1.0 / 30, (double)GetFrameTime());
    if (!fixedDusk) {   // the day turns (about eight minutes), and now and then a sandstorm rolls through
        day = Mod(day + dt / 480, 1.0);
        nextStorm -= dt;
        if (nextStorm <= 0 && stormT <= 0) { stormT = 30 + std::rand() % 20; nextStorm = 150 + std::rand() % 120; }
        const float target = stormT > 0 ? 1.0f : 0.0f;
        if (stormT > 0) stormT -= dt;
        storm += (target - storm) * (float)std::min(1.0, dt / 6);
        lk = LookAtTime(day, storm);
    } else lk = LookAtTime(0.55, 0);
    UpdateWeather(g, hw, hh, dt);
    const Ctx C{lk, g, g.now};
    const double CH = DesertWorld::CH;
    auto chunks = g.world.ChunksIn(g.cam.x - hw, g.cam.y - hh, g.cam.x + hw, g.cam.y + hh);
    // the dunes: baked as they come into view, at most two a frame
    int baked = 0;
    for (DesertChunk* ch : chunks) {
        const double x0 = ch->i * CH, y0 = ch->j * CH;
        if (x0 > g.cam.x + hw || x0 + CH < g.cam.x - hw || y0 > g.cam.y + hh || y0 + CH < g.cam.y - hh) continue;
        const long long key = ((long long)ch->i << 32) ^ (unsigned)ch->j;
        auto it = chunkTex.find(key);
        if (it == chunkTex.end() && baked < 2) { BakeChunk(ch->i, ch->j); baked++; it = chunkTex.find(key); }
        if (it != chunkTex.end()) {
            const float n = (float)it->second.width - 1;
            DrawTexturePro(it->second, {0.5f, 0.5f, n, n}, {(float)x0, (float)y0, (float)CH, (float)CH}, {0, 0}, 0, lk.sandLight);
        } else draw::FillRect((float)x0, (float)y0, (float)CH, (float)CH, Scale(lk.sandLight, 0.9f));
    }
    // forget textures far behind
    if (chunkTex.size() > 60) {
        const int ci = (int)std::floor(g.cam.x / CH), cj = (int)std::floor(g.cam.y / CH);
        for (auto it = chunkTex.begin(); it != chunkTex.end();) {
            const int ti = (int)(it->first >> 32), tj = (int)(int32_t)(it->first & 0xffffffff);
            if (std::abs(ti - ci) > 3 || std::abs(tj - cj) > 3) { UnloadTexture(it->second); it = chunkTex.erase(it); } else ++it;
        }
    }
    auto visible = [&](double x, double y, double m) { return std::fabs(x - g.cam.x) < hw + m && std::fabs(y - g.cam.y) < hh + m; };
    // flat things on the ground
    for (DesertChunk* ch : chunks)
        for (const Scen& d : ch->scen) {
            if (d.t == SC_PLATE && visible(d.x, d.y, d.r)) DrawPlate(C, d);
            else if (d.t == SC_TRAIL && visible(d.cx, d.cy, 600)) DrawTrail(C, d);
        }
    for (DesertChunk* ch : chunks)
        for (const Scen& d : ch->scen) {
            if (d.t == SC_SALT && visible(d.x, d.y, d.r + 40)) DrawSalt(C, d);
            else if (d.t == SC_OASIS && visible(d.x, d.y, d.r + 60)) DrawOasis(C, d);
        }
    for (const SkidMark& s : g.skids) draw::Line(s.x0, s.y0, s.x1, s.y1, 4, lk.skid);
    std::vector<const Scen*> items;
    for (DesertChunk* ch : chunks) for (const Scen& d : ch->scen) items.push_back(&d);
    DrawScenery(g, items, hw, hh, [&] {
        g.DrawCar(g.car.x, g.car.y, g.car.a, g.car.w, g.BrakeIn() > 0.1, g.paint);
        DrawPuffs(C, g.smoke, lk);
    });
    for (DesertChunk* ch : chunks)
        for (const Scen& d : ch->scen) if (d.t == SC_ARCH && visible(d.cx, d.cy, 400)) DrawArch(C, d, g.car.x, g.car.y);
    for (DesertChunk* ch : chunks)
        for (const Scen& d : ch->scen)
            if (d.t == SC_BALLOON) {   // drifting a little around its home, high above
                const double x = d.x + std::sin(g.now * 0.05 + d.s * 9) * 300, y = d.y + std::cos(g.now * 0.04 + d.s * 5) * 200;
                if (visible(x, y, 400)) { DrawBalloon(C, d, x, y, true); DrawBalloon(C, d, x, y, false); }
            }
    if (!grains.empty()) {
        const double dx = std::cos(windA), dy = std::sin(windA);
        const Color c = Alpha(lk.sandLight, 0.3f + 0.15f * lk.storm);
        for (const Grain& p : grains) draw::Line((float)p.x, (float)p.y, (float)(p.x - dx * p.len), (float)(p.y - dy * p.len), 1.6f, Alpha(c, (float)std::min(1.0, p.life * 2)));
    }
}

// ---------- weather ----------
void DesertScene::UpdateWeather(const Game& g, double hw, double hh, double dt) {
    const size_t want = (size_t)(520 * lk.storm + (lk.storm > 0.05 ? 0 : 70 * lk.wind));
    const double speed = 260 + 460 * lk.storm, dx = std::cos(windA), dy = std::sin(windA);
    if (grains.size() > want) grains.resize(want);
    auto spawn = [&](Grain& p) {
        p.x = g.cam.x + (std::rand() / (double)RAND_MAX - 0.5) * hw * 2.2;
        p.y = g.cam.y + (std::rand() / (double)RAND_MAX - 0.5) * hh * 2.2;
        p.len = 18 + 12 * lk.storm + std::rand() % 40;
        p.life = 0.6 + (std::rand() % 100) / 60.0;
    };
    while (grains.size() < want) { Grain p; spawn(p); grains.push_back(p); }
    for (Grain& p : grains) {
        p.x += dx * speed * dt; p.y += dy * speed * dt; p.life -= dt;
        if (p.life <= 0 || std::fabs(p.x - g.cam.x) > hw * 1.2 || std::fabs(p.y - g.cam.y) > hh * 1.2) spawn(p);
    }
}

// ---------- the world pass ----------
static void AcrossMark(const Track& T, int q, Color col, float thick) {
    V2 p = T.pts[q], t = T.tan[q];
    const double nx = -t.y, ny = t.x;
    draw::Line((float)(p.x - nx * T.half), (float)(p.y - ny * T.half), (float)(p.x + nx * T.half), (float)(p.y + ny * T.half), thick, col);
    for (int s : {1, -1}) draw::FillCircle((float)(p.x + nx * (T.half + 6) * s), (float)(p.y + ny * (T.half + 6) * s), 5, col);
}

// Scenery standing on the ground, sorted far to near; also notes where the lanterns are for the night glow.
void DesertScene::DrawScenery(Game& g, const std::vector<const Scen*>& items, double hw, double hh, const std::function<void()>& drawCar) {
    const Ctx C{lk, g, g.now};
    // Depth, bigger drawn later: in isometric it is the position down the screen, so things in
    // front cover the car; with the fake perspective it is nearness to the camera, and the car goes first.
    const bool iso = g.up.x != 0 || g.up.y != 0;
    auto depth = [&](double x, double y) { return iso ? x + y : -Hypot(x - g.cam.x, y - g.cam.y); };
    std::vector<std::pair<double, const Scen*>> vis;
    for (const Scen* d : items) {
        if (d->t == SC_OASIS || d->t == SC_BALLOON || d->t == SC_ARCH || d->t == SC_SALT || d->t == SC_PLATE || d->t == SC_TRAIL) continue;
        if (std::fabs(d->cx - g.cam.x) < hw + 260 && std::fabs(d->cy - g.cam.y) < hh + 260) vis.push_back({depth(d->cx, d->cy), d});
        if (d->t == SC_LANTERN && std::fabs(d->x - g.cam.x) < hw + 200 && std::fabs(d->y - g.cam.y) < hh + 200) lamps.push_back(g.TopOf(d->x, d->y, d->ht));
    }
    std::stable_sort(vis.begin(), vis.end(), [](auto& a, auto& b) { return a.first < b.first; });
    const double carDepth = iso ? depth(g.car.x, g.car.y) : -1e18;
    bool carDrawn = false;
    for (auto& [dist, dp] : vis) {
        if (!carDrawn && dist >= carDepth) { drawCar(); carDrawn = true; }
        const Scen& d = *dp;
        switch (d.t) {
            case SC_ROCK: DrawRock(C, d); break;
            case SC_HOODOO: DrawStack(C, d, 4, 0.25f, false); break;
            case SC_RUIN: DrawStack(C, d, 3, 0.05f, true); break;
            case SC_WALL: DrawWall(C, d); break;
            case SC_LANTERN: DrawLantern(C, d); break;
            case SC_GRASS: DrawGrass(C, d); break;
            case SC_BUSH: DrawBush(C, d); break;
            case SC_FLAGS: DrawFlags(C, d); break;
            case SC_STONE: DrawStone(C, d); break;
            case SC_PALM: g.DrawPalm(d); break;
            default: break;
        }
    }
    if (!carDrawn) drawCar();
}

void DesertScene::Draw(Game& g, double hw, double hh) {
    lamps.clear();
    if (free) { DrawFree(g, hw, hh); return; }
    const Track& T = *g.T;
    const double now = g.now;
    const Ctx C{lk, g, now};
    UpdateWeather(g, hw, hh, std::min(1.0 / 30, (double)GetFrameTime()));
    if (ground.id) DrawTexturePro(ground, {0, 0, (float)ground.width, (float)ground.height}, {(float)gx0, (float)gy0, (float)gw, (float)gh}, {0, 0}, 0, WHITE);

    auto visible = [&](double x, double y, double m) { return std::fabs(x - g.cam.x) < hw + m && std::fabs(y - g.cam.y) < hh + m; };
    // ground-level things first: water and the balloons' shadows
    for (const Scen& d : scen) if (d.t == SC_OASIS && visible(d.x, d.y, d.r + 60)) DrawOasis(C, d);
    auto balloonAt = [&](const Scen& d, double& x, double& y) {   // drifting slowly with the wind, looping over the stage
        const double span = gw * 0.8, dx = std::cos(windA) * now * 9, dy = std::sin(windA) * now * 9;
        x = gx0 + gw * 0.1 + Mod(d.x - gx0 + dx, span); y = d.y + dy * 0.3;
    };
    for (const Scen& d : scen)
        if (d.t == SC_BALLOON) { double x, y; balloonAt(d, x, y); if (visible(x, y, 400)) DrawBalloon(C, d, x, y, true); }

    // the track: soft run-off, packed-sand road with feathered edges
    std::vector<Vector2> path(T.pts.size());
    for (size_t i = 0; i < T.pts.size(); i++) path[i] = F(T.pts[i].x, T.pts[i].y);
    const int N = (int)path.size();
    draw::Polyline(path.data(), N, true, (float)(T.edge * 2 + 26), Alpha(lk.runoff, 0.3f), draw::ROUND, false);
    draw::Polyline(path.data(), N, true, (float)(T.edge * 2 + 10), Alpha(lk.runoff, 0.55f), draw::ROUND, false);
    draw::Polyline(path.data(), N, true, (float)(T.edge * 2), lk.runoff);
    draw::Polyline(path.data(), N, true, (float)(T.w + 18), Alpha(lk.road, 0.35f), draw::ROUND, false);
    draw::Polyline(path.data(), N, true, (float)(T.w + 7), Alpha(lk.road, 0.6f), draw::ROUND, false);
    draw::Polyline(path.data(), N, true, (float)T.w, lk.road);
    for (const Zone& z : T.zones) {
        draw::Polyline(path.data() + z.a, z.b - z.a + 1, false, (float)T.w, lk.zone, draw::BUTT, false);
        AcrossMark(T, z.a, Hex(0xe8a13a, 0.75f), 3);
        AcrossMark(T, z.b, Hex(0xe8a13a, 0.35f), 2);
    }
    {   // start / finish: a row of dark and pale tiles
        const V2 p = T.pts[0], t = T.tan[0];
        const float sq = (float)T.w / 10;
        rlPushMatrix();
        rlTranslatef((float)p.x, (float)p.y, 0);
        rlRotatef((float)(std::atan2(t.y, t.x) * RAD2DEG), 0, 0, 1);
        for (int c = 0; c < 10; c++)
            for (int r = 0; r < 2; r++) draw::FillRect(r * sq - sq, (float)-T.half + c * sq, sq, sq, (c + r) % 2 ? Alpha(Scale(lk.road, 0.35f), 0.85f) : Alpha(Scale(lk.road, 1.05f), 0.85f));
        rlPopMatrix();
    }
    for (const Scen& d : scen) if (d.t == SC_SALT && visible(d.x, d.y, d.r + 40)) DrawSalt(C, d);
    for (const SkidMark& s : g.skids) draw::Line(s.x0, s.y0, s.x1, s.y1, 4, lk.skid);
    // clipping points: a ring for how close the rear has to get, and the marker
    for (size_t zi = 0; zi < T.zones.size(); zi++)
        for (size_t ci = 0; ci < T.zones[zi].clips.size(); ci++) {
            const Clip& c = T.zones[zi].clips[ci];
            const bool hit = g.run && zi < g.run->clipHit.size() && g.run->clipHit[zi][ci];
            draw::DashedCircle((float)c.x, (float)c.y, (float)T.clipR, 2, 8, 8, hit ? Rgba(110, 200, 120, .55f) : Alpha(Scale(lk.road, 0.5f), 0.45f));
            const V2 sh = C.Shadow(0.03);
            draw::FillCircle((float)(c.x + sh.x), (float)(c.y + sh.y), 10, C.ShadowCol());
            draw::FillCircle((float)c.x, (float)c.y, 10, hit ? Hex(0x4fd66a) : Hex(0xff7a1a));
            draw::FillCircle((float)c.x, (float)c.y, 4, Hex(0xfff3e0));
        }
    // scenery in depth order with the cars in it, then arches and balloons above everything
    std::vector<const Scen*> items;
    for (const Scen& d : scen) items.push_back(&d);
    DrawScenery(g, items, hw, hh, [&] {
        if (g.run) {   // ghost of the best run
            double a;
            bool ok;
            const V2 p = g.GhostAt(g.state == ST_COUNT ? 0 : g.run->t, a, ok);
            if (ok) { static const Paint ghostPaint = MakePaint(0xbfe9ff); g.DrawCar(p.x, p.y, a, 0, false, ghostPaint, 0.42f); }
        }
        g.DrawCar(g.car.x, g.car.y, g.car.a, g.car.w, g.BrakeIn() > 0.1, g.paint);
        DrawPuffs(C, g.smoke, lk);
    });
    for (const Scen& d : scen) if (d.t == SC_ARCH && visible(d.cx, d.cy, 400)) DrawArch(C, d, g.car.x, g.car.y);
    for (const Scen& d : scen)
        if (d.t == SC_BALLOON) { double x, y; balloonAt(d, x, y); if (visible(x, y, 400)) DrawBalloon(C, d, x, y, false); }
    // blowing sand
    if (!grains.empty()) {
        const double dx = std::cos(windA), dy = std::sin(windA);
        const Color c = Alpha(lk.sandLight, 0.3f + 0.15f * lk.storm);
        for (const Grain& p : grains) draw::Line((float)p.x, (float)p.y, (float)(p.x - dx * p.len), (float)(p.y - dy * p.len), 1.6f, Alpha(c, (float)std::min(1.0, p.life * 2)));
    }
}

// ---------- the screen pass: night light, colour grade, haze ----------
void DesertScene::DrawScreen(Game& g) {
    const float W = g.W, H = g.H, z = (float)(g.cam.z * g.S);
    auto toScreen = [&](double x, double y) { return g.ToScreen(x, y); };
    const Vector2 carS = toScreen(g.car.x, g.car.y);
    if (lk.dark.a > 0) {
        // dark away from the headlights; then the lamps and the headlights add their glow
        const Vector2 f = toScreen(g.car.x + std::cos(g.car.a) * 90, g.car.y + std::sin(g.car.a) * 90);
        draw::RadialGradient(f.x, f.y, 60 * z, 420 * z, Alpha(lk.dark, 0), lk.dark, (W + H) * 2);
        BeginBlendMode(BLEND_ADDITIVE);
        const float strength = lk.dark.a / 255.0f;
        for (const V2& l : lamps) {
            const Vector2 p = toScreen(l.x, l.y);
            draw::RadialGradient(p.x, p.y, 0, 85 * z, Rgba(255, 170, 80, 0.5f * strength), Rgba(255, 170, 80, 0), 0);
        }
        // headlight beams on the ground
        const double a = g.car.a;
        rlBegin(RL_TRIANGLES);
        for (int i = 0; i < 16; i++) {
            const double a0 = a - 0.45 + 0.9 * i / 16, a1 = a - 0.45 + 0.9 * (i + 1) / 16, R = 380;
            const Vector2 o = toScreen(g.car.x + std::cos(a) * 22, g.car.y + std::sin(a) * 22);
            const Vector2 p0 = toScreen(g.car.x + std::cos(a0) * R, g.car.y + std::sin(a0) * R), p1 = toScreen(g.car.x + std::cos(a1) * R, g.car.y + std::sin(a1) * R);
            const Color c0 = Rgba(255, 240, 210, 0.35f * strength), c1 = Rgba(255, 240, 210, 0);
            rlColor4ub(c0.r, c0.g, c0.b, c0.a); rlVertex2f(o.x, o.y);
            rlColor4ub(c1.r, c1.g, c1.b, c1.a); rlVertex2f(p0.x, p0.y);
            rlColor4ub(c1.r, c1.g, c1.b, c1.a); rlVertex2f(p1.x, p1.y);
        }
        rlEnd();
        EndBlendMode();
    } else if (lk.lights) {
        // evening: the lamps glow softly even before dark
        BeginBlendMode(BLEND_ADDITIVE);
        for (const V2& l : lamps) {
            const Vector2 p = toScreen(l.x, l.y);
            draw::RadialGradient(p.x, p.y, 0, 42 * z, Rgba(255, 180, 90, 0.28f), Rgba(255, 180, 90, 0), 0);
        }
        EndBlendMode();
    }
    if (lk.storm > 0.01f)   // sandstorm: the world fades into haze a few car lengths away
        draw::RadialGradient(carS.x, carS.y, 200 * z, 640 * z, Alpha(lk.sandLow, 0), Alpha(lk.sandLow, 0.88f * lk.storm), (W + H) * 2);
    draw::FillRectGradV(0, 0, W, H, lk.gradeTop, lk.gradeBottom);
    draw::RadialGradient(W / 2, H / 2, std::min(W, H) * 0.42f, Hypot(W, H) * 0.62f, Alpha(lk.vignette, 0), lk.vignette, (W + H) * 2);
}
