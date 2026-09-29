// menus.cpp - the main menu, the HUD and the overlays (pause, results, level-up), drawn in the
// style of the web version's menus: slanted items, glass panels with cut corners, cyan and
// yellow accents, Chakra Petch. Sizes are CSS pixels from style.css, times the UI scale S.
#include "game.h"
#include "rlgl.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace {
Game* G = nullptr;
float S = 1;
float px(float v) { return v * S; }
const float SK = 0.2126f;   // tan(12 deg)

const Color WHITE_C = {255, 255, 255, 255};
const Color DIM = Rgba(220, 226, 240, .55f), TEXT_C = Rgba(220, 226, 240, .78f), NAVY = Rgba(10, 14, 26, .85f);

enum Id {
    ID_MAIN = 100, ID_BACK = 200, ID_STAGE = 300, ID_SWATCH = 400, ID_CUSTOM = 408, ID_HUE = 410, ID_SAT = 411, ID_VAL = 412,
    ID_NEXT = 500, ID_RETRY = 501, ID_MENU = 502,
    ID_RESUME = 600, ID_VOLDN = 601, ID_VOLUP = 602, ID_MUTE = 603, ID_RESET = 604, ID_QUITMENU = 605,
    ID_CARD = 700,
};

std::string Upper(std::string s) { for (char& c : s) c = (char)toupper((unsigned char)c); return s; }

// ---- shapes ----
// A panel with the top-right and bottom-left corners cut (CSS clip-path), a 160deg gradient,
// a thin border and a thick coloured bar on the left (or the right, mirrored).
void Panel(Rectangle r, float cut, Color c0, Color c1, Color border, Color bar, float barW, bool mirrored = false) {
    Vector2 p[6];
    if (!mirrored) p[0] = {r.x, r.y}, p[1] = {r.x + r.width - cut, r.y}, p[2] = {r.x + r.width, r.y + cut}, p[3] = {r.x + r.width, r.y + r.height}, p[4] = {r.x + cut, r.y + r.height}, p[5] = {r.x, r.y + r.height - cut};
    else p[0] = {r.x + cut, r.y}, p[1] = {r.x + r.width, r.y}, p[2] = {r.x + r.width, r.y + r.height - cut}, p[3] = {r.x + r.width - cut, r.y + r.height}, p[4] = {r.x, r.y + r.height}, p[5] = {r.x, r.y + cut};
    const float dx = 0.342f, dy = 0.94f, span = r.width * dx + r.height * dy;
    draw::FillConvexGrad(p, 6, [&](Vector2 v) { return Mix(c0, c1, Clamp(((v.x - r.x) * dx + (v.y - r.y) * dy) / span, 0, 1)); });
    if (border.a) draw::StrokePoly(p, 6, px(1), border);
    if (barW > 0) {
        if (!mirrored) {
            const Vector2 b[4] = {{r.x, r.y}, {r.x + barW, r.y}, {r.x + barW, r.y + r.height - cut + barW}, {r.x, r.y + r.height - cut}};
            draw::FillConvex(b, 4, bar);
        } else {
            const Vector2 b[4] = {{r.x + r.width - barW, r.y + cut - barW}, {r.x + r.width, r.y + cut * 0}, {r.x + r.width, r.y + r.height - cut}, {r.x + r.width - barW, r.y + r.height - cut + barW}};
            const Vector2 b2[4] = {{r.x + r.width - barW, r.y}, {r.x + r.width, r.y}, {r.x + r.width, r.y + r.height - cut}, {r.x + r.width - barW, r.y + r.height - cut + barW}};
            (void)b;
            draw::FillConvex(b2, 4, bar);
        }
    }
}
// A slanted box (CSS skewX(-deg) on the whole element).
void Skewed(Rectangle r, float deg, Color c, Color border = BLANK, float bw = 0) {
    const float k = std::tan(deg * DEG2RAD) * r.height / 2;
    const Vector2 p[4] = {{r.x + k, r.y}, {r.x + r.width + k, r.y}, {r.x + r.width - k, r.y + r.height}, {r.x - k, r.y + r.height}};
    draw::FillConvex(p, 4, c);
    if (border.a && bw > 0) draw::StrokePoly(p, 4, bw, border);
}
void SkewedGradH(Rectangle r, float deg, Color l, Color rc, float fadeAt = 1) {
    const float k = std::tan(deg * DEG2RAD) * r.height / 2;
    const Vector2 p[4] = {{r.x + k, r.y}, {r.x + r.width + k, r.y}, {r.x + r.width - k, r.y + r.height}, {r.x - k, r.y + r.height}};
    draw::FillConvexGrad(p, 4, [&](Vector2 v) { return Mix(l, rc, Clamp((v.x - r.x) / (r.width * fadeAt), 0, 1)); });
}
void SkewBar(Rectangle r, float deg, Color c) { Skewed(r, deg, c); }
void Star(float cx, float cy, float r, bool filled, Color c) {
    Vector2 p[10];
    for (int i = 0; i < 10; i++) {
        float a = -PI / 2 + i * PI / 5, rr = i % 2 ? r * 0.45f : r;
        p[i] = {cx + std::cos(a) * rr, cy + std::sin(a) * rr};
    }
    if (filled) draw::FillFan({cx, cy}, p, 10, c);
    else draw::StrokePoly(p, 10, std::max(1.0f, r * 0.12f), c);
}
void Stars(float x, float y, float size, int n, int total, float gap, Color c) {   // x, y: top left
    for (int i = 0; i < total; i++) Star(x + size / 2 + i * (size + gap), y + size / 2, size / 2, i < n, c);
}

// ---- text ----
float TW(FontWeight w, const std::string& s, float size, float ls = 0) { return TextWidth(w, s, px(size), px(size) * ls); }
void Tx(FontWeight w, const std::string& s, float x, float y, float size, Color c, float skew = 0, float ls = 0) { Text(w, s, x, y, px(size), c, skew, px(size) * ls); }
void TShadow(FontWeight w, const std::string& s, float x, float y, float size, Color c, float skew, float ls, float dy, float alpha) {
    Tx(w, s, x, y + px(dy), size, Rgba(0, 0, 0, alpha), skew, ls);
    Tx(w, s, x, y, size, c, skew, ls);
}
float Paragraph(const std::string& s, float x, float y, float w, float size, float lh, Color c, FontWeight fw = W500) {
    for (auto& line : WrapText(fw, s, px(size), w)) { Tx(fw, line, x, y, size, c); y += px(size * lh); }
    return y;
}
float ParagraphH(const std::string& s, float w, float size, float lh, FontWeight fw = W500) {
    return WrapText(fw, s, px(size), w).size() * px(size * lh);
}

// ---- widgets ----
enum BtnStyle { PRIMARY, GHOST };
bool Button(int id, Rectangle r, const std::string& label, BtnStyle st, float fs, UiKind sound, bool isDefault = false, Color textCol = BLANK) {
    const bool act = G->ui.Item(id, r, true, isDefault);
    const bool hot = G->ui.Hot(id);
    if (st == PRIMARY) {
        if (hot) Skewed({r.x - px(3), r.y - px(3), r.width + px(6), r.height + px(6)}, 10, Rgba(255, 207, 58, .25f));
        Skewed(r, 10, hot ? Hex(0xffd95e) : ACCENT_C);
    } else {
        if (hot) SkewedGradH(r, 10, Rgba(47, 214, 255, .26f), NAVY);
        else Skewed(r, 10, NAVY);
        Skewed(r, 10, BLANK, hot ? Rgba(47, 214, 255, .5f) : Rgba(255, 255, 255, .14f), px(1));
        if (hot) SkewBar({r.x - std::tan(10 * DEG2RAD) * 0, r.y, px(4), r.height}, 10, ACCENT_C);
    }
    const std::string L = Upper(label);
    const float tw = TW(W700, L, fs, 0.06f);
    Color tc = textCol.a ? textCol : st == PRIMARY ? Hex(0x1c1c1c) : WHITE_C;
    Tx(W700, L, r.x + r.width / 2 - tw / 2, r.y + r.height / 2 - px(fs) * 0.62f, fs, tc, 10, 0.06f);
    if (act) G->audio.Ui(sound);
    return act;
}

void PreTag(const std::string& s, float x, float y) { Tx(W700, Upper(s), x, y, 12, CYAN, 0, 0.26f); }

float Stats(const std::vector<std::array<std::string, 3>>& rows, float x, float y, float w, int cols) {   // dl: label / value (/ "gold")
    if (rows.empty()) return y;
    draw::FillRect(x, y, w, px(1), Rgba(255, 255, 255, .1f));
    y += px(14);
    const float colW = w / cols;
    float labelW = 0;
    for (auto& r : rows) labelW = std::max(labelW, TW(W700, Upper(r[0]), 12, 0.16f));
    labelW = std::min(labelW + px(20), colW * 0.62f);
    for (size_t i = 0; i < rows.size(); i++) {
        const float cx = x + (i % cols) * colW, cy = y + (i / cols) * px(28);
        Tx(W700, Upper(rows[i][0]), cx, cy + px(3), 12, DIM, 0, 0.16f);
        if (rows[i][2] == "stars") Stars(cx + labelW, cy + px(1), px(16), std::stoi(rows[i][1]), 3, px(4), ACCENT_C);
        else Tx(W700, rows[i][1], cx + labelW, cy, 16, rows[i][2] == "gold" ? ACCENT_C : WHITE_C);
    }
    return y + ((rows.size() + cols - 1) / cols) * px(28);
}

void Kbd(const std::string& key, float& x, float y, bool pad = false, Color padCol = BLANK) {
    const float h = px(24), w = pad ? h : std::max(px(26), TW(W700, key, 12) + px(14));
    if (pad) draw::FillCircle(x + w / 2, y + h / 2, h / 2, padCol);
    else { draw::FillRoundRect(x, y, w, h, px(4), Rgba(255, 255, 255, .1f)); draw::StrokeRect(x, y, w, h, px(1), Rgba(255, 255, 255, .28f)); }
    Tx(W700, key, x + w / 2 - TW(W700, key, 12) / 2, y + h / 2 - px(12) * 0.62f, 12, WHITE_C);
    x += w + px(7);
}
}  // namespace

// ---------- main menu ----------
static void MenuBackground(Game& g) {
    const float W = g.W, H = g.H;
    const Color c = Rgba(4, 7, 16, 1);
    const float stops[4][2] = {{0, .95f}, {.4f, .86f}, {.72f, .5f}, {1, .78f}};
    for (int i = 0; i < 3; i++)
        draw::FillRectGradH(W * stops[i][0], 0, W * (stops[i + 1][0] - stops[i][0]), H, Alpha(c, stops[i][1]), Alpha(c, stops[i + 1][1]));
    // slow diagonal streaks
    const float period = px(300), off = (float)std::fmod(g.now / 14 * 600, 600) * S, k = std::tan(25 * DEG2RAD) * H;
    for (float x = -period * 2 - k + std::fmod(off, period); x < W + period; x += period) {
        const Vector2 p[4] = {{x + px(140) + k, 0}, {x + px(142) + k, 0}, {x + px(142), H}, {x + px(140), H}};
        draw::FillConvex(p, 4, Rgba(47, 214, 255, .06f * .7f));
    }
    draw::FillRectGradV(0, H * 0.6f, W, H * 0.4f, Rgba(47, 214, 255, 0), Rgba(47, 214, 255, .12f));
}

static void Logo(Game& g, float x, float y, bool small) {
    const float vw = g.W / S / 100;
    if (!small) {
        const float fs = Clamp(6.2f * vw, 40, 84), lh = fs * 0.88f;
        const float w = std::max(TW(W700, "SIDEWAYS", fs, -0.01f), TW(W700, "CITY", fs, -0.01f));
        TShadow(W700, "SIDEWAYS", x, y, fs, WHITE_C, 12, -0.01f, 4, .4f);
        TextGlow(W700, "CITY", x, y + px(lh), px(fs), Rgba(255, 207, 58, .35f), px(14), 12, -px(fs) * 0.01f);
        TShadow(W700, "CITY", x, y + px(lh), fs, ACCENT_C, 12, -0.01f, 4, .4f);
        const float by = y + px(lh * 2) + px(12) - px(fs) * 0.1f;
        draw::FillRect(x, by - px(3), w * 0.62f, px(12), Rgba(47, 214, 255, .12f));
        draw::FillRectGradH(x, by, w * 0.62f, px(6), CYAN, Rgba(47, 214, 255, 0));
    } else {
        const float fs = Clamp(2.6f * vw, 26, 38);
        TShadow(W700, "SIDEWAYS", x, y, fs, WHITE_C, 12, -0.01f, 4, .4f);
        const float x2 = x + TW(W700, "SIDEWAYS ", fs, -0.01f);
        TShadow(W700, "CITY", x2, y, fs, ACCENT_C, 12, -0.01f, 4, .4f);
        const float w = x2 - x + TW(W700, "CITY", fs, -0.01f);
        draw::FillRectGradH(x, y + px(fs) + px(6), w * 0.4f, px(4), CYAN, Rgba(47, 214, 255, 0));
    }
}

static void MenuStats(Game& g, float right, float y) {
    int stars = 0;
    for (size_t i = 0; i < STAGES.size(); i++) stars += g.Prog((int)i).stars;
    const std::string vals[3] = {std::to_string(stars) + "/" + std::to_string(STAGES.size() * 3), FmtNum(g.best), g.SurvBestT() ? FmtT(g.SurvBestT()) : "--:--"};
    const char* labels[3] = {"STARS", "BEST DRIFT", "SURVIVED"};
    float x = right;
    for (int i = 2; i >= 0; i--) {
        const float w = std::max(TW(W500, labels[i], 11, 0.2f), TW(W700, vals[i], 22));
        Tx(W500, labels[i], x - TW(W500, labels[i], 11, 0.2f), y, 11, DIM, 0, 0.2f);
        Tx(W700, vals[i], x - TW(W700, vals[i], 22), y + px(16), 22, WHITE_C, 10);
        x -= w + px(26);
    }
}

// The info panel: tag, title, text and stats.
static float InfoPanel(Rectangle r, const std::string& tag, const std::string& title, const std::string& text,
                       const std::vector<std::array<std::string, 3>>& stats, int cols, bool render = true) {
    const float pad = px(26), w = r.width - pad * 2;
    const float titleFs = Clamp(2.4f * G->W / S / 100, 24, 32);
    float h = px(22) + px(12) + px(6) + px(titleFs) + px(10) + ParagraphH(text, w, 16, 1.5f) + px(16) + (stats.empty() ? 0 : px(14) + ((stats.size() + cols - 1) / cols) * px(28)) + px(22);
    if (!render) return h;
    r.height = h;
    Panel(r, px(22), Rgba(14, 20, 36, .9f), Rgba(8, 11, 20, .9f), Rgba(47, 214, 255, .22f), CYAN, px(4));
    float y = r.y + px(22), x = r.x + pad;
    PreTag(tag, x, y); y += px(12) + px(6);
    Tx(W700, Upper(title), x, y, titleFs, WHITE_C, 10); y += px(titleFs) + px(10);
    y = Paragraph(text, x, y, w, 16, 1.5f, TEXT_C) + px(16);
    Stats(stats, x, y, w, cols);
    return h;
}

static void MainView(Game& g, Rectangle area) {
    struct Item { const char* title; int mode; };
    std::vector<Item> items = {{"Stages", 0}, {"Survive the night", 1}, {"Free drive", 2}, {"Garage", 3}, {"Controls", 4}};
    if (g.desktopQuit) items.push_back({"Quit", 5});
    const float vw = g.W / S / 100, fs = Clamp(2.7f * vw, 24, 38), itemH = px(fs) * 1.25f + px(20), gap = px(4);
    const float listW = std::min(px(460), area.width * 0.45f);
    const float listH = items.size() * itemH + (items.size() - 1) * gap;
    float y = area.y + std::max(0.0f, (area.height - listH) / 2);
    int sel = 0;
    for (size_t i = 0; i < items.size(); i++) if (g.ui.Hot(ID_MAIN + (int)i)) sel = (int)i;
    for (size_t i = 0; i < items.size(); i++) {
        const int id = ID_MAIN + (int)i;
        const Rectangle r{area.x, y, listW, itemH};
        const bool act = g.ui.Item(id, r, true, i == 0);
        const bool hot = g.ui.Hot(id);
        if (hot) {
            SkewedGradH(r, 12, Rgba(47, 214, 255, .34f), Rgba(47, 214, 255, 0), 1);
            Skewed({r.x, r.y + px(7), px(6), r.height - px(14)}, 12, ACCENT_C);
            Skewed({r.x - px(3), r.y + px(4), px(12), r.height - px(8)}, 12, Rgba(255, 207, 58, .2f));
        }
        const std::string L = Upper(items[i].title);
        const float ty = r.y + r.height / 2 - px(fs) * 0.62f;
        if (hot) TextGlow(W700, L, r.x + px(28), ty, px(fs), Rgba(47, 214, 255, .55f), px(10), 12, px(fs) * 0.02f);
        Tx(W700, L, r.x + px(28), ty, fs, hot ? WHITE_C : Rgba(230, 236, 250, .45f), 12, 0.02f);
        if (act) {
            const int m = items[i].mode;
            if (m == 0) { g.audio.Ui(UI_SELECT); g.view = V_STAGES; g.ui.ResetFocus(); return; }
            if (m == 1) { g.audio.Ui(UI_START); g.StartSurvival(); return; }
            if (m == 2) { g.audio.Ui(UI_START); g.StartCity(); return; }
            if (m == 3) { g.audio.Ui(UI_SELECT); g.view = V_GARAGE; g.ui.ResetFocus(); return; }
            if (m == 4) { g.audio.Ui(UI_SELECT); g.view = V_CONTROLS; g.ui.ResetFocus(); return; }
            if (m == 5) { g.audio.Ui(UI_SELECT); g.quit = true; return; }
        }
        y += itemH + gap;
    }
    // info for the selected item
    int stars = 0, cleared = 0;
    for (size_t i = 0; i < STAGES.size(); i++) { int s = g.Prog((int)i).stars; stars += s; if (s) cleared++; }
    const std::string n = std::to_string(STAGES.size());
    std::string tag, title, text;
    std::vector<std::array<std::string, 3>> st;
    switch (items[sel].mode) {
        case 0:
            tag = "Career"; title = "Stages";
            text = n + " drift events, from a forgiving practice bowl to frozen lakes, neon downtown and a midnight mountain pass. Stay sideways through the yellow zones, clip the orange cones and beat the target before the clock runs out.";
            st = {{"Cleared", std::to_string(cleared) + " / " + n, ""}, {"Stars", std::to_string(stars) + " / " + std::to_string(STAGES.size() * 3), ""}};
            break;
        case 1:
            tag = "Survival"; title = "Survive the night";
            text = "Hordes pour in from every side of an endless city. Ramming and tyre smoke kill, your drift combo multiplies the damage, and every level-up brings a new upgrade. Last until dawn.";
            st = {{"Best time", g.SurvBestT() ? FmtT(g.SurvBestT()) : "None yet", ""}, {"Most kills", FmtNum(g.SurvBestKills()), ""}, {"Length", "10 minutes", ""}};
            break;
        case 2:
            tag = "Free roam"; title = "Free drive";
            text = "Cruise Miami Beach at your own pace: Ocean Drive, the causeways over the bay and the art deco streets in between. Chain drifts anywhere.";
            st = {{"Best drift", FmtNum(g.best), ""}};
            break;
        case 3: {
            tag = "Customise"; title = "Garage";
            text = "Pick a paint for your car. Eight classic colours or any custom shade you like.";
            std::string name = "Custom";
            for (auto& p : PAINTS) if (p.hex == g.paintHex) name = p.name;
            st = {{"Current paint", name, ""}};
            break;
        }
        case 4: tag = "Help"; title = "Controls"; text = "Keyboard and controller controls. Controller triggers work like pedals."; break;
        default: tag = "Exit"; title = "Quit"; text = "Close the game and return to the desktop."; break;
    }
    const float px0 = area.x + listW + g.W * 0.05f, pw = std::min(px(540), area.x + area.width - px0);
    const float ph = InfoPanel({px0, 0, pw, 0}, tag, title, text, st, 1, false);
    InfoPanel({px0, area.y + (area.height - ph) / 2, pw, 0}, tag, title, text, st, 1);
}

static bool BackButton(Game& g, float x, float y, float& outW) {
    const std::string L = "‹  BACK";
    const float w = TW(W700, L, 13, 0.12f) + px(28), h = px(13) * 1.2f + px(16);
    outW = w;
    const Rectangle r{x, y, w, h};
    const bool act = g.ui.Item(ID_BACK, r);
    const bool hot = g.ui.Hot(ID_BACK);
    Skewed(r, 12, hot ? Rgba(47, 214, 255, .15f) : Rgba(255, 255, 255, .07f), hot ? CYAN : Rgba(255, 255, 255, .25f), px(1));
    Tx(W700, L, r.x + px(12), r.y + px(8), 13, WHITE_C, 12, 0.12f);
    if (act) { g.audio.Ui(UI_BACK); g.view = V_MAIN; g.ui.ResetFocus(); }
    return act;
}

static float SubHead(Game& g, float x, float y, const std::string& title, bool& back) {
    float bw;
    back = BackButton(g, x, y + px(6), bw);
    const float fs = Clamp(3.0f * g.W / S / 100, 26, 42);
    Tx(W700, Upper(title), x + bw + px(20), y + px(20) - px(fs) * 0.55f + px(6), fs, WHITE_C, 12);
    return y + std::max(px(fs), px(40)) + px(18);
}

static void StagesView(Game& g, Rectangle area) {
    bool back;
    float y = SubHead(g, area.x, area.y, "Stage select", back);
    if (back) return;
    const float gap = px(30), leftW = (area.width - gap) / 2.1f, rightX = area.x + leftW + gap, rightW = area.width - leftW - gap;
    // stage list: two columns of six
    const float colGap = px(12), rowGap = px(8), colW = (leftW - colGap) / 2, rowH = px(16) * 1.2f + px(12) * 1.3f + px(16);
    int sel = -1;
    for (size_t i = 0; i < STAGES.size(); i++) if (g.ui.Hot(ID_STAGE + (int)i)) sel = (int)i;
    for (size_t i = 0; i < STAGES.size(); i++) {
        const int col = (int)i / 6, row = (int)i % 6;
        const Rectangle r{area.x + col * (colW + colGap), y + row * (rowH + rowGap), colW, rowH};
        const bool open = i == 0 || g.Prog((int)i - 1).stars > 0;
        const auto p = g.Prog((int)i);
        const int id = ID_STAGE + (int)i;
        const bool act = g.ui.Item(id, r, open, (int)i == g.stageSel);
        const bool hot = g.ui.Hot(id) && open;
        const float a = open ? 1 : 0.38f;
        if (hot) SkewedGradH(r, 8, Rgba(47, 214, 255, .24f), Rgba(10, 14, 26, .8f));
        else Skewed(r, 8, Alpha(Rgba(10, 14, 26, .78f), a));
        Skewed(r, 8, BLANK, hot ? Rgba(47, 214, 255, .45f) : Rgba(255, 255, 255, .1f * a), px(1));
        if (hot) Skewed({r.x, r.y, px(4), r.height}, 8, ACCENT_C);
        char num[4];
        std::snprintf(num, sizeof num, "%02d", (int)i + 1);
        Tx(W700, num, r.x + px(14), r.y + r.height / 2 - px(24) * 0.62f, 24, hot ? CYAN : Rgba(255, 255, 255, .3f * a), 8);
        const float nx = r.x + px(14) + px(42) + px(14);
        const float starsW = px(12) * 3 + px(3) * 2;
        std::string name = Upper(STAGES[i].name);
        while (name.size() > 3 && TW(W700, name, 16, 0.02f) > r.x + r.width - px(14) - starsW - px(10) - nx) name = name.substr(0, name.size() - 4) + "...";
        Tx(W700, name, nx, r.y + px(8), 16, Alpha(WHITE_C, a), 8, 0.02f);
        const std::string bs = open ? (p.best ? "Best " + FmtNum(p.best) : "Not cleared yet") : "Locked · clear stage " + std::to_string(i);
        Tx(W500, bs, nx, r.y + px(8) + px(16) * 1.25f, 12, Rgba(220, 226, 240, .6f * a), 8);
        Stars(r.x + r.width - px(14) - starsW, r.y + r.height / 2 - px(6), px(12), p.stars, 3, px(3), Alpha(ACCENT_C, a));
        if (act) { g.audio.Ui(UI_START); g.stageSel = (int)i; g.StartStage((int)i); return; }
    }
    if (sel >= 0) g.stageSel = sel;
    // stage info with the track preview
    const int i = g.stageSel;
    const StageDef& def = STAGES[i];
    static std::vector<std::unique_ptr<Track>> meta(STAGES.size());
    if (!meta[i]) { meta[i] = std::make_unique<Track>(BuildTrack(def)); StageGoals(*meta[i], i); }
    const Track& Tm = *meta[i];
    const auto p = g.Prog(i);
    const float prevH = std::min(g.H * 0.3f, px(300)), prevW = std::min(rightW - px(52), prevH * 520 / 340);
    std::vector<std::array<std::string, 3>> st = {
        {"Target", FmtNum(Tm.pass), ""}, {"Laps", std::to_string(def.laps), ""}, {"Time limit", FmtT(Tm.limit), ""},
        {"Drift zones", std::to_string(Tm.zones.size()), ""}, {"Best", p.best ? FmtNum(p.best) : "None yet", ""},
        {"Rating", std::to_string(p.stars), "stars"}, {"Ghost", g.GhostExists(i) ? "Saved" : "None", ""}};
    const float pad = px(26), w = rightW - pad * 2;
    const float titleFs = Clamp(2.4f * g.W / S / 100, 24, 32);
    const float h = px(22) + prevW * 340 / 520 + px(12) + px(18) + px(titleFs) + px(10) + ParagraphH(def.desc, w, 16, 1.5f) + px(12) + px(14) + 4 * px(28) + px(22);
    const Rectangle r{rightX, y, rightW, h};
    Panel(r, px(22), Rgba(14, 20, 36, .9f), Rgba(8, 11, 20, .9f), Rgba(47, 214, 255, .22f), CYAN, px(4));
    float yy = r.y + px(22);
    g.DrawTrackPreview(i, {r.x + r.width / 2 - prevW / 2, yy, prevW, prevW * 340 / 520});
    yy += prevW * 340 / 520 + px(12);
    PreTag("Stage " + std::to_string(i + 1) + " · " + def.theme, r.x + pad, yy); yy += px(18);
    Tx(W700, Upper(def.name), r.x + pad, yy, titleFs, WHITE_C, 10); yy += px(titleFs) + px(10);
    yy = Paragraph(def.desc, r.x + pad, yy, w, 16, 1.5f, TEXT_C) + px(12);
    Stats(st, r.x + pad, yy, w, 2);
}

static void GarageView(Game& g, Rectangle area) {
    bool back;
    float y = SubHead(g, area.x, area.y, "Garage", back);
    if (back) return;
    const float gap = px(30), leftW = (area.width - gap) * 1.5f / 2.5f, rightX = area.x + leftW + gap, rightW = area.width - leftW - gap;
    const float showH = std::min(leftW / 2, area.y + area.height - y), showW = showH * 2;
    const Rectangle show{area.x, y, showW, showH};
    // showroom: dark glass with a cyan glow on the floor
    Panel(show, px(26), Rgba(6, 9, 18, .72f), Rgba(6, 9, 18, .72f), Rgba(47, 214, 255, .22f), BLANK, 0);
    rlBegin(RL_TRIANGLES);
    for (int k = 0; k < 64; k++) {
        const float t0 = 2 * PI * k / 64, t1 = 2 * PI * (k + 1) / 64, cx = show.x + show.width / 2, cy = show.y + show.height * 0.7f;
        const float rx = show.width * 0.5f * 0.62f, ry = show.height * 0.7f * 0.62f;
        const Color c0 = Rgba(47, 214, 255, .2f), c1 = Rgba(47, 214, 255, 0);
        rlColor4ub(c0.r, c0.g, c0.b, c0.a); rlVertex2f(cx, cy);
        rlColor4ub(c1.r, c1.g, c1.b, c1.a); rlVertex2f(cx + std::cos(t0) * rx, std::min(show.y + show.height, cy + std::sin(t0) * ry));
        rlColor4ub(c1.r, c1.g, c1.b, c1.a); rlVertex2f(cx + std::cos(t1) * rx, std::min(show.y + show.height, cy + std::sin(t1) * ry));
    }
    rlEnd();
    g.DrawGarageCar(show);
    // paint picker
    float x = rightX, yy = y + std::max(0.0f, (showH - px(300)) / 2);
    PreTag("Paint", x, yy); yy += px(18);
    std::string name = "Custom";
    for (auto& p : PAINTS) if (p.hex == g.paintHex) name = p.name;
    Tx(W700, Upper(name), x, yy, 30, WHITE_C, 10); yy += px(30) + px(18);
    const float sw = px(42), sg = px(12);
    const int perRow = std::max(1, (int)((rightW + sg) / (sw + sg)));
    for (int i = 0; i <= 8; i++) {
        const float cx = x + (i % perRow) * (sw + sg), cy = yy + (i / perRow) * (sw + sg);
        const Rectangle r{cx, cy, sw, sw};
        const int id = i < 8 ? ID_SWATCH + i : ID_CUSTOM;
        const bool chosen = i < 8 ? PAINTS[i].hex == g.paintHex : std::none_of(std::begin(PAINTS), std::end(PAINTS), [&](const PaintPreset& p) { return p.hex == g.paintHex; });
        const bool act = g.ui.Item(id, r, true, chosen);
        const bool hot = g.ui.Hot(id);
        const float c = sw / 2;
        if (chosen || hot) draw::FillCircle(cx + c, cy + c, c + px(hot ? 6 : 5), hot && !chosen ? INK : ACCENT_C);
        if (chosen || hot) draw::FillCircle(cx + c, cy + c, c + px(2), Hex(0x05070e));
        if (i < 8) draw::FillCircle(cx + c, cy + c, c, Hex(PAINTS[i].hex));
        else for (int s = 0; s < 36; s++) draw::FillSector(cx + c, cy + c, c, -PI / 2 + s * PI / 18, -PI / 2 + (s + 1) * PI / 18, ColorFromHSV(s * 10.0f, 0.8f, 1));   // conic rainbow
        draw::StrokeCircle(cx + c, cy + c, c, px(2), chosen ? INK : Rgba(243, 234, 210, .3f));
        if (act) {
            g.audio.Ui(UI_SELECT);
            if (i < 8) { g.ChoosePaint(PAINTS[i].hex); Vector3 hsv = ColorToHSV(Hex(PAINTS[i].hex)); g.customH = hsv.x; g.customS = hsv.y; g.customV = hsv.z; }
            else g.ChoosePaint(ToHex(ColorFromHSV(g.customH, g.customS, g.customV)));
        }
    }
    yy += ((8 / perRow) + 1) * (sw + sg) + px(14);
    // custom colour: hue, saturation, brightness
    PreTag("Custom colour", x, yy); yy += px(22);
    const char* labels[3] = {"Hue", "Saturation", "Brightness"};
    float* vals[3] = {&g.customH, &g.customS, &g.customV};
    for (int k = 0; k < 3; k++) {
        const float bw = std::min(rightW, px(380)), bh = px(14);
        const Rectangle r{x, yy + px(18), bw, bh};
        Tx(W700, Upper(labels[k]), x, yy, 12, DIM, 0, 0.16f);
        float v = k == 0 ? *vals[0] / 360 : *vals[k];
        const bool changed = g.ui.Slider(ID_HUE + k, {r.x - px(8), r.y - px(8), r.width + px(16), r.height + px(16)}, v, k == 0 ? 1.0f / 72 : 0.05f);
        for (int s = 0; s < 24; s++) {
            const float u0 = s / 24.0f, u1 = (s + 1) / 24.0f;
            auto col = [&](float u) {
                return k == 0 ? ColorFromHSV(u * 360, 1, 1) : k == 1 ? ColorFromHSV(g.customH, u, g.customV) : ColorFromHSV(g.customH, g.customS, u);
            };
            draw::FillRectGradH(r.x + r.width * u0, r.y, r.width * (u1 - u0) + 0.5f, bh, col(u0), col(u1));
        }
        const bool hot = g.ui.Hot(ID_HUE + k);
        draw::StrokeRect(r.x, r.y, r.width, bh, px(1), hot ? CYAN : Rgba(255, 255, 255, .25f));
        const float kx = r.x + r.width * v;
        draw::FillRect(kx - px(3), r.y - px(4), px(6), bh + px(8), hot ? ACCENT_C : WHITE_C);
        if (changed) {
            if (k == 0) g.customH = v * 360; else *vals[k] = v;
            g.ChoosePaint(ToHex(ColorFromHSV(g.customH, g.customS, g.customV)));
        }
        yy += px(18) + bh + px(16);
    }
}

static void ControlsView(Game& g, Rectangle area) {
    bool back;
    float y = SubHead(g, area.x, area.y, "Controls", back);
    if (back) return;
    const float gap = px(30), w = (area.width - gap) / 2;
    auto panel = [&](float x, const char* tag, const std::vector<std::pair<std::string, std::string>>& keys) {
        float keyW = 0;
        for (auto& k : keys) keyW = std::max(keyW, TW(W700, k.first, 16));
        const float h = px(22) + px(18) + keys.size() * px(30) + px(12);
        Panel({x, y, w, h}, px(22), Rgba(14, 20, 36, .9f), Rgba(8, 11, 20, .9f), Rgba(47, 214, 255, .22f), CYAN, px(4));
        float yy = y + px(22);
        PreTag(tag, x + px(26), yy); yy += px(24);
        for (auto& k : keys) {
            Tx(W700, k.first, x + px(26), yy, 16, WHITE_C);
            Tx(W500, k.second, x + px(26) + keyW + px(18), yy, 16, Rgba(220, 226, 240, .75f));
            yy += px(30);
        }
    };
    panel(area.x, "Keyboard", {{"↑ / W", "Gas"}, {"↓ / S", "Brake and reverse"}, {"Left Right / A D", "Steer"}, {"Space", "Handbrake to kick the rear out"},
                               {"R", "Put the car back on the road"}, {"Esc / P", "Pause"}, {"M", "Mute or unmute"}, {"− / +", "Volume"}, {"F11", "Fullscreen"}});
    panel(area.x + w + gap, "Controller", {{"Left stick", "Steer"}, {"RT / LT", "Gas and brake, analog like pedals"}, {"A / RB", "Handbrake"}, {"Y", "Reset car"}, {"Start", "Pause"},
                                           {"D-pad", "Move through menus"}, {"B", "Back"}});
}

void Game::DrawMenus() {
    G = this; ::S = S;
    MenuBackground(*this);
    const float padX = std::max(px(20), W * 0.05f), padT = px(30), padB = px(18);
    const bool sub = view != V_MAIN;
    Logo(*this, padX, padT, sub);
    MenuStats(*this, W - padX, padT);
    const float vw = W / S / 100;
    const float logoH = sub ? px(Clamp(2.6f * vw, 26, 38)) + px(14) : px(Clamp(6.2f * vw, 40, 84)) * 1.76f + px(20);
    const float footH = px(24) + px(11) * 1.4f + px(6);
    const Rectangle area{padX, padT + logoH + H * (sub ? 0.02f : 0.03f), W - padX * 2, H - padB - footH - H * 0.025f - (padT + logoH + H * (sub ? 0.02f : 0.03f))};
    switch (view) {
        case V_MAIN: MainView(*this, area); break;
        case V_STAGES: StagesView(*this, area); break;
        case V_GARAGE: GarageView(*this, area); break;
        default: ControlsView(*this, area); break;
    }
    // footer: key hints and credits
    float x = padX, y = H - padB - footH;
    auto hint = [&](const std::string& t) { Tx(W700, t, x, y + px(5), 12, Rgba(220, 226, 240, .7f), 0, 0.12f); x += TW(W700, t, 12, 0.12f) + px(20); };
    if (ui.padNav) {
        Kbd("A", x, y, true, Hex(0x2ea043)); hint("SELECT");
        if (sub) { Kbd("B", x, y, true, Hex(0xd9443a)); hint("BACK"); }
    } else {
        Kbd("↑↓", x, y); hint("NAVIGATE");
        Kbd("ENTER", x, y); hint("SELECT");
        if (sub) { Kbd("ESC", x, y); hint("BACK"); }
    }
    const std::string credit = "Music: \"Liquid Flame\" by Of Far Different Nature (CC0). Engine sound: Supra dyno recording by editboy23 on Freesound (CC0).";
    Tx(W500, credit, std::max(x, W - padX - TW(W500, credit, 11)), H - padB - px(11) * 1.3f, 11, Rgba(220, 226, 240, .45f));
}

// ---------- HUD ----------
void Game::DrawHud(double speed) {
    G = this; ::S = S;
    const float top = px(16), side = px(20);
    // XP bar
    if (surv) {
        draw::FillRect(0, 0, W, px(7), Rgba(0, 0, 0, .5f));
        const float fw = W * (float)surv->xp / surv->next;
        draw::FillRect(0, 0, fw, px(10), Rgba(63, 169, 255, .25f));
        draw::FillRectGradH(0, 0, fw, px(7), Hex(0x3fa9ff), Hex(0x9be6ff));
    }
    // score card
    std::string sub1, sub2;
    bool warn = false;
    if (mode == MODE_TRACK && run) {
        sub1 = "Target " + FmtNum(T->pass);
        const double left = T->limit - run->t;
        sub2 = "Lap " + std::to_string(std::min(run->lap + 1, run->def->laps)) + "/" + std::to_string(run->def->laps) + "  ·  " + FmtT(left);
        warn = left < 10;
    } else if (surv) {
        sub1 = "Dawn in " + FmtT(600 - surv->t);
        sub2 = "Level " + std::to_string(surv->lvl) + "  ·  " + FmtNum(surv->kills) + " kills";
    } else sub1 = "Best drift " + FmtNum(best);
    const std::string sc = FmtNum(score);
    float cw = std::max({px(180), TW(W700, sc, 34) + px(36), TW(W700, Upper(sub1), 12, .12f) + px(40), TW(W700, Upper(sub2), 12, .12f) + px(40)});
    float ch = px(9) + px(11) * 1.3f + px(34) * 1.05f + px(3) + px(12) * 1.3f + (sub2.empty() ? 0 : px(3) + px(12) * 1.3f) + px(11);
    Panel({side, top + (surv ? px(6) : 0), cw, ch}, px(14), Rgba(10, 14, 26, .8f), Rgba(6, 9, 18, .62f), Rgba(47, 214, 255, .22f), CYAN, px(4));
    float x = side + px(18), y = top + (surv ? px(6) : 0) + px(9);
    Tx(W700, "SCORE", x, y, 11, CYAN, 0, 0.26f); y += px(11) * 1.3f;
    Tx(W700, sc, x, y, 34, WHITE_C, 10); y += px(34) * 1.05f + px(3);
    Tx(W700, Upper(sub1), x, y, 12, TEXT_C, 0, 0.12f); y += px(12) * 1.3f + px(3);
    if (!sub2.empty()) Tx(W700, Upper(sub2), x, y, 12, warn ? BAD : TEXT_C, 0, 0.12f);
    // minimap
    const Rectangle mr{side, top + (surv ? px(6) : 0) + ch + px(10), px(150), px(150)};
    Panel(mr, px(12), Rgba(6, 9, 18, .72f), Rgba(6, 9, 18, .72f), Rgba(47, 214, 255, .3f), BLANK, 0);
    DrawMini(mr);

    // speed card
    const std::string spd = std::to_string((int)JsRound(speed * 0.32));
    const float sw = std::max(px(200), TW(W700, spd, 46) + TW(W700, "KM/H", 12, .2f) + px(44));
    const float sh = px(9) + px(46) * 1.05f + px(4) + px(28) + px(11);
    const float sx = W - side - sw, sy = top + (surv ? px(6) : 0);
    Panel({sx, sy, sw, sh}, px(14), Rgba(10, 14, 26, .8f), Rgba(6, 9, 18, .62f), Rgba(47, 214, 255, .22f), CYAN, px(4), true);
    const float right = W - side - px(18);
    const float uw = TW(W700, "KM/H", 12, .2f);
    Tx(W700, "KM/H", right - uw, sy + px(9) + px(46) * 0.72f, 12, CYAN, 0, 0.2f);
    Tx(W700, spd, right - uw - px(8) - TW(W700, spd, 46), sy + px(9), 46, WHITE_C, 10);
    const float gy = sy + px(9) + px(46) * 1.05f + px(4);
    Skewed({right - px(30), gy, px(30), px(28)}, 10, ACCENT_C);
    const std::string gear = std::to_string(audio.Gear() + 1);
    Tx(W700, gear, right - px(15) - TW(W700, gear, 18) / 2, gy + px(14) - px(18) * 0.62f, 18, Hex(0x10131b), 10);
    const int lit = (int)JsRound(std::min(1.0f, audio.Rpm() / Audio::LIMIT) * 14);
    for (int k = 0; k < 14; k++) {
        const float segX = right - px(30) - px(10) - (14 - k) * px(10) + px(3);
        const bool on = k < lit, hot = k >= 11;
        Skewed({segX, gy + px(7), px(7), px(14)}, 14, on ? (hot ? Hex(0xff4f4f) : CYAN) : Rgba(255, 255, 255, .12f));
    }
    // reset and pause buttons (mouse)
    float bx = W - side;
    auto hudBtn = [&](const std::string& label, bool pauseIcon) {
        const float w = TW(W700, label, 13, .1f) + px(32) + (pauseIcon ? px(26) : 0), h = px(40);
        bx -= w;
        const Rectangle r{bx, sy + sh + px(10), w, h};
        const bool hover = CheckCollisionPointRec(GetMousePosition(), r);
        if (hover) SkewedGradH(r, 10, Rgba(47, 214, 255, .28f), Rgba(10, 14, 26, .82f));
        else Skewed(r, 10, Rgba(10, 14, 26, .82f));
        Skewed(r, 10, BLANK, hover ? Rgba(47, 214, 255, .6f) : Rgba(255, 255, 255, .18f), px(1));
        float tx = r.x + px(16);
        if (pauseIcon) {
            draw::FillRoundRect(tx + px(3), r.y + h / 2 - px(7), px(4), px(14), px(1), WHITE_C);
            draw::FillRoundRect(tx + px(11), r.y + h / 2 - px(7), px(4), px(14), px(1), WHITE_C);
            tx += px(26);
        }
        Tx(W700, label, tx, r.y + h / 2 - px(13) * 0.62f, 13, WHITE_C, 10, 0.1f);
        bx -= px(8);
        return ui.Clicked(r);
    };
    const bool driving = state == ST_RACE || state == ST_FREE || state == ST_COUNT;
    if (driving) {
        if (hudBtn("PAUSE", true)) Pause();
        if (hudBtn("RESET", false) && (state == ST_RACE || state == ST_FREE)) { Wreck(); ResetCar(); }
    }

    // drift combo
    static float comboA = 0;
    comboA += ((chain.active ? 1.0f : 0.0f) - comboA) * std::min(1.0f, GetFrameTime() / 0.08f);
    if (comboA > 0.01f) {
        const std::string pts = FmtNum(JsRound(chain.pts)), mult = "x" + std::to_string(chain.mult);
        const float cy = H * 0.22f, pw = TW(W700, pts, 52);
        TextGlow(W700, pts, W / 2 - pw / 2, cy, px(52), Alpha(Rgba(255, 207, 58, .55f), comboA), px(14), 10);
        TShadow(W700, pts, W / 2 - pw / 2, cy, 52, Alpha(ACCENT_C, comboA), 10, 0, 3, .5f * comboA);
        const float mw = TW(W700, mult, 17) + px(24), my = cy + px(52) + px(6);
        Skewed({W / 2 - mw / 2, my, mw, px(17) * 1.3f + px(4)}, 10, Alpha(CYAN, comboA));
        Tx(W700, mult, W / 2 - mw / 2 + px(12), my + px(2), 17, Alpha(Hex(0x061018), comboA), 10);
    }
    // toast and zone messages
    if (toast.alpha > 0.01f) {
        const float tw = TW(W700, toast.text, 30);
        TextGlow(W700, toast.text, W / 2 - tw / 2, H * 0.34f, px(30), Alpha(toast.color, toast.alpha * 0.6f), px(12), 10);
        TShadow(W700, toast.text, W / 2 - tw / 2, H * 0.34f, 30, Alpha(toast.color, toast.alpha), 10, 0, 3, .6f * toast.alpha);
    }
    if (zmsg.alpha > 0.01f) {
        const std::string t = Upper(zmsg.text);
        const float tw = TW(W700, t, 19, .08f), bw = tw + px(32), bh = px(19) * 1.25f + px(10);
        Skewed({W / 2 - bw / 2, H * 0.41f, bw, bh}, 10, Rgba(6, 9, 18, .7f * zmsg.alpha));
        Tx(W700, t, W / 2 - tw / 2, H * 0.41f + px(5), 19, Alpha(zmsg.color, zmsg.alpha), 10, 0.08f);
    }
    if (mode == MODE_TRACK && run && run->wrongT > 0.7 && state == ST_RACE) {
        const float tw = TW(W700, "WRONG WAY", 30, .1f), bw = tw + px(60), bh = px(30) * 1.2f + px(20);
        Skewed({W / 2 - bw / 2 - px(8), H / 2 - bh / 2 - px(8), bw + px(16), bh + px(16)}, 10, Rgba(255, 40, 40, .18f));
        Skewed({W / 2 - bw / 2, H / 2 - bh / 2, bw, bh}, 10, Rgba(200, 30, 40, .88f));
        Tx(W700, "WRONG WAY", W / 2 - tw / 2, H / 2 - px(30) * 0.62f, 30, WHITE_C, 10, 0.1f);
    }
    // countdown / intro banner
    if (bannerOn) {
        const float bw = std::min(W * 0.9f, px(520));
        const auto lines = WrapText(W500, bannerD, px(15), bw - px(36));
        const float nH = bannerN.empty() ? 0 : px(120);
        const float dH = lines.size() * px(15) * 1.4f + px(20);
        const float total = nH + px(8) + px(28) * 1.2f + px(10) + dH;
        float yy = H * 0.3f - total / 2;
        if (!bannerN.empty()) {
            const float nw = TW(W700, bannerN, 120);
            TextGlow(W700, bannerN, W / 2 - nw / 2, yy, px(120), Rgba(255, 207, 58, .6f), px(24), 10);
            TShadow(W700, bannerN, W / 2 - nw / 2, yy, 120, ACCENT_C, 10, 0, 5, .5f);
        }
        yy += nH + px(8);
        const std::string t = Upper(bannerT);
        TShadow(W700, t, W / 2 - TW(W700, t, 28) / 2, yy, 28, WHITE_C, 10, 0, 3, .6f);
        yy += px(28) * 1.2f + px(10);
        float lw = 0;
        for (auto& l : lines) lw = std::max(lw, TW(W500, l, 15));
        const Rectangle d{W / 2 - (lw + px(36)) / 2, yy, lw + px(36), dH};
        draw::FillRect(d.x, d.y, d.width, d.height, Rgba(6, 9, 18, .78f));
        draw::FillRect(d.x, d.y, px(4), d.height, CYAN);
        float ly = d.y + px(10);
        for (auto& l : lines) { Tx(W500, l, d.x + px(20), ly, 15, Rgba(220, 226, 240, .9f)); ly += px(15) * 1.4f; }
    }
}

// ---------- overlays: pause, results, level-up ----------
static Rectangle Card(Game& g, float maxW, float h, const char* pre, const std::string& title, Color titleCol, float titleFs) {
    draw::FillRect(0, 0, g.W, g.H, Rgba(4, 7, 16, .72f));
    const float w = std::min(maxW, g.W - px(40));
    const Rectangle r{g.W / 2 - w / 2, g.H / 2 - h / 2, w, h};
    Panel(r, px(20), Rgba(14, 20, 36, .95f), Rgba(8, 11, 20, .95f), Rgba(47, 214, 255, .25f), CYAN, px(4));
    PreTag(pre, r.x + px(26) + px(3), r.y + px(26));
    Tx(W700, Upper(title), r.x + px(26), r.y + px(26) + px(16), titleFs, titleCol, 10);
    return r;
}

void Game::DrawOverlays() {
    G = this; ::S = S;
    if (state == ST_PAUSE) {
        const float pad = px(24), h = pad + px(16) + px(28) * 1.2f + px(14) + px(52) + px(18) + px(50) * 3 + px(8) * 2 + px(26);
        const Rectangle r = Card(*this, px(380), h, "Sideways City", "Paused", WHITE_C, 28);
        float y = r.y + pad + px(16) + px(28) * 1.2f + px(14);
        const float x = r.x + pad, w = r.width - pad * 2, u = (w - px(8) * 5) / 6;
        if (Button(ID_RESUME, {x, y, w, px(52)}, "Resume", PRIMARY, 18, UI_SELECT, true)) { Resume(); return; }
        y += px(52) + px(18);
        auto saveVol = [&]() { save.SetInt("vol", audio.Volume()); save.SetBool("muted", audio.Muted()); Save(); };
        if (Button(ID_VOLDN, {x, y, u, px(50)}, "−", GHOST, 22, UI_SELECT)) { audio.SetVolume(audio.Volume() - 1); saveVol(); }
        if (Button(ID_VOLUP, {x + (u + px(8)) * 5, y, u, px(50)}, "+", GHOST, 22, UI_SELECT)) { audio.SetVolume(audio.Volume() + 1); saveVol(); }
        {   // volume label with a 10-step meter
            const float vx = x + u + px(8), vw = u * 4 + px(8) * 3;
            const std::string L = "VOLUME " + std::to_string(audio.Volume());
            Tx(W700, L, vx + vw / 2 - TW(W700, L, 13, .1f) / 2, y + px(6), 13, Rgba(220, 226, 240, .8f), 0, 0.1f);
            const float mw = 10 * px(10) + 9 * px(4), mx = vx + vw / 2 - mw / 2;
            for (int k = 0; k < 10; k++) Skewed({mx + k * px(14), y + px(28), px(10), px(12)}, 14, k < audio.Volume() ? CYAN : Rgba(255, 255, 255, .14f));
        }
        y += px(50) + px(8);
        if (Button(ID_MUTE, {x, y, w, px(50)}, audio.Muted() ? "Sound off" : "Sound on", GHOST, 15, UI_SELECT, false, audio.Muted() ? BAD : BLANK)) { audio.SetMuted(!audio.Muted()); saveVol(); }
        y += px(50) + px(8);
        const float half = (w - px(8)) / 2;
        if (Button(ID_RESET, {x, y, half, px(50)}, "Reset car", GHOST, 15, UI_SELECT)) {
            Resume();
            if (state == ST_RACE || state == ST_FREE) { Wreck(); ResetCar(); }
            return;
        }
        if (Button(ID_QUITMENU, {x + half + px(8), y, half, px(50)}, "Quit to menu", GHOST, 15, UI_SELECT)) { ShowMenu(); return; }
        return;
    }
    if (state == ST_DONE && resultOn) {
        const float pad = px(26), w = std::min(px(420), W - px(40)) - pad * 2;
        const float starsH = rStarsShown ? px(40) + px(20) : 0;
        const float h = pad + px(16) + px(32) * 1.2f + px(10) + starsH + ParagraphH(rScore, w, 16, 1.45f) + px(8) + ParagraphH(rBest, w, 16, 1.45f) + px(8) + px(18) + px(44) + pad;
        const Rectangle r = Card(*this, px(420), h, "Results", rTitle, rGood ? ACCENT_C : BAD, 32);
        float y = r.y + pad + px(16) + px(32) * 1.2f + px(10);
        const float x = r.x + pad;
        if (rStarsShown) { Stars(x, y, px(40), rStars, rSurv ? 3 : 3, px(10), ACCENT_C); y += starsH; }
        y = Paragraph(rScore, x, y, w, 16, 1.45f, MUTED) + px(8);
        y = Paragraph(rBest, x, y, w, 16, 1.45f, MUTED) + px(8) + px(18);
        const int n = rNext ? 3 : 2;
        const float bw = (w - px(10) * (n - 1)) / n;
        float bx = x;
        const int stage = run ? run->i : 0;
        if (rNext) {
            if (Button(ID_NEXT, {bx, y, bw, px(44)}, "Next stage", PRIMARY, 15, UI_START, true)) { StartStage(stage + 1); return; }
            bx += bw + px(10);
        }
        if (Button(ID_RETRY, {bx, y, bw, px(44)}, "Retry", GHOST, 15, UI_START, !rNext)) {
            if (rSurv) StartSurvival(); else StartStage(stage);
            return;
        }
        bx += bw + px(10);
        if (Button(ID_MENU, {bx, y, bw, px(44)}, "Menu", GHOST, 15, UI_SELECT)) { ShowMenu(rSurv ? V_MAIN : V_STAGES); return; }
        return;
    }
    if (state == ST_LEVELUP && surv) {
        const float pad = px(26), maxW = px(780), w = std::min(maxW, W - px(40)) - pad * 2, cw = (w - px(24)) / 3;
        float cardH = px(150);
        for (int k : upOpts) {
            const int l = k == U_REPAIR ? 0 : surv->up[k] + 1;
            cardH = std::max(cardH, px(14) * 2 + px(17) * 1.3f + px(6) + px(13) * 1.3f + px(6) + ParagraphH(UpDesc(k, l), cw - px(32), 14, 1.35f));
        }
        const float h = pad + px(16) + px(30) * 1.2f + px(4) + px(16) * 1.5f + px(14) + cardH + pad;
        const Rectangle r = Card(*this, maxW, h, "Upgrade", "Level " + std::to_string(surv->lvl - surv->pendingLv), ACCENT_C, 30);
        float y = r.y + pad + px(16) + px(30) * 1.2f + px(4);
        Tx(W500, "Pick an upgrade.", r.x + pad, y, 16, MUTED);
        y += px(16) * 1.5f + px(14);
        for (size_t i = 0; i < upOpts.size(); i++) {
            const int k = upOpts[i];
            const int l = k == U_REPAIR ? 0 : surv->up[k] + 1;
            const Rectangle c{r.x + pad + i * (cw + px(12)), y, cw, cardH};
            const bool enabled = upLock <= 0;
            const bool act = ui.Item(ID_CARD + (int)i, c, enabled, i == 0);
            const bool hot = ui.Hot(ID_CARD + (int)i) && enabled;
            const bool evo = k == U_INFERNO;
            const float a = enabled ? 1 : 0.55f;
            if (evo) Panel(c, px(14), Rgba(255, 122, 26, .25f * a), Rgba(10, 14, 26, .9f * a), hot ? Rgba(47, 214, 255, .5f) : Rgba(255, 255, 255, .14f * a), Hex(0xff7a1a), px(4));
            else if (hot) Panel(c, px(14), Rgba(47, 214, 255, .24f), Rgba(10, 14, 26, .9f), Rgba(47, 214, 255, .5f), ACCENT_C, px(4));
            else Panel(c, px(14), Alpha(NAVY, a), Alpha(NAVY, a), Rgba(255, 255, 255, .14f * a), BLANK, 0);
            float cy = c.y + px(14);
            const std::string num = std::to_string(i + 1);
            Tx(W500, num, c.x + c.width - px(12) - TW(W500, num, 12), c.y + px(10), 12, Alpha(MUTED, a));
            Tx(W700, Upper(UpName(k)), c.x + px(18), cy, 17, Alpha(WHITE_C, a), 0, 0.03f);
            cy += px(17) * 1.3f + px(6);
            if (evo) Tx(W700, "EVOLUTION", c.x + px(18), cy, 13, Alpha(CYAN, a), 0, 0.15f);
            else if (k != U_REPAIR) {
                for (int d = 0; d < 5; d++) {
                    const float dx = c.x + px(18) + d * px(15) + px(5), dy = cy + px(7);
                    if (d < l) draw::FillCircle(dx, dy, px(5), Alpha(CYAN, a));
                    else draw::StrokeCircle(dx, dy, px(4.5f), px(1.2f), Alpha(CYAN, a));
                }
                if (l == 1) Tx(W700, "NEW", c.x + px(18) + 5 * px(15) + px(10), cy, 13, Alpha(CYAN, a), 0, 0.15f);
            }
            cy += px(13) * 1.3f + px(6);
            Paragraph(UpDesc(k, l), c.x + px(18), cy, c.width - px(32), 14, 1.35f, Alpha(MUTED, a));
            if (act) { audio.Ui(UI_SELECT); ChooseUp(k); return; }
        }
    }
}
