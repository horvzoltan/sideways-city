#include "draw.h"
#include <algorithm>
#include <cmath>

Color Shade(uint32_t hex, double f) {
    auto ch = [&](int v) { return (unsigned char)Clamp(JsRound(v * f), 0, 255); };
    return {ch(hex >> 16), ch((hex >> 8) & 255), ch(hex & 255), 255};
}
Color Mix(Color a, Color b, float k) {
    auto m = [&](unsigned char x, unsigned char y) { return (unsigned char)JsRound(x + (y - x) * k); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
}
uint32_t ToHex(Color c) { return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b; }

namespace draw {
float pxPerUnit = 1;

static inline void V(Vector2 p, Color c) { rlColor4ub(c.r, c.g, c.b, c.a); rlVertex2f(p.x, p.y); }
static inline void V(float x, float y, Color c) { rlColor4ub(c.r, c.g, c.b, c.a); rlVertex2f(x, y); }

static int Segs(float r, float span = 2 * PI) {
    int n = (int)(r * pxPerUnit * 0.6f * span / (2 * PI));
    int lo = std::max(3, (int)(12 * span / (2 * PI)));
    return std::clamp(n, lo, 96);
}

void Tri(Vector2 a, Vector2 b, Vector2 c, Color col) {
    rlBegin(RL_TRIANGLES); V(a, col); V(b, col); V(c, col); rlEnd();
}

void FillConvex(const Vector2* p, int n, Color c) {
    if (n < 3 || c.a == 0) return;
    rlBegin(RL_TRIANGLES);
    for (int i = 1; i + 1 < n; i++) { V(p[0], c); V(p[i], c); V(p[i + 1], c); }
    rlEnd();
}

void FillFan(Vector2 ctr, const Vector2* p, int n, Color c) {
    if (n < 2 || c.a == 0) return;
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < n; i++) { V(ctr, c); V(p[i], c); V(p[(i + 1) % n], c); }
    rlEnd();
}

void FillRect(float x, float y, float w, float h, Color c) {
    if (c.a == 0) return;
    rlBegin(RL_TRIANGLES);
    V(x, y, c); V(x, y + h, c); V(x + w, y + h, c);
    V(x, y, c); V(x + w, y + h, c); V(x + w, y, c);
    rlEnd();
}
void FillRectGradV(float x, float y, float w, float h, Color t, Color b) {
    rlBegin(RL_TRIANGLES);
    V(x, y, t); V(x, y + h, b); V(x + w, y + h, b);
    V(x, y, t); V(x + w, y + h, b); V(x + w, y, t);
    rlEnd();
}
void FillRectGradH(float x, float y, float w, float h, Color l, Color r) {
    rlBegin(RL_TRIANGLES);
    V(x, y, l); V(x, y + h, l); V(x + w, y + h, r);
    V(x, y, l); V(x + w, y + h, r); V(x + w, y, r);
    rlEnd();
}

void FillSector(float x, float y, float r, float a0, float a1, Color c) {
    if (c.a == 0 || r <= 0) return;
    int n = Segs(r, std::fabs(a1 - a0));
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < n; i++) {
        float t0 = a0 + (a1 - a0) * i / n, t1 = a0 + (a1 - a0) * (i + 1) / n;
        V(x, y, c); V(x + std::cos(t0) * r, y + std::sin(t0) * r, c); V(x + std::cos(t1) * r, y + std::sin(t1) * r, c);
    }
    rlEnd();
}
void FillCircle(float x, float y, float r, Color c) { FillSector(x, y, r, 0, 2 * PI, c); }

void FillEllipse(float x, float y, float rx, float ry, float rot, Color c) {
    if (c.a == 0) return;
    int n = Segs(std::max(rx, ry));
    float cr = std::cos(rot), sr = std::sin(rot);
    auto P = [&](int i) {
        float t = 2 * PI * i / n, ex = std::cos(t) * rx, ey = std::sin(t) * ry;
        return Vector2{x + ex * cr - ey * sr, y + ex * sr + ey * cr};
    };
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < n; i++) { V(x, y, c); V(P(i), c); V(P(i + 1), c); }
    rlEnd();
}

void FillRoundRect(float x, float y, float w, float h, float r, Color c) {
    r = std::min(r, std::min(w, h) / 2);
    std::vector<Vector2> p;
    const float cx[4] = {x + w - r, x + w - r, x + r, x + r}, cy[4] = {y + r, y + h - r, y + h - r, y + r};
    for (int k = 0; k < 4; k++)
        for (int i = 0; i <= 4; i++) {
            float a = -PI / 2 + (k + i / 4.0f) * PI / 2;
            p.push_back({cx[k] + std::cos(a) * r, cy[k] + std::sin(a) * r});
        }
    FillConvex(p.data(), (int)p.size(), c);
}

void StrokeArc(float x, float y, float r, float a0, float a1, float width, Color c) {
    if (c.a == 0) return;
    int n = Segs(r, std::fabs(a1 - a0));
    float ri = r - width / 2, ro = r + width / 2;
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < n; i++) {
        float t0 = a0 + (a1 - a0) * i / n, t1 = a0 + (a1 - a0) * (i + 1) / n;
        float c0 = std::cos(t0), s0 = std::sin(t0), c1 = std::cos(t1), s1 = std::sin(t1);
        V(x + c0 * ri, y + s0 * ri, c); V(x + c0 * ro, y + s0 * ro, c); V(x + c1 * ro, y + s1 * ro, c);
        V(x + c0 * ri, y + s0 * ri, c); V(x + c1 * ro, y + s1 * ro, c); V(x + c1 * ri, y + s1 * ri, c);
    }
    rlEnd();
}
void StrokeCircle(float x, float y, float r, float width, Color c) { StrokeArc(x, y, r, 0, 2 * PI, width, c); }

void DashedCircle(float x, float y, float r, float width, float dash, float gap, Color c) {
    float L = 2 * PI * r;
    for (float s = 0; s < L; s += dash + gap) StrokeArc(x, y, r, s / r, std::min(s + dash, L) / r, width, c);
}

void StrokeEllipseArc(float x, float y, float rx, float ry, float rot, float a0, float a1, float width, Color c) {
    int n = Segs(std::max(rx, ry), std::fabs(a1 - a0));
    float cr = std::cos(rot), sr = std::sin(rot);
    std::vector<Vector2> p;
    for (int i = 0; i <= n; i++) {
        float t = a0 + (a1 - a0) * i / n, ex = std::cos(t) * rx, ey = std::sin(t) * ry;
        p.push_back({x + ex * cr - ey * sr, y + ex * sr + ey * cr});
    }
    Polyline(p.data(), (int)p.size(), false, width, c, BUTT, false);
}

void Line(float x0, float y0, float x1, float y1, float width, Color c, Cap cap) {
    float dx = x1 - x0, dy = y1 - y0, L = std::sqrt(dx * dx + dy * dy);
    if (L < 1e-6f) { if (cap == ROUND) FillCircle(x0, y0, width / 2, c); return; }
    float nx = -dy / L * width / 2, ny = dx / L * width / 2;
    rlBegin(RL_TRIANGLES);
    V(x0 + nx, y0 + ny, c); V(x0 - nx, y0 - ny, c); V(x1 - nx, y1 - ny, c);
    V(x0 + nx, y0 + ny, c); V(x1 - nx, y1 - ny, c); V(x1 + nx, y1 + ny, c);
    rlEnd();
    if (cap == ROUND) { FillCircle(x0, y0, width / 2, c); FillCircle(x1, y1, width / 2, c); }
}

void StrokeRect(float x, float y, float w, float h, float width, Color c) {
    const Vector2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    StrokePoly(p, 4, width, c);
}
void StrokePoly(const Vector2* p, int n, float width, Color c) { Polyline(p, n, true, width, c, BUTT, false); }

void Polyline(const Vector2* pin, int nin, bool closed, float width, Color c, Cap cap, bool joins) {
    if (nin < 2 || c.a == 0 || width <= 0) return;
    // drop repeated points
    std::vector<Vector2> p;
    p.reserve(nin);
    for (int i = 0; i < nin; i++)
        if (p.empty() || std::fabs(pin[i].x - p.back().x) + std::fabs(pin[i].y - p.back().y) > 1e-4f) p.push_back(pin[i]);
    if (closed && p.size() > 2 && std::fabs(p[0].x - p.back().x) + std::fabs(p[0].y - p.back().y) <= 1e-4f) p.pop_back();
    const int n = (int)p.size();
    if (n < 2) { if (cap == ROUND) FillCircle(pin[0].x, pin[0].y, width / 2, c); return; }
    const float h = width / 2;
    std::vector<Vector2> off(n);
    std::vector<char> stamp(n, 0);
    auto dirTo = [&](int a, int b, float& len) {
        float dx = p[b].x - p[a].x, dy = p[b].y - p[a].y;
        len = std::sqrt(dx * dx + dy * dy);
        return Vector2{dx / len, dy / len};
    };
    for (int i = 0; i < n; i++) {
        bool hasPrev = closed || i > 0, hasNext = closed || i < n - 1;
        float l0 = 0, l1 = 0;
        Vector2 d0{0, 0}, d1{0, 0};
        if (hasPrev) d0 = dirTo((i - 1 + n) % n, i, l0);
        if (hasNext) d1 = dirTo(i, (i + 1) % n, l1);
        Vector2 n0{-d0.y, d0.x}, n1{-d1.y, d1.x};
        if (!hasPrev) { off[i] = {n1.x * h, n1.y * h}; continue; }
        if (!hasNext) { off[i] = {n0.x * h, n0.y * h}; continue; }
        Vector2 m{n0.x + n1.x, n0.y + n1.y};
        float ml = std::sqrt(m.x * m.x + m.y * m.y);
        float cosT = std::clamp(n0.x * n1.x + n0.y * n1.y, -1.0f, 1.0f);
        if (ml < 1e-4f) { m = n1; ml = 1; }
        m = {m.x / ml, m.y / ml};
        float dot = std::max(0.35f, m.x * n1.x + m.y * n1.y);   // mitre, limited
        off[i] = {m.x * h / dot, m.y * h / dot};
        float turn = std::acos(cosT);
        if (joins && turn > 1e-3f && h > 0.7f * std::min(l0, l1) / turn) stamp[i] = 1;   // tighter than the stroke: round it off
        if (joins && turn > 0.5f) stamp[i] = 1;
    }
    rlBegin(RL_TRIANGLES);
    const int segs = closed ? n : n - 1;
    for (int i = 0; i < segs; i++) {
        int j = (i + 1) % n;
        Vector2 a{p[i].x + off[i].x, p[i].y + off[i].y}, b{p[i].x - off[i].x, p[i].y - off[i].y};
        Vector2 cc{p[j].x - off[j].x, p[j].y - off[j].y}, d{p[j].x + off[j].x, p[j].y + off[j].y};
        V(a, c); V(b, c); V(cc, c);
        V(a, c); V(cc, c); V(d, c);
    }
    rlEnd();
    for (int i = 0; i < n; i++) if (stamp[i]) FillCircle(p[i].x, p[i].y, h, c);
    if (!closed && cap == ROUND) { FillCircle(p[0].x, p[0].y, h, c); FillCircle(p[n - 1].x, p[n - 1].y, h, c); }
}

void DashedPolyline(const Vector2* p, int n, bool closed, float width, float dash, float gap, Color c, Cap cap) {
    if (n < 2) return;
    const int segs = closed ? n : n - 1;
    std::vector<float> cum(segs + 1, 0);
    for (int i = 0; i < segs; i++) {
        Vector2 a = p[i], b = p[(i + 1) % n];
        cum[i + 1] = cum[i] + std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
    }
    const float total = cum[segs];
    auto at = [&](float s, int& seg) {
        while (seg < segs - 1 && cum[seg + 1] < s) seg++;
        float L = cum[seg + 1] - cum[seg], u = L > 0 ? (s - cum[seg]) / L : 0;
        Vector2 a = p[seg], b = p[(seg + 1) % n];
        return Vector2{a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u};
    };
    std::vector<Vector2> piece;
    int seg = 0;
    for (float s = 0; s < total; s += dash + gap) {
        float e = std::min(s + dash, total);
        piece.clear();
        piece.push_back(at(s, seg));
        int k = seg;
        while (k < segs && cum[k + 1] < e) { piece.push_back(p[(k + 1) % n]); k++; }
        int s2 = seg;
        piece.push_back(at(e, s2));
        Polyline(piece.data(), (int)piece.size(), false, width, c, cap, false);
    }
}

void RadialGradient(float x, float y, float r0, float r1, Color c0, Color c1, float cover) {
    const int n = 72;
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < n; i++) {
        float t0 = 2 * PI * i / n, t1 = 2 * PI * (i + 1) / n;
        float a0 = std::cos(t0), b0 = std::sin(t0), a1 = std::cos(t1), b1 = std::sin(t1);
        if (c0.a) { V(x, y, c0); V(x + a0 * r0, y + b0 * r0, c0); V(x + a1 * r0, y + b1 * r0, c0); }
        V(x + a0 * r0, y + b0 * r0, c0); V(x + a0 * r1, y + b0 * r1, c1); V(x + a1 * r1, y + b1 * r1, c1);
        V(x + a0 * r0, y + b0 * r0, c0); V(x + a1 * r1, y + b1 * r1, c1); V(x + a1 * r0, y + b1 * r0, c0);
        if (c1.a && cover > r1) {
            V(x + a0 * r1, y + b0 * r1, c1); V(x + a0 * cover, y + b0 * cover, c1); V(x + a1 * cover, y + b1 * cover, c1);
            V(x + a0 * r1, y + b0 * r1, c1); V(x + a1 * cover, y + b1 * cover, c1); V(x + a1 * r1, y + b1 * r1, c1);
        }
    }
    rlEnd();
}

void QuadTo(std::vector<Vector2>& out, Vector2 p0, Vector2 c, Vector2 p1, int segs) {
    for (int i = 1; i <= segs; i++) {
        float t = (float)i / segs, u = 1 - t;
        out.push_back({u * u * p0.x + 2 * u * t * c.x + t * t * p1.x, u * u * p0.y + 2 * u * t * c.y + t * t * p1.y});
    }
}
}  // namespace draw

// ---------- text ----------
namespace {
struct FontSet { std::vector<Font> sizes; };
FontSet g_fonts[2];
bool g_fontsOk = false;

const Font& PickFont(FontWeight fw, float size) {
    const auto& v = g_fonts[fw].sizes;
    for (const Font& f : v) if (f.baseSize >= size * 0.95f) return f;
    return v.back();
}
}  // namespace

bool LoadFonts() {
    std::vector<int> cps;
    for (int c = 32; c < 127; c++) cps.push_back(c);
    for (int c : {0xB7, 0xD7, 0x2212, 0x2191, 0x2193, 0x2039, 0x2014, 0x2013, 0x2022, 0x2026}) cps.push_back(c);
    const char* files[2] = {"assets/fonts/chakra-petch-latin-500-normal.ttf", "assets/fonts/chakra-petch-latin-700-normal.ttf"};
    g_fontsOk = true;
    for (int w = 0; w < 2; w++) {
        for (int size : {20, 36, 64, 128}) {
            Font f = FileExists(files[w]) ? LoadFontEx(files[w], size, cps.data(), (int)cps.size()) : GetFontDefault();
            if (f.texture.id == GetFontDefault().texture.id) g_fontsOk = false;
            else SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
            g_fonts[w].sizes.push_back(f);
        }
    }
    return g_fontsOk;
}

void UnloadFonts() {
    for (auto& fs : g_fonts) {
        for (Font& f : fs.sizes) if (f.texture.id != GetFontDefault().texture.id) UnloadFont(f);
        fs.sizes.clear();
    }
}

float TextWidth(FontWeight fw, const std::string& s, float size, float spacing) {
    const Font& f = PickFont(fw, size);
    float scale = size / f.baseSize, w = 0;
    int n = 0;
    for (size_t i = 0; i < s.size();) {
        int bytes = 0, cp = GetCodepointNext(s.c_str() + i, &bytes);
        i += bytes > 0 ? bytes : 1;
        int idx = GetGlyphIndex(f, cp);
        w += (f.glyphs[idx].advanceX ? f.glyphs[idx].advanceX : f.recs[idx].width) * scale + spacing;
        n++;
    }
    return n ? w - spacing : 0;
}

void Text(FontWeight fw, const std::string& s, float x, float y, float size, Color c, float skew, float spacing) {
    if (s.empty() || c.a == 0) return;
    const Font& f = PickFont(fw, size);
    const float scale = size / f.baseSize, k = std::tan(skew * DEG2RAD), mid = y + size * 0.55f, pad = (float)f.glyphPadding;
    const float tw = (float)f.texture.width, th = (float)f.texture.height;
    rlSetTexture(f.texture.id);
    rlBegin(RL_QUADS);
    rlColor4ub(c.r, c.g, c.b, c.a);
    float pen = x;
    for (size_t i = 0; i < s.size();) {
        int bytes = 0, cp = GetCodepointNext(s.c_str() + i, &bytes);
        i += bytes > 0 ? bytes : 1;
        int idx = GetGlyphIndex(f, cp);
        const Rectangle r = f.recs[idx];
        if (cp != ' ' && cp != '\t') {
            float x0 = pen + (f.glyphs[idx].offsetX - pad) * scale, y0 = y + (f.glyphs[idx].offsetY - pad) * scale;
            float x1 = x0 + (r.width + 2 * pad) * scale, y1 = y0 + (r.height + 2 * pad) * scale;
            float u0 = (r.x - pad) / tw, v0 = (r.y - pad) / th, u1 = (r.x + r.width + pad) / tw, v1 = (r.y + r.height + pad) / th;
            float s0 = (mid - y0) * k, s1 = (mid - y1) * k;
            rlTexCoord2f(u0, v0); rlVertex2f(x0 + s0, y0);
            rlTexCoord2f(u0, v1); rlVertex2f(x0 + s1, y1);
            rlTexCoord2f(u1, v1); rlVertex2f(x1 + s1, y1);
            rlTexCoord2f(u1, v0); rlVertex2f(x1 + s0, y0);
        }
        pen += (f.glyphs[idx].advanceX ? f.glyphs[idx].advanceX : r.width) * scale + spacing;
    }
    rlEnd();
    rlSetTexture(0);
}

void TextGlow(FontWeight fw, const std::string& s, float x, float y, float size, Color glow, float radius, float skew, float spacing) {
    for (int ring = 1; ring <= 2; ring++)
        for (int i = 0; i < 8; i++) {
            float a = i * PI / 4 + ring * 0.4f, r = radius * ring / 2;
            Text(fw, s, x + std::cos(a) * r, y + std::sin(a) * r, size, Alpha(glow, 0.22f / ring), skew, spacing);
        }
}

std::vector<std::string> WrapText(FontWeight fw, const std::string& s, float size, float maxW, float spacing) {
    std::vector<std::string> lines;
    std::string line, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::string trial = line.empty() ? word : line + " " + word;
        if (!line.empty() && TextWidth(fw, trial, size, spacing) > maxW) { lines.push_back(line); line = word; }
        else line = trial;
        word.clear();
    };
    for (char ch : s) {
        if (ch == ' ') flushWord();
        else if (ch == '\n') { flushWord(); lines.push_back(line); line.clear(); }
        else word += ch;
    }
    flushWord();
    if (!line.empty()) lines.push_back(line);
    return lines;
}
