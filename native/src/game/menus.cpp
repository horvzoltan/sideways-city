// menus.cpp - the main menu, the HUD and the overlays (pause, results), in the calm style of the
// desert: soft rounded panels of warm dark glass, upright cream text, small letter-spaced labels,
// pill-shaped buttons and a sun-gold accent. Sizes are design pixels times the UI scale S.
#include "game.h"
#include "rlgl.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>

namespace {
Game* G = nullptr;
float S = 1;
float px(float v) { return v * S; }

// ---- palette ----
const Color CREAM = Hex(0xfbf4ea);            // text
const Color SOFT = Hex(0xfbf4ea, 0.72f);      // body text
const Color FAINT = Hex(0xfbf4ea, 0.45f);     // labels
const Color LINE = Hex(0xfbf4ea, 0.14f);      // hairlines
const Color SUN = Hex(0xf4b56a);              // highlight
const Color STAR = Hex(0xf2c46b);
const Color GLASS = Rgba(34, 24, 34, 0.62f);  // soft warm dark glass
const Color GLASS_HI = Rgba(255, 244, 230, 0.13f);
const Color DARK_TEXT = Hex(0x2a2028);

enum Id {
    ID_MAIN = 100, ID_BACK = 200, ID_STAGE = 300, ID_SWATCH = 400, ID_CUSTOM = 408, ID_HUE = 410, ID_SAT = 411, ID_VAL = 412,
    ID_NEXT = 500, ID_RETRY = 501, ID_MENU = 502,
    ID_RESUME = 600, ID_VOLDN = 601, ID_VOLUP = 602, ID_MUTE = 603, ID_RESET = 604, ID_QUITMENU = 605,
};

std::string Upper(std::string s) { for (char& c : s) c = (char)toupper((unsigned char)c); return s; }

// ---- shapes ----
void Glass(Rectangle r, float radius, float alpha = 1) {   // a soft rounded panel with a faint edge
    draw::FillRoundRect(r.x, r.y, r.width, r.height, radius, Alpha(GLASS, alpha));
    draw::StrokeRoundRect(r.x, r.y, r.width, r.height, radius, px(1), Alpha(LINE, alpha));
}
void Pill(Rectangle r, Color fill, Color edge = BLANK) {
    const float rad = r.height / 2;
    if (fill.a) draw::FillRoundRect(r.x, r.y, r.width, r.height, rad, fill);
    if (edge.a) draw::StrokeRoundRect(r.x, r.y, r.width, r.height, rad, px(1.2f), edge);
}
void Star(float cx, float cy, float r, bool filled, Color c) {
    Vector2 p[10];
    for (int i = 0; i < 10; i++) {
        float a = -PI / 2 + i * PI / 5, rr = i % 2 ? r * 0.45f : r;
        p[i] = {cx + std::cos(a) * rr, cy + std::sin(a) * rr};
    }
    if (filled) draw::FillFan({cx, cy}, p, 10, c);
    else draw::StrokePoly(p, 10, std::max(1.0f, r * 0.1f), Alpha(c, 0.6f));
}
void Stars(float x, float y, float size, int n, int total, float gap, Color c) {   // x, y: top left
    for (int i = 0; i < total; i++) Star(x + size / 2 + i * (size + gap), y + size / 2, size / 2, i < n, c);
}

// ---- text ----
float TW(FontWeight w, const std::string& s, float size, float ls = 0) { return TextWidth(w, s, px(size), px(size) * ls); }
void Tx(FontWeight w, const std::string& s, float x, float y, float size, Color c, float ls = 0) { Text(w, s, x, y, px(size), c, 0, px(size) * ls); }
void TxShadow(FontWeight w, const std::string& s, float x, float y, float size, Color c, float ls = 0) {   // soft shadow for text over the world
    Tx(w, s, x, y + px(2), size, Rgba(30, 18, 26, 0.35f * c.a / 255.0f), ls);
    Tx(w, s, x, y, size, c, ls);
}
void TxCenter(FontWeight w, const std::string& s, float cx, float y, float size, Color c, float ls = 0) { Tx(w, s, cx - TW(w, s, size, ls) / 2, y, size, c, ls); }
void Label(const std::string& s, float x, float y, Color c = FAINT) { Tx(W700, Upper(s), x, y, 11, c, 0.28f); }   // small spaced capitals
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
        if (hot) Pill({r.x - px(4), r.y - px(4), r.width + px(8), r.height + px(8)}, Hex(0xfbf4ea, 0.18f));
        Pill(r, hot ? CREAM : Hex(0xfbf4ea, 0.88f));
    } else Pill(r, hot ? GLASS_HI : BLANK, hot ? Hex(0xfbf4ea, 0.75f) : Hex(0xfbf4ea, 0.3f));
    const std::string L = Upper(label);
    const Color tc = textCol.a ? textCol : st == PRIMARY ? DARK_TEXT : CREAM;
    TxCenter(W700, L, r.x + r.width / 2, r.y + r.height / 2 - px(fs) * 0.6f, fs, tc, 0.16f);
    if (act) G->audio.Ui(sound);
    return act;
}

float Stats(const std::vector<std::array<std::string, 3>>& rows, float x, float y, float w, int cols) {   // label / value (/ "gold" / "stars")
    if (rows.empty()) return y;
    draw::FillRect(x, y, w, px(1), LINE);
    y += px(16);
    const float colW = w / cols;
    float labelW = 0;
    for (auto& r : rows) labelW = std::max(labelW, TW(W700, Upper(r[0]), 11, 0.28f));
    labelW = std::min(labelW + px(18), colW * 0.62f);
    for (size_t i = 0; i < rows.size(); i++) {
        const float cx = x + (i % cols) * colW, cy = y + (i / cols) * px(28);
        Label(rows[i][0], cx, cy + px(3));
        if (rows[i][2] == "stars") Stars(cx + labelW, cy + px(1), px(15), std::stoi(rows[i][1]), 3, px(4), STAR);
        else Tx(W500, rows[i][1], cx + labelW, cy, 16, rows[i][2] == "gold" ? SUN : CREAM);
    }
    return y + ((rows.size() + cols - 1) / cols) * px(28);
}

void Kbd(const std::string& key, float& x, float y, bool pad = false, Color padCol = BLANK) {
    const float h = px(24), w = pad ? h : std::max(px(26), TW(W700, key, 11) + px(16));
    if (pad) draw::FillCircle(x + w / 2, y + h / 2, h / 2, padCol);
    else Pill({x, y, w, h}, BLANK, Hex(0xfbf4ea, 0.35f));
    TxCenter(W700, key, x + w / 2, y + h / 2 - px(11) * 0.6f, 11, CREAM);
    x += w + px(8);
}
}  // namespace

// ---------- main menu ----------
static void MenuBackground(Game& g) {   // soften the desert behind the menu, darker on the left where the text is
    const float W = g.W, H = g.H;
    const Color c = Rgba(30, 20, 32, 1);
    const float stops[4][2] = {{0, .62f}, {.38f, .42f}, {.7f, .12f}, {1, .28f}};
    for (int i = 0; i < 3; i++)
        draw::FillRectGradH(W * stops[i][0], 0, W * (stops[i + 1][0] - stops[i][0]), H, Alpha(c, stops[i][1]), Alpha(c, stops[i + 1][1]));
    draw::FillRectGradV(0, H * 0.65f, W, H * 0.35f, Alpha(c, 0), Alpha(c, 0.35f));
}

static void Logo(Game& g, float x, float y, bool small) {
    const float vw = g.W / S / 100;
    if (!small) {
        const float fs = Clamp(5.6f * vw, 38, 76), lh = fs * 1.02f;
        TxShadow(W500, "SIDEWAYS", x, y, fs, CREAM, 0.12f);
        TxShadow(W500, "CITY", x, y + px(lh), fs, SUN, 0.12f);
        // a thin horizon with a small sun resting on it
        const float w = TW(W500, "SIDEWAYS", fs, 0.12f), by = y + px(lh * 2) + px(16);
        draw::FillRectGradH(x, by, w * 0.7f, px(1.5f), Hex(0xfbf4ea, 0.6f), Hex(0xfbf4ea, 0));
        draw::FillSector(x + w * 0.16f, by, px(9), PI, 2 * PI, SUN);
    } else {
        const float fs = Clamp(2.2f * vw, 22, 32);
        TxShadow(W500, "SIDEWAYS", x, y, fs, CREAM, 0.12f);
        TxShadow(W500, "CITY", x + TW(W500, "SIDEWAYS ", fs, 0.12f), y, fs, SUN, 0.12f);
    }
}

static void MenuStats(Game& g, float right, float y) {
    int stars = 0;
    for (size_t i = 0; i < STAGES.size(); i++) stars += g.Prog((int)i).stars;
    const std::string vals[2] = {std::to_string(stars) + " / " + std::to_string(STAGES.size() * 3), FmtNum(g.best)};
    const char* labels[2] = {"Stars", "Best drift"};
    float x = right;
    for (int i = 1; i >= 0; i--) {
        const float w = std::max(TW(W700, Upper(labels[i]), 11, 0.28f), TW(W500, vals[i], 22));
        Label(labels[i], x - TW(W700, Upper(labels[i]), 11, 0.28f), y);
        TxShadow(W500, vals[i], x - TW(W500, vals[i], 22), y + px(16), 22, CREAM);
        x -= w + px(32);
    }
}

// The info panel: label, title, text and stats.
static float InfoPanel(Rectangle r, const std::string& tag, const std::string& title, const std::string& text,
                       const std::vector<std::array<std::string, 3>>& stats, int cols, bool render = true) {
    const float pad = px(28), w = r.width - pad * 2;
    const float titleFs = Clamp(2.3f * G->W / S / 100, 24, 32);
    const float h = px(26) + px(11) + px(10) + px(titleFs) * 1.1f + px(12) + ParagraphH(text, w, 15, 1.6f) + px(10) +
                    (stats.empty() ? 0 : px(16) + ((stats.size() + cols - 1) / cols) * px(28)) + px(22);
    if (!render) return h;
    r.height = h;
    Glass(r, px(20));
    float y = r.y + px(26), x = r.x + pad;
    Label(tag, x, y, SAND_C); y += px(11) + px(10);
    Tx(W500, title, x, y, titleFs, CREAM, 0.02f); y += px(titleFs) * 1.1f + px(12);
    y = Paragraph(text, x, y, w, 15, 1.6f, SOFT) + px(10);
    Stats(stats, x, y, w, cols);
    return h;
}

static void MainView(Game& g, Rectangle area) {
    struct Item { const char* title; int mode; };
    std::vector<Item> items = {{"Stages", 0}, {"Free drive", 2}, {"Garage", 3}, {"Controls", 4}};
    if (g.desktopQuit) items.push_back({"Quit", 5});
    const float vw = g.W / S / 100, fs = Clamp(2.2f * vw, 22, 32), itemH = px(fs) * 1.3f + px(18), gap = px(2);
    const float listW = std::min(px(420), area.width * 0.4f);
    const float listH = items.size() * itemH + (items.size() - 1) * gap;
    float y = area.y + std::max(0.0f, (area.height - listH) / 2);
    int sel = 0;
    for (size_t i = 0; i < items.size(); i++) if (g.ui.Hot(ID_MAIN + (int)i)) sel = (int)i;
    for (size_t i = 0; i < items.size(); i++) {
        const int id = ID_MAIN + (int)i;
        const Rectangle r{area.x, y, listW, itemH};
        const bool act = g.ui.Item(id, r, true, i == 0);
        const bool hot = g.ui.Hot(id);
        const float ty = r.y + r.height / 2 - px(fs) * 0.6f;
        if (hot) draw::FillCircle(r.x + px(6), r.y + r.height / 2, px(4.5f), SUN);   // a small sun marks the choice
        TxShadow(W500, Upper(items[i].title), r.x + px(26), ty, fs, hot ? CREAM : Hex(0xfbf4ea, 0.5f), 0.1f);
        if (act) {
            const int m = items[i].mode;
            if (m == 0) { g.audio.Ui(UI_SELECT); g.view = V_STAGES; g.ui.ResetFocus(); return; }
            if (m == 2) { g.audio.Ui(UI_START); g.StartFree(); return; }
            if (m == 3) { g.audio.Ui(UI_SELECT); g.view = V_GARAGE; g.ui.ResetFocus(); return; }
            if (m == 4) { g.audio.Ui(UI_SELECT); g.view = V_CONTROLS; g.ui.ResetFocus(); return; }
            if (m == 5) { g.audio.Ui(UI_SELECT); g.quit = true; return; }
        }
        y += itemH + gap;
    }
    // about the chosen item
    int stars = 0, cleared = 0;
    for (size_t i = 0; i < STAGES.size(); i++) { int s = g.Prog((int)i).stars; stars += s; if (s) cleared++; }
    const std::string n = std::to_string(STAGES.size());
    std::string tag, title, text;
    std::vector<std::array<std::string, 3>> st;
    switch (items[sel].mode) {
        case 0:
            tag = "Career"; title = "Stages";
            text = n + " compact drift stages across the desert, from soft dunes at dawn to a narrow ridge before first light. Stay sideways through the drift zones, clip the orange markers and beat the target before the clock runs out.";
            st = {{"Cleared", std::to_string(cleared) + " / " + n, ""}, {"Stars", std::to_string(stars) + " / " + std::to_string(STAGES.size() * 3), ""}};
            break;
        case 2:
            tag = "Free roam"; title = "Free drive";
            text = "Cruise the open desert at your own pace: dunes, lakes, salt flats and old ruins, from dawn through the night and the odd sandstorm. It never ends. Chain drifts anywhere.";
            st = {{"Best drift", FmtNum(g.best), ""}};
            break;
        case 3: {
            tag = "Customise"; title = "Garage";
            text = "Pick a paint for your car. Eight classic colours or any custom shade you like.";
            std::string name = "Custom";
            for (auto& p : PAINTS) if (p.hex == g.paintHex) name = p.name;
            st = {{"Paint", name, ""}};
            break;
        }
        case 4: tag = "Help"; title = "Controls"; text = "Keyboard and controller controls. The controller's triggers work like pedals."; break;
        default: tag = "Exit"; title = "Quit"; text = "Close the game and return to the desktop."; break;
    }
    const float px0 = area.x + listW + g.W * 0.05f, pw = std::min(px(520), area.x + area.width - px0);
    const float ph = InfoPanel({px0, 0, pw, 0}, tag, title, text, st, 1, false);
    InfoPanel({px0, area.y + (area.height - ph) / 2, pw, 0}, tag, title, text, st, 1);
}

static bool BackButton(Game& g, float x, float y, float& outW) {
    const std::string L = "‹  BACK";
    const float w = TW(W700, L, 12, 0.16f) + px(36), h = px(34);
    outW = w;
    const Rectangle r{x, y, w, h};
    const bool act = g.ui.Item(ID_BACK, r);
    const bool hot = g.ui.Hot(ID_BACK);
    Pill(r, hot ? GLASS_HI : BLANK, hot ? Hex(0xfbf4ea, 0.75f) : Hex(0xfbf4ea, 0.3f));
    TxCenter(W700, L, r.x + w / 2, r.y + h / 2 - px(12) * 0.6f, 12, CREAM, 0.16f);
    if (act) { g.audio.Ui(UI_BACK); g.view = V_MAIN; g.ui.ResetFocus(); }
    return act;
}

static float SubHead(Game& g, float x, float y, const std::string& title, bool& back) {
    float bw;
    back = BackButton(g, x, y + px(4), bw);
    const float fs = Clamp(2.6f * g.W / S / 100, 24, 36);
    TxShadow(W500, title, x + bw + px(22), y + px(21) - px(fs) * 0.6f, fs, CREAM, 0.03f);
    return y + std::max(px(fs), px(40)) + px(20);
}

static void StagesView(Game& g, Rectangle area) {
    bool back;
    float y = SubHead(g, area.x, area.y, "Stage select", back);
    if (back) return;
    const float gap = px(30), leftW = (area.width - gap) / 2.1f, rightX = area.x + leftW + gap, rightW = area.width - leftW - gap;
    // the stages in two columns
    const float colGap = px(12), rowGap = px(6), colW = (leftW - colGap) / 2, rowH = px(15) * 1.2f + px(12) * 1.3f + px(20);
    int sel = -1;
    for (size_t i = 0; i < STAGES.size(); i++) if (g.ui.Hot(ID_STAGE + (int)i)) sel = (int)i;
    for (size_t i = 0; i < STAGES.size(); i++) {
        const int perCol = (int)(STAGES.size() + 1) / 2, col = (int)i / perCol, row = (int)i % perCol;
        const Rectangle r{area.x + col * (colW + colGap), y + row * (rowH + rowGap), colW, rowH};
        const bool open = i == 0 || g.Prog((int)i - 1).stars > 0;
        const auto p = g.Prog((int)i);
        const int id = ID_STAGE + (int)i;
        const bool act = g.ui.Item(id, r, open, (int)i == g.stageSel);
        const bool hot = g.ui.Hot(id) && open;
        const float a = open ? 1 : 0.38f;
        if (hot) { draw::FillRoundRect(r.x, r.y, r.width, r.height, px(14), GLASS_HI); draw::StrokeRoundRect(r.x, r.y, r.width, r.height, px(14), px(1), Hex(0xfbf4ea, 0.35f)); }
        else draw::FillRoundRect(r.x, r.y, r.width, r.height, px(14), Alpha(GLASS, 0.7f * a));
        char num[4];
        std::snprintf(num, sizeof num, "%02d", (int)i + 1);
        Tx(W500, num, r.x + px(16), r.y + r.height / 2 - px(20) * 0.6f, 20, hot ? SUN : Hex(0xfbf4ea, 0.4f * a));
        const float nx = r.x + px(16) + px(34) + px(10);
        const float starsW = px(11) * 3 + px(3) * 2;
        std::string name = Upper(STAGES[i].name);
        while (name.size() > 3 && TW(W700, name, 13, 0.08f) > r.x + r.width - px(16) - starsW - px(10) - nx) name = name.substr(0, name.size() - 4) + "...";
        Tx(W700, name, nx, r.y + px(10), 13, Alpha(CREAM, a), 0.08f);
        const std::string bs = open ? (p.best ? "Best " + FmtNum(p.best) : "Not cleared yet") : "Locked · clear stage " + std::to_string(i);
        Tx(W500, bs, nx, r.y + px(10) + px(13) * 1.5f, 12, Hex(0xfbf4ea, 0.5f * a));
        Stars(r.x + r.width - px(16) - starsW, r.y + r.height / 2 - px(5.5f), px(11), p.stars, 3, px(3), Alpha(STAR, a));
        if (act) { g.audio.Ui(UI_START); g.stageSel = (int)i; g.StartStage((int)i); return; }
    }
    if (sel >= 0) g.stageSel = sel;
    // the chosen stage, with its track
    const int i = g.stageSel;
    const StageDef& def = STAGES[i];
    static std::vector<std::unique_ptr<Track>> meta(STAGES.size());
    if (!meta[i]) { meta[i] = std::make_unique<Track>(BuildTrack(def)); StageGoals(*meta[i], i); }
    const Track& Tm = *meta[i];
    const auto p = g.Prog(i);
    const float prevH = std::min(g.H * 0.3f, px(300)), prevW = std::min(rightW - px(56), prevH * 520 / 340);
    std::vector<std::array<std::string, 3>> st = {
        {"Target", FmtNum(Tm.pass), ""}, {"Laps", std::to_string(def.laps), ""}, {"Time limit", FmtT(Tm.limit), ""},
        {"Drift zones", std::to_string(Tm.zones.size()), ""}, {"Best", p.best ? FmtNum(p.best) : "None yet", ""},
        {"Rating", std::to_string(p.stars), "stars"}, {"Ghost", g.GhostExists(i) ? "Saved" : "None", ""}};
    const float pad = px(28), w = rightW - pad * 2;
    const float titleFs = Clamp(2.3f * g.W / S / 100, 24, 32);
    const float h = px(22) + prevW * 340 / 520 + px(14) + px(21) + px(titleFs) * 1.1f + px(10) + ParagraphH(def.desc, w, 15, 1.6f) + px(10) + px(16) + 4 * px(28) + px(22);
    const Rectangle r{rightX, y, rightW, h};
    Glass(r, px(20));
    float yy = r.y + px(22);
    g.DrawTrackPreview(i, {r.x + r.width / 2 - prevW / 2, yy, prevW, prevW * 340 / 520});
    yy += prevW * 340 / 520 + px(14);
    Label("Stage " + std::to_string(i + 1) + "  ·  " + def.sky, r.x + pad, yy, SAND_C); yy += px(21);
    Tx(W500, def.name, r.x + pad, yy, titleFs, CREAM, 0.02f); yy += px(titleFs) * 1.1f + px(10);
    yy = Paragraph(def.desc, r.x + pad, yy, w, 15, 1.6f, SOFT) + px(10);
    Stats(st, r.x + pad, yy, w, 2);
}

static void GarageView(Game& g, Rectangle area) {
    bool back;
    float y = SubHead(g, area.x, area.y, "Garage", back);
    if (back) return;
    const float gap = px(30), leftW = (area.width - gap) * 1.5f / 2.5f, rightX = area.x + leftW + gap, rightW = area.width - leftW - gap;
    const float showH = std::min(leftW / 2, area.y + area.height - y), showW = showH * 2;
    const Rectangle show{area.x, y, showW, showH};
    // the showroom: soft glass with warm light pooling under the car
    Glass(show, px(24));
    rlBegin(RL_TRIANGLES);
    for (int k = 0; k < 64; k++) {
        const float t0 = 2 * PI * k / 64, t1 = 2 * PI * (k + 1) / 64, cx = show.x + show.width / 2, cy = show.y + show.height * 0.62f;
        const float rx = show.width * 0.34f, ry = show.height * 0.36f;
        const Color c0 = Rgba(244, 181, 106, .2f), c1 = Rgba(244, 181, 106, 0);
        rlColor4ub(c0.r, c0.g, c0.b, c0.a); rlVertex2f(cx, cy);
        rlColor4ub(c1.r, c1.g, c1.b, c1.a); rlVertex2f(cx + std::cos(t0) * rx, cy + std::sin(t0) * ry);
        rlColor4ub(c1.r, c1.g, c1.b, c1.a); rlVertex2f(cx + std::cos(t1) * rx, cy + std::sin(t1) * ry);
    }
    rlEnd();
    rlDrawRenderBatchActive();
    BeginScissorMode((int)show.x, (int)show.y, (int)show.width, (int)show.height);
    g.DrawGarageCar(show);
    EndScissorMode();
    // paint
    float x = rightX, yy = y + std::max(0.0f, (showH - px(300)) / 2);
    Label("Paint", x, yy, SAND_C); yy += px(20);
    std::string name = "Custom";
    for (auto& p : PAINTS) if (p.hex == g.paintHex) name = p.name;
    TxShadow(W500, name, x, yy, 28, CREAM, 0.02f); yy += px(28) + px(20);
    const float sw = px(40), sg = px(14);
    const int perRow = std::max(1, (int)((rightW + sg) / (sw + sg)));
    for (int i = 0; i <= 8; i++) {
        const float cx = x + (i % perRow) * (sw + sg), cy = yy + (i / perRow) * (sw + sg);
        const Rectangle r{cx, cy, sw, sw};
        const int id = i < 8 ? ID_SWATCH + i : ID_CUSTOM;
        const bool chosen = i < 8 ? PAINTS[i].hex == g.paintHex : std::none_of(std::begin(PAINTS), std::end(PAINTS), [&](const PaintPreset& p) { return p.hex == g.paintHex; });
        const bool act = g.ui.Item(id, r, true, chosen);
        const bool hot = g.ui.Hot(id);
        const float c = sw / 2;
        if (chosen || hot) draw::StrokeCircle(cx + c, cy + c, c + px(5), px(1.5f), chosen ? CREAM : Hex(0xfbf4ea, 0.5f));
        if (i < 8) draw::FillCircle(cx + c, cy + c, c, Hex(PAINTS[i].hex));
        else for (int s = 0; s < 36; s++) draw::FillSector(cx + c, cy + c, c, -PI / 2 + s * PI / 18, -PI / 2 + (s + 1) * PI / 18, ColorFromHSV(s * 10.0f, 0.55f, 0.95f));   // soft rainbow
        if (act) {
            g.audio.Ui(UI_SELECT);
            if (i < 8) { g.ChoosePaint(PAINTS[i].hex); Vector3 hsv = ColorToHSV(Hex(PAINTS[i].hex)); g.customH = hsv.x; g.customS = hsv.y; g.customV = hsv.z; }
            else g.ChoosePaint(ToHex(ColorFromHSV(g.customH, g.customS, g.customV)));
        }
    }
    yy += ((8 / perRow) + 1) * (sw + sg) + px(14);
    // a custom colour: hue, saturation, brightness
    Label("Custom colour", x, yy, SAND_C); yy += px(24);
    const char* labels[3] = {"Hue", "Saturation", "Brightness"};
    float* vals[3] = {&g.customH, &g.customS, &g.customV};
    for (int k = 0; k < 3; k++) {
        const float bw = std::min(rightW, px(380)), bh = px(8);
        const Rectangle r{x, yy + px(20), bw, bh};
        Label(labels[k], x, yy);
        float v = k == 0 ? *vals[0] / 360 : *vals[k];
        const bool changed = g.ui.Slider(ID_HUE + k, {r.x - px(8), r.y - px(10), r.width + px(16), r.height + px(20)}, v, k == 0 ? 1.0f / 72 : 0.05f);
        for (int s = 0; s < 24; s++) {
            const float u0 = s / 24.0f, u1 = (s + 1) / 24.0f;
            auto col = [&](float u) {
                return k == 0 ? ColorFromHSV(u * 360, 0.8f, 1) : k == 1 ? ColorFromHSV(g.customH, u, g.customV) : ColorFromHSV(g.customH, g.customS, u);
            };
            draw::FillRectGradH(r.x + r.width * u0, r.y, r.width * (u1 - u0) + 0.5f, bh, col(u0), col(u1));
        }
        const bool hot = g.ui.Hot(ID_HUE + k);
        const float kx = r.x + r.width * v;
        draw::FillCircle(kx, r.y + bh / 2, px(hot ? 10 : 8), CREAM);
        draw::FillCircle(kx, r.y + bh / 2, px(hot ? 6 : 5), k == 0 ? ColorFromHSV(g.customH, 0.8f, 1) : ColorFromHSV(g.customH, g.customS, g.customV));
        if (changed) {
            if (k == 0) g.customH = v * 360; else *vals[k] = v;
            g.ChoosePaint(ToHex(ColorFromHSV(g.customH, g.customS, g.customV)));
        }
        yy += px(20) + bh + px(22);
    }
}

static void ControlsView(Game& g, Rectangle area) {
    bool back;
    float y = SubHead(g, area.x, area.y, "Controls", back);
    if (back) return;
    const float gap = px(30), w = (area.width - gap) / 2;
    auto panel = [&](float x, const char* tag, const std::vector<std::pair<std::string, std::string>>& keys) {
        float keyW = 0;
        for (auto& k : keys) keyW = std::max(keyW, TW(W700, k.first, 14, 0.04f));
        const float h = px(26) + px(22) + keys.size() * px(32) + px(12);
        Glass({x, y, w, h}, px(20));
        float yy = y + px(26);
        Label(tag, x + px(28), yy, SAND_C); yy += px(28);
        for (auto& k : keys) {
            Tx(W700, k.first, x + px(28), yy, 14, CREAM, 0.04f);
            Tx(W500, k.second, x + px(28) + keyW + px(22), yy, 15, SOFT);
            yy += px(32);
        }
    };
    panel(area.x, "Keyboard", {{"↑ / W", "Gas"}, {"↓ / S", "Brake and reverse"}, {"Left Right / A D", "Steer"}, {"Space", "Handbrake to kick the rear out"},
                               {"R", "Put the car back on the road"}, {"Esc / P", "Pause"}, {"M", "Mute or unmute"}, {"− / +", "Volume"}, {"F11", "Fullscreen"}});
    panel(area.x + w + gap, "Controller", {{"Left stick", "Steer"}, {"R2 / L2", "Gas and brake, analog like pedals"}, {"X / R1", "Handbrake"}, {"Y", "Reset car"}, {"Start", "Pause"},
                                           {"D-pad", "Move through menus"}, {"B", "Back"}});
}

void Game::DrawMenus() {
    G = this; ::S = S;
    MenuBackground(*this);
    const float padX = std::max(px(24), W * 0.055f), padT = px(34), padB = px(20);
    const bool sub = view != V_MAIN;
    Logo(*this, padX, padT, sub);
    MenuStats(*this, W - padX, padT);
    const float vw = W / S / 100;
    const float logoH = sub ? px(Clamp(2.2f * vw, 22, 32)) + px(16) : px(Clamp(5.6f * vw, 38, 76)) * 2.04f + px(34);
    const float footH = px(24) + px(11) * 1.4f + px(8);
    const float top = padT + logoH + H * (sub ? 0.025f : 0.02f);
    const Rectangle area{padX, top, W - padX * 2, H - padB - footH - H * 0.025f - top};
    switch (view) {
        case V_MAIN: MainView(*this, area); break;
        case V_STAGES: StagesView(*this, area); break;
        case V_GARAGE: GarageView(*this, area); break;
        default: ControlsView(*this, area); break;
    }
    // key hints and credits
    float x = padX, y = H - padB - footH;
    auto hint = [&](const std::string& t) { Tx(W700, t, x, y + px(6), 11, Hex(0xfbf4ea, 0.6f), 0.2f); x += TW(W700, t, 11, 0.2f) + px(22); };
    if (ui.padNav) {
        Kbd("A", x, y, true, Hex(0x8fbf7a)); hint("SELECT");
        if (sub) { Kbd("B", x, y, true, Hex(0xe9806e)); hint("BACK"); }
    } else {
        Kbd("↑↓", x, y); hint("NAVIGATE");
        Kbd("ENTER", x, y); hint("SELECT");
        if (sub) { Kbd("ESC", x, y); hint("BACK"); }
    }
    const std::string credit = "Music: \"Liquid Flame\" by Of Far Different Nature (CC0). Engine sound: Supra dyno recording by editboy23 on Freesound (CC0).";
    Tx(W500, credit, std::max(x, W - padX - TW(W500, credit, 11)), H - padB - px(11) * 1.3f, 11, Hex(0xfbf4ea, 0.4f));
}

// ---------- HUD ----------
void Game::DrawHud(double speed) {
    G = this; ::S = S;
    const float top = px(18), side = px(22);
    // score
    std::string sub1, sub2;
    bool warn = false;
    if (mode == MODE_TRACK && run) {
        sub1 = "Target " + FmtNum(T->pass);
        const double left = T->limit - run->t;
        sub2 = "Lap " + std::to_string(std::min(run->lap + 1, run->def->laps)) + " / " + std::to_string(run->def->laps) + "   " + FmtT(left);
        warn = left < 10;
    } else sub1 = "Best drift " + FmtNum(best);
    const std::string sc = FmtNum(score);
    const float cw = std::max({px(170), TW(W500, sc, 36) + px(40), TW(W700, Upper(sub1), 11, .2f) + px(40), TW(W700, Upper(sub2), 11, .2f) + px(40)});
    const float ch = px(14) + px(11) * 1.3f + px(36) * 1.1f + px(4) + px(11) * 1.5f + (sub2.empty() ? 0 : px(11) * 1.5f) + px(12);
    Glass({side, top, cw, ch}, px(18), 0.8f);
    float x = side + px(20), y = top + px(14);
    Label("Score", x, y); y += px(11) * 1.3f;
    Tx(W500, sc, x, y, 36, CREAM); y += px(36) * 1.1f + px(4);
    Tx(W700, Upper(sub1), x, y, 11, SOFT, 0.2f); y += px(11) * 1.5f;
    if (!sub2.empty()) Tx(W700, Upper(sub2), x, y, 11, warn ? BAD : SOFT, 0.2f);
    // minimap
    const Rectangle mr{side, top + ch + px(12), px(150), px(150)};
    Glass(mr, px(18), 0.8f);
    DrawMini(mr);

    // speed, revs and gear
    const std::string spd = std::to_string((int)JsRound(speed * 0.32));
    const float sw = std::max(px(190), TW(W500, spd, 44) + TW(W700, "KM/H", 11, .28f) + px(52));
    const float sh = px(14) + px(44) * 1.05f + px(12) + px(26) + px(12);
    const float sx = W - side - sw, sy = top;
    Glass({sx, sy, sw, sh}, px(18), 0.8f);
    const float right = W - side - px(20);
    const float uw = TW(W700, "KM/H", 11, .28f);
    Label("km/h", right - uw, sy + px(14) + px(44) * 0.72f);
    Tx(W500, spd, right - uw - px(10) - TW(W500, spd, 44), sy + px(14), 44, CREAM);
    const float gy = sy + px(14) + px(44) * 1.05f + px(12);
    const float gr = px(13);   // gear: a small ring with the number
    draw::StrokeCircle(right - gr, gy + gr, gr, px(1.5f), SUN);
    const std::string gear = std::to_string(audio.Gear() + 1);
    TxCenter(W700, gear, right - gr, gy + gr - px(13) * 0.6f, 13, SUN);
    {   // revs: a thin line that fills towards the redline
        const float lx = sx + px(20), lw = right - gr * 2 - px(14) - lx, ly = gy + gr;
        const float k = std::min(1.0f, audio.Rpm() / Audio::LIMIT);
        draw::Line(lx, ly, lx + lw, ly, px(3), LINE, draw::ROUND);
        draw::Line(lx, ly, lx + lw * k, ly, px(3), k > 0.86f ? BAD : CREAM, draw::ROUND);
    }
    // pause and reset (mouse)
    float bx = W - side;
    auto hudBtn = [&](const std::string& label, bool pauseIcon) {
        const float w = TW(W700, label, 11, .2f) + px(30) + (pauseIcon ? px(18) : 0), h = px(32);
        bx -= w;
        const Rectangle r{bx, sy + sh + px(10), w, h};
        const bool hover = CheckCollisionPointRec(GetMousePosition(), r);
        Pill(r, hover ? GLASS_HI : Alpha(GLASS, 0.8f), hover ? Hex(0xfbf4ea, 0.7f) : Hex(0xfbf4ea, 0.25f));
        float tx = r.x + px(15);
        if (pauseIcon) {
            draw::FillRoundRect(tx, r.y + h / 2 - px(5), px(3), px(10), px(1), CREAM);
            draw::FillRoundRect(tx + px(6), r.y + h / 2 - px(5), px(3), px(10), px(1), CREAM);
            tx += px(18);
        }
        Tx(W700, label, tx, r.y + h / 2 - px(11) * 0.6f, 11, CREAM, 0.2f);
        bx -= px(8);
        return ui.Clicked(r);
    };
    const bool driving = state == ST_RACE || state == ST_FREE || state == ST_COUNT;
    if (driving) {
        if (hudBtn("PAUSE", true)) Pause();
        if (hudBtn("RESET", false) && (state == ST_RACE || state == ST_FREE)) { Wreck(); ResetCar(); }
    }

    if (showDebug) {   // F3: handling numbers (car.h)
        const double fx = std::cos(car.a), fy = std::sin(car.a), u = car.vx * fx + car.vy * fy, v = -car.vx * fy + car.vy * fx;
        char b[256];
        std::snprintf(b, sizeof b, "speed %4.0f  slide angle %5.1f  rotation %5.2f  steer %+.2f  handbrake %.2f",
                      std::hypot(u, v), std::atan2(v, std::fabs(u) + 1e-6) * 57.3, car.w, car.steer, HandbrakeIn());
        const float yy = H - px(40);
        draw::FillRoundRect(side - px(8), yy - px(4), TW(W500, b, 13) + px(16), px(22), px(8), Alpha(GLASS, 1.4f));
        Tx(W500, b, side, yy, 13, CREAM);
    }

    // the drift combo
    static float comboA = 0;
    comboA += ((chain.active ? 1.0f : 0.0f) - comboA) * std::min(1.0f, GetFrameTime() / 0.08f);
    if (comboA > 0.01f) {
        const std::string pts = FmtNum(JsRound(chain.pts)), mult = "x" + std::to_string(chain.mult);
        const float cy = H * 0.2f, pw = TW(W500, pts, 50);
        TextGlow(W500, pts, W / 2 - pw / 2, cy, px(50), Alpha(Rgba(244, 181, 106, .45f), comboA), px(14));
        TxShadow(W500, pts, W / 2 - pw / 2, cy, 50, Alpha(SUN, comboA));
        const float mw = TW(W700, mult, 13, .12f) + px(26), my = cy + px(50) * 1.05f + px(6);
        Pill({W / 2 - mw / 2, my, mw, px(24)}, Alpha(GLASS, comboA), Hex(0xfbf4ea, 0.5f * comboA));
        TxCenter(W700, mult, W / 2, my + px(12) - px(13) * 0.6f, 13, Alpha(CREAM, comboA), 0.12f);
    }
    // messages
    if (toast.alpha > 0.01f) {
        const float tw = TW(W500, toast.text, 28);
        TextGlow(W500, toast.text, W / 2 - tw / 2, H * 0.33f, px(28), Alpha(toast.color, toast.alpha * 0.4f), px(10));
        TxShadow(W500, toast.text, W / 2 - tw / 2, H * 0.33f, 28, Alpha(toast.color, toast.alpha));
    }
    if (zmsg.alpha > 0.01f) {
        const std::string t = Upper(zmsg.text);
        const float tw = TW(W700, t, 13, .2f), bw = tw + px(40), bh = px(32);
        Pill({W / 2 - bw / 2, H * 0.41f, bw, bh}, Alpha(GLASS, 1.4f * zmsg.alpha));
        TxCenter(W700, t, W / 2, H * 0.41f + bh / 2 - px(13) * 0.6f, 13, Alpha(zmsg.color, zmsg.alpha), 0.2f);
    }
    if (mode == MODE_TRACK && run && run->wrongT > 0.7 && state == ST_RACE) {
        const float tw = TW(W700, "WRONG WAY", 18, .3f), bw = tw + px(64), bh = px(48);
        Pill({W / 2 - bw / 2, H / 2 - bh / 2, bw, bh}, Alpha(BAD, 0.88f));
        TxCenter(W700, "WRONG WAY", W / 2, H / 2 - px(18) * 0.6f, 18, CREAM, 0.3f);
    }
    // countdown and stage intro
    if (bannerOn) {
        const float bw = std::min(W * 0.9f, px(540));
        const auto lines = WrapText(W500, bannerD, px(15), bw - px(48));
        const float nH = bannerN.empty() ? 0 : px(112);
        const float dH = lines.size() * px(15) * 1.55f + px(28);
        const float total = nH + px(10) + px(14) * 1.4f + px(16) + dH;
        float yy = H * 0.3f - total / 2;
        if (!bannerN.empty()) {
            TextGlow(W500, bannerN, W / 2 - TW(W500, bannerN, 112) / 2, yy, px(112), Rgba(244, 181, 106, .5f), px(22));
            TxShadow(W500, bannerN, W / 2 - TW(W500, bannerN, 112) / 2, yy, 112, CREAM);
        }
        yy += nH + px(10);
        TxShadow(W700, Upper(bannerT), W / 2 - TW(W700, Upper(bannerT), 14, 0.3f) / 2, yy, 14, CREAM, 0.3f);
        yy += px(14) * 1.4f + px(16);
        float lw = 0;
        for (auto& l : lines) lw = std::max(lw, TW(W500, l, 15));
        const Rectangle d{W / 2 - (lw + px(48)) / 2, yy, lw + px(48), dH};
        draw::FillRoundRect(d.x, d.y, d.width, d.height, px(16), Alpha(GLASS, 1.5f));
        float ly = d.y + px(14);
        for (auto& l : lines) { TxCenter(W500, l, W / 2, ly, 15, SOFT); ly += px(15) * 1.55f; }
    }
}

// ---------- overlays: pause and results ----------
static Rectangle Card(Game& g, float maxW, float h, const char* pre, const std::string& title, Color titleCol, float titleFs) {
    draw::FillRect(0, 0, g.W, g.H, Rgba(24, 16, 26, .5f));
    const float w = std::min(maxW, g.W - px(40));
    const Rectangle r{g.W / 2 - w / 2, g.H / 2 - h / 2, w, h};
    draw::FillRoundRect(r.x, r.y, r.width, r.height, px(24), Rgba(34, 24, 34, .78f));
    draw::StrokeRoundRect(r.x, r.y, r.width, r.height, px(24), px(1), LINE);
    Label(pre, r.x + px(28), r.y + px(28), SAND_C);
    Tx(W500, title, r.x + px(28), r.y + px(28) + px(20), titleFs, titleCol, 0.02f);
    return r;
}

void Game::DrawOverlays() {
    G = this; ::S = S;
    if (state == ST_PAUSE) {
        const float pad = px(28), h = pad + px(20) + px(30) * 1.2f + px(18) + px(50) + px(18) + px(46) * 3 + px(10) * 2 + px(28);
        const Rectangle r = Card(*this, px(380), h, "Sideways City", "Paused", CREAM, 30);
        float y = r.y + pad + px(20) + px(30) * 1.2f + px(18);
        const float x = r.x + pad, w = r.width - pad * 2, u = px(46);
        if (Button(ID_RESUME, {x, y, w, px(50)}, "Resume", PRIMARY, 14, UI_SELECT, true)) { Resume(); return; }
        y += px(50) + px(18);
        auto saveVol = [&]() { save.SetInt("vol", audio.Volume()); save.SetBool("muted", audio.Muted()); Save(); };
        if (Button(ID_VOLDN, {x, y, u, px(46)}, "−", GHOST, 18, UI_SELECT)) { audio.SetVolume(audio.Volume() - 1); saveVol(); }
        if (Button(ID_VOLUP, {x + w - u, y, u, px(46)}, "+", GHOST, 18, UI_SELECT)) { audio.SetVolume(audio.Volume() + 1); saveVol(); }
        {   // volume: a label and ten soft steps
            const float vx = x + u + px(10), vw = w - 2 * u - px(20);
            const std::string L = "VOLUME " + std::to_string(audio.Volume());
            TxCenter(W700, L, vx + vw / 2, y + px(6), 11, SOFT, 0.28f);
            const float step = px(12), mw = 10 * step - px(4), mx = vx + vw / 2 - mw / 2;
            for (int k = 0; k < 10; k++) draw::FillRoundRect(mx + k * step, y + px(26), px(8), px(10), px(3), k < audio.Volume() ? CREAM : Hex(0xfbf4ea, 0.18f));
        }
        y += px(46) + px(10);
        if (Button(ID_MUTE, {x, y, w, px(46)}, audio.Muted() ? "Sound off" : "Sound on", GHOST, 12, UI_SELECT, false, audio.Muted() ? BAD : BLANK)) { audio.SetMuted(!audio.Muted()); saveVol(); }
        y += px(46) + px(10);
        const float half = (w - px(10)) / 2;
        if (Button(ID_RESET, {x, y, half, px(46)}, "Reset car", GHOST, 12, UI_SELECT)) {
            Resume();
            if (state == ST_RACE || state == ST_FREE) { Wreck(); ResetCar(); }
            return;
        }
        if (Button(ID_QUITMENU, {x + half + px(10), y, half, px(46)}, "Quit to menu", GHOST, 12, UI_SELECT)) { ShowMenu(); return; }
        return;
    }
    if (state == ST_DONE && resultOn) {
        const float pad = px(28), w = std::min(px(440), W - px(40)) - pad * 2;
        const float starsH = px(38) + px(22);
        const float h = pad + px(20) + px(32) * 1.2f + px(14) + starsH + ParagraphH(rScore, w, 15, 1.55f) + px(8) + ParagraphH(rBest, w, 15, 1.55f) + px(26) + px(46) + pad;
        const Rectangle r = Card(*this, px(440), h, "Results", rTitle, rGood ? SUN : BAD, 32);
        float y = r.y + pad + px(20) + px(32) * 1.2f + px(14);
        const float x = r.x + pad;
        Stars(x, y, px(38), rStars, 3, px(12), STAR);
        y += starsH;
        y = Paragraph(rScore, x, y, w, 15, 1.55f, SOFT) + px(8);
        y = Paragraph(rBest, x, y, w, 15, 1.55f, SOFT) + px(26);
        const int n = rNext ? 3 : 2;
        const float bw = (w - px(10) * (n - 1)) / n;
        float bx = x;
        const int stage = run ? run->i : 0;
        if (rNext) {
            if (Button(ID_NEXT, {bx, y, bw, px(46)}, "Next stage", PRIMARY, 12, UI_START, true)) { StartStage(stage + 1); return; }
            bx += bw + px(10);
        }
        if (Button(ID_RETRY, {bx, y, bw, px(46)}, "Retry", GHOST, 12, UI_START, !rNext)) {
            StartStage(stage);
            return;
        }
        bx += bw + px(10);
        if (Button(ID_MENU, {bx, y, bw, px(46)}, "Menu", GHOST, 12, UI_SELECT)) { ShowMenu(V_STAGES); return; }
        return;
    }
}
