// render.cpp - the camera, the car, palms, the minimaps and the menu previews. The desert itself
// (stages and free roam) is drawn by desert_scene.cpp.
#include "game.h"
#include "rlgl.h"
#include <algorithm>

namespace {
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

// Side walls of a flat outline (clockwise, in the car's frame) raised by `lift`: one quad per edge.
// Walls facing one side of the screen's up axis are lit, the others in shade.
void Extrude(const Vector2* p, int n, Vector2 lift, Color base) {
    const uint32_t hex = ToHex(base);
    for (int i = 0; i < n; i++) {
        const Vector2 a = p[i], b = p[(i + 1) % n];
        const float ex = b.x - a.x, ey = b.y - a.y;
        const float side = ey * lift.y + ex * lift.x;   // outward normal (ey, -ex) against the lift, turned a quarter
        Color c = Shade(hex, side > 0 ? 0.92 : 0.7);
        c.a = base.a;
        const Vector2 q[4] = {a, b, {b.x + lift.x, b.y + lift.y}, {a.x + lift.x, a.y + lift.y}};
        draw::FillConvex(q, 4, c);
    }
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
    // the shadow falls along the sun in the world, so turn that offset into the car's frame
    const float wx = (float)(shadowX * 3), wy = (float)(shadowY * 3), ca = std::cos((float)a), sa = std::sin((float)a);
    const Vector2 so{ca * wx + sa * wy, -sa * wx + ca * wy};
    std::vector<Vector2> sh(body);
    for (auto& v : sh) { v.x += so.x; v.y += so.y; }
    draw::FillFan(so, sh.data(), n, A(Rgba(0, 0, 0, .35f)));
    // tyres peek out from under the wide arches; fronts turn with the steering
    const Color tyre = A(Hex(0x141414));
    Rect(-18, -13, 10, 3, tyre); Rect(-18, 10, 10, 3, tyre);
    Push(12.5, 0, w * 0.14);
    Rect(-4.5f, -12.5f, 9, 3, tyre); Rect(-4.5f, 9.5f, 9, 3, tyre);
    Pop();
    // isometric: the body and the cabin stand up from the ground. `up` is the screen's up in the
    // world; turn it into the car's frame like the shadow above.
    const bool solid = up.x != 0 || up.y != 0;
    const float CAR_H = 7, CAB_H = 5;   // in car units (the car is 48 long)
    const Vector2 f{ca * (float)up.x + sa * (float)up.y, -sa * (float)up.x + ca * (float)up.y};
    const Vector2 lift{f.x * CAR_H, f.y * CAR_H}, cab{f.x * CAB_H, f.y * CAB_H};
    if (solid) {
        Extrude(body.data(), n, lift, A(p.dark));
        rlTranslatef(lift.x, lift.y, 0);
    }
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
    if (solid) {   // the cabin rises above the body
        static const Vector2 cabin[8] = {{11.5f, -7.2f}, {11.5f, 7.2f}, {3, 8.8f}, {-9, 8.4f}, {-13.5f, 6.8f}, {-13.5f, -6.8f}, {-9, -8.4f}, {3, -8.8f}};
        Extrude(cabin, 8, cab, A(Hex(0x0d1117)));
        rlTranslatef(cab.x, cab.y, 0);
    }
    Poly({{11.5f, -7.2f}, {11.5f, 7.2f}, {3, 8.8f}, {3, -8.8f}}, glass);
    Poly({{3, -8.8f}, {-9, -8.4f}, {-13.5f, -6.8f}, {-13.5f, 6.8f}, {-9, 8.4f}, {3, 8.8f}}, glass);
    // roof panel over the glass
    draw::FillRoundRect(-8.5f, -7.2f, 11, 14.4f, 2, A(p.roof));
    Rect(9, -6, 1.5f, 5, A(Rgba(255, 255, 255, .18f)));   // windshield glint
    // twin stripes on the roof, then back down on the body for the hood and deck
    Rect(-8.5f, -3.4f, 11, 2, A(p.stripe)); Rect(-8.5f, 1.4f, 11, 2, A(p.stripe));
    if (solid) rlTranslatef(-cab.x, -cab.y, 0);
    for (auto [x0, x1] : {std::pair<float, float>{11.5f, 24.4f}, {-23.2f, -13.5f}}) {
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
void Game::DrawPalm(const Scen& p) {
    const V2 top = TopOf(p.x, p.y, p.ht);
    const float ox = (float)top.x, oy = (float)top.y;
    const float shx = (float)(shadowX * p.ht * 100), shy = (float)(shadowY * p.ht * 100);   // cast along the sun
    draw::FillEllipse((float)p.x + shx, (float)p.y + shy, (float)p.r * 0.55f, (float)p.r * 0.4f, std::atan2(shy, shx), Rgba(0, 0, 0, .12f));
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

void Game::Render(double speed) {
    const float Wc = W / S, Hc = H / S;   // CSS-sized view
    const Look& lk = desert.look();
    DrawRectangle(0, 0, (int)W, (int)H, lk.sandLow);
    shadowX = std::cos(lk.shadowDir) * lk.shadowLen; shadowY = std::sin(lk.shadowDir) * lk.shadowLen;   // shadows fall along the sun
    const double dt = std::min(1.0 / 30, (double)GetFrameTime());
    const double scaleBase = std::min(1.15, std::max(0.75, std::min(Wc, Hc) / 620.0)) * (mode == MODE_TRACK ? 0.8 : 1) * (iso ? 1.15 : 1);
    const double tz = scaleBase * (1.1 - std::min(0.38, speed / 1500));
    const double kz = 1 - std::pow(1 - 0.04, dt * 60), kp = 1 - std::pow(1 - 0.1, dt * 60);   // per-frame easing at 60 fps
    cam.z += (tz - cam.z) * kz;
    const double tx = car.x + car.vx * 0.35, ty = car.y + car.vy * 0.35;
    cam.x += (tx - cam.x) * kp; cam.y += (ty - cam.y) * kp;
    const double sx = (std::rand() / (double)RAND_MAX - .5) * shake, sy = (std::rand() / (double)RAND_MAX - .5) * shake;

    // the camera, as BeginMode2D would build it, with the isometric turn and squash between the
    // zoom and the pan (Camera2D cannot squash)
    const double Z = cam.z * S;
    rlDrawRenderBatchActive();
    rlLoadIdentity();
    rlTranslatef((float)(W / 2 + sx * S), (float)(H / 2 + sy * S), 0);
    double hw, hh;
    if (iso) {
        rlScalef((float)Z, (float)(Z * ISO_SQUASH), 1);
        rlRotatef((float)ISO_ROT, 0, 0, 1);
        up = {-std::sqrt(2.0), -std::sqrt(2.0)};
        hw = hh = (Wc / 2 + Hc) * 0.7072 / cam.z + 64;   // the screen's diamond, as a box in the world
    } else {
        rlScalef((float)Z, (float)Z, 1);
        up = {0, 0};
        hw = Wc / 2 / cam.z + 64; hh = Hc / 2 / cam.z + 64;
    }
    rlTranslatef((float)-cam.x, (float)-cam.y, 0);
    draw::pxPerUnit = (float)Z;
    desert.Draw(*this, hw, hh);
    EndMode2D();
    up = {0, 0};
    draw::pxPerUnit = 1;
    desert.DrawScreen(*this);   // night light, colour grade, sandstorm haze
}

Vector2 Game::ToScreen(double x, double y) const {
    const double px = x - cam.x, py = y - cam.y, Z = cam.z * S;
    if (!iso) return {(float)(W / 2 + px * Z), (float)(H / 2 + py * Z)};
    const double c = std::cos(ISO_ROT * DEG2RAD), s = std::sin(ISO_ROT * DEG2RAD);
    return {(float)(W / 2 + (c * px - s * py) * Z), (float)(H / 2 + (s * px + c * py) * ISO_SQUASH * Z)};
}

// ---------- minimap ----------
void Game::BuildMini() {
    if (!miniOk) return;
    BeginTextureMode(miniBg);
    ClearBackground(BLANK);
    BeginTextureBlend();
    draw::pxPerUnit = 1;
    if (mode == MODE_TRACK) {
        double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
        for (V2 p : T->pts) { x0 = std::min(x0, p.x); y0 = std::min(y0, p.y); x1 = std::max(x1, p.x); y1 = std::max(y1, p.y); }
        const double pad = T->edge + 60, s = std::min(250 / (x1 - x0 + 2 * pad), 250 / (y1 - y0 + 2 * pad));
        mm = {s, 140 - (x0 + x1) / 2 * s, 140 - (y0 + y1) / 2 * s};
        Push(mm.ox, mm.oy, 0, s);
        draw::pxPerUnit = (float)s;
        const auto path = F(T->pts);
        const float w = (float)std::max(T->w, 5 / s);
        draw::Polyline(path.data(), (int)path.size(), true, w, Rgba(243, 234, 210, .75f), draw::ROUND, false);
        for (const Zone& z : T->zones) draw::Polyline(path.data() + z.a, z.b - z.a + 1, false, w * 0.55f, Hex(0xf4b56a), draw::BUTT, false);
        V2 p = T->pts[0], t = T->tan[0];
        draw::Line((float)(p.x + t.y * T->w * 1.2), (float)(p.y - t.x * T->w * 1.2), (float)(p.x - t.y * T->w * 1.2), (float)(p.y + t.x * T->w * 1.2), (float)(5 / s), WHITE);
        Pop();
        draw::pxPerUnit = 1;
    }
    EndBlendMode();
    EndTextureMode();
}

void Game::DrawMini(Rectangle r) {
    const float f = r.width / 280;
    if (mode == MODE_FREE) {   // free roam: a radar of the desert around the car
        const double s = 0.05;
        mm = {s, 140 - car.x * s, 140 - car.y * s};
        const double reach = 140 / s;
        rlDrawRenderBatchActive();
        BeginScissorMode((int)(r.x + 10 * f), (int)(r.y + 10 * f), (int)(260 * f), (int)(260 * f));
        Push(r.x, r.y, 0, f);
        draw::FillRoundRect(10, 10, 260, 260, 24, Alpha(desert.look().sandLow, 0.92f));
        for (DesertChunk* ch : world.ChunksIn(car.x - reach, car.y - reach, car.x + reach, car.y + reach))
            for (const Scen& d : ch->scen) {
                const float x = (float)(d.x * s + mm.ox), y = (float)(d.y * s + mm.oy);
                switch (d.t) {
                    case SC_OASIS: draw::FillCircle(x, y, (float)(d.r * s), Hex(0x5fa9a4)); break;
                    case SC_SALT: draw::FillCircle(x, y, (float)(d.r * s), Rgba(246, 241, 232, .7f)); break;
                    case SC_PLATE: draw::FillCircle(x, y, (float)(d.r * s), Rgba(120, 95, 75, .6f)); break;
                    case SC_ARCH: draw::Line(x, y, (float)(d.x2 * s + mm.ox), (float)(d.y2 * s + mm.oy), 3, Hex(0x8a5a40)); break;
                    case SC_HOODOO: case SC_RUIN: case SC_WALL: draw::FillCircle(x, y, 2.5f, Rgba(90, 60, 45, .8f)); break;
                    case SC_ROCK: draw::FillCircle(x, y, 1.6f, Rgba(90, 60, 45, .6f)); break;
                    default: break;
                }
            }
        Pop();
        rlDrawRenderBatchActive();
        EndScissorMode();
    } else if (miniOk) DrawTextureRT(miniBg, r.x, r.y, r.width, r.height);
    Push(r.x, r.y, 0, f);
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
    const Color grid = Rgba(241, 211, 161, .12f);
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
    draw::Polyline(path.data(), N, true, (float)(T.w + 30 * k / sc), Rgba(241, 211, 161, .2f), draw::ROUND, false);
    draw::Polyline(path.data(), N, true, (float)(T.w * 0.55), Hex(0xfbf4ea, 0.9f));
    for (const Zone& z : T.zones) draw::Polyline(path.data() + z.a, z.b - z.a + 1, false, (float)(T.w * 0.55), Hex(0xf4b56a), draw::ROUND);
    V2 p = T.pts[0], t = T.tan[0];
    draw::Line((float)(p.x + t.y * T.w), (float)(p.y - t.x * T.w), (float)(p.x - t.y * T.w), (float)(p.y + t.x * T.w), (float)(8 * k / sc), Hex(0xe9806e), draw::ROUND);
    Pop();
    draw::pxPerUnit = 1;
}
