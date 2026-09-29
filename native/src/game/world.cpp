#include "world.h"
#include <algorithm>

const std::vector<uint32_t> DECO = {0xf4a7b9, 0x9ee0cc, 0x8fd3e8, 0xf8c89a, 0xcbb7e6, 0xf3e3a0, 0xf1ede4, 0xffb3a1};   // art deco pastels
const std::vector<uint32_t> ACCENT = {0x2bb5b0, 0xe2598b, 0xf39a4a, 0x5b8fd6, 0xf1ede4};
const std::vector<uint32_t> TOWERS = {0xeef0ee, 0xd9e6ea, 0xebe4d6, 0xd3e2e0};
const std::vector<uint32_t> FRONDS = {0x3f8a3a, 0x4a9a3c, 0x367a34, 0x52a043};

bool CircleHit(const Solid& b, double cx, double cy, double r, Hit& out) {
    double lx, ly, hw, hh, c = 1, s = 0;
    if (b.rot) {
        c = std::cos(b.ang); s = std::sin(b.ang);
        double dx = cx - b.cx, dy = cy - b.cy;
        lx = dx * c + dy * s; ly = -dx * s + dy * c; hw = b.hw; hh = b.hh;
    } else {
        hw = b.w / 2; hh = b.h / 2;
        lx = cx - (b.x + hw); ly = cy - (b.y + hh);
    }
    double px = std::max(-hw, std::min(hw, lx)), py = std::max(-hh, std::min(hh, ly));
    double ex = lx - px, ey = ly - py, d = Hypot(ex, ey), pen;
    if (d >= r) return false;
    if (d > 0.001) { ex /= d; ey /= d; pen = r - d; }
    else {   // centre inside: leave by the nearest face
        double qx = hw - std::fabs(lx), qy = hh - std::fabs(ly);
        if (qx < qy) { ex = lx < 0 ? -1 : 1; ey = 0; pen = qx + r; }
        else { ex = 0; ey = ly > 0 ? 1 : -1; pen = qy + r; }
    }
    out = {ex * c - ey * s, ex * s + ey * c, pen};
    return true;
}
