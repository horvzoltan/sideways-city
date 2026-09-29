// draw.h - the 2D vector drawing the web version got from Canvas2D, on top of rlgl:
// filled shapes, thick polylines with round joins, dashes, radial gradients and skewed text.
#pragma once
#include "raylib.h"
#include "world.h"
#include <string>
#include <vector>

// ---- colors ----
inline Color Hex(uint32_t rgb, float a = 1) {
    return {(unsigned char)(rgb >> 16), (unsigned char)((rgb >> 8) & 255), (unsigned char)(rgb & 255), (unsigned char)(Clamp(a, 0, 1) * 255 + 0.5)};
}
inline Color Rgba(int r, int g, int b, float a) { return {(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)(Clamp(a, 0, 1) * 255 + 0.5)}; }
inline Color Alpha(Color c, float a) { c.a = (unsigned char)(Clamp(c.a * a, 0, 255)); return c; }
Color Shade(uint32_t hex, double f);                 // every channel times f, like the web shade()
Color Mix(Color a, Color b, float k);                // a -> b
uint32_t ToHex(Color c);

namespace draw {
// World-to-pixel scale of the current transform; circles use it to pick their segment count.
extern float pxPerUnit;

enum Cap { BUTT, ROUND };

void Tri(Vector2 a, Vector2 b, Vector2 c, Color col);
void FillConvex(const Vector2* p, int n, Color c);                  // fan from p[0]
void FillFan(Vector2 center, const Vector2* p, int n, Color c);      // closed star-shaped polygon
// Convex polygon with a color per vertex, from a function of the position (gradients).
template <class F> void FillConvexGrad(const Vector2* p, int n, F colorAt);
void FillRect(float x, float y, float w, float h, Color c);
void FillRectGradV(float x, float y, float w, float h, Color top, Color bottom);
void FillRectGradH(float x, float y, float w, float h, Color left, Color right);
void FillCircle(float x, float y, float r, Color c);
void FillEllipse(float x, float y, float rx, float ry, float rot, Color c);
void FillSector(float x, float y, float r, float a0, float a1, Color c);
void FillRoundRect(float x, float y, float w, float h, float r, Color c);
void StrokeCircle(float x, float y, float r, float width, Color c);
void StrokeArc(float x, float y, float r, float a0, float a1, float width, Color c);
void DashedCircle(float x, float y, float r, float width, float dash, float gap, Color c);
void StrokeEllipseArc(float x, float y, float rx, float ry, float rot, float a0, float a1, float width, Color c);
void Line(float x0, float y0, float x1, float y1, float width, Color c, Cap cap = BUTT);
void StrokeRect(float x, float y, float w, float h, float width, Color c);
void StrokePoly(const Vector2* p, int n, float width, Color c);    // closed outline, mitred
// Thick polyline. `joins` stamps a disc where the stroke is tighter than its own width
// (Canvas round joins); leave it off for translucent strokes so they do not double up.
void Polyline(const Vector2* p, int n, bool closed, float width, Color c, Cap cap = ROUND, bool joins = true);
void DashedPolyline(const Vector2* p, int n, bool closed, float width, float dash, float gap, Color c, Cap cap = BUTT);
// Canvas radial gradient: c0 inside r0, a ramp to c1 at r1, c1 out to `cover`.
void RadialGradient(float x, float y, float r0, float r1, Color c0, Color c1, float cover);
// Appends a quadratic curve from p0 (not included) to p1.
void QuadTo(std::vector<Vector2>& out, Vector2 p0, Vector2 c, Vector2 p1, int segs = 8);
}  // namespace draw

// ---- text: Chakra Petch, the web version's font ----
enum FontWeight { W500, W700 };
bool LoadFonts();
void UnloadFonts();
float TextWidth(FontWeight fw, const std::string& s, float size, float spacing = 0);
// (x, y) is the top left of the line. skew leans the letters like CSS skewX(-skew deg).
void Text(FontWeight fw, const std::string& s, float x, float y, float size, Color c, float skew = 0, float spacing = 0);
// Soft glow (CSS text-shadow with blur): a few faint copies around the text.
void TextGlow(FontWeight fw, const std::string& s, float x, float y, float size, Color glow, float radius, float skew = 0, float spacing = 0);
std::vector<std::string> WrapText(FontWeight fw, const std::string& s, float size, float maxW, float spacing = 0);

// ---- template ----
#include "rlgl.h"
template <class F> void draw::FillConvexGrad(const Vector2* p, int n, F colorAt) {
    if (n < 3) return;
    rlBegin(RL_TRIANGLES);
    for (int i = 1; i + 1 < n; i++) {
        const Vector2 v[3] = {p[0], p[i], p[i + 1]};
        for (const Vector2& q : v) { Color c = colorAt(q); rlColor4ub(c.r, c.g, c.b, c.a); rlVertex2f(q.x, q.y); }
    }
    rlEnd();
}
