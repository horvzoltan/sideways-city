#include "ui.h"
#include "game.h"
#include <cmath>

void Ui::SetPadNav(int dir, bool a, bool menuActive) {
    padA = a;
    if (!menuActive) { padDir = 0; return; }
    if (dir || a) padNav = true;
    if (dir != padDir) { padDir = dir; padT = 0.35f; if (dir) navDir = dir; }
    else if (dir && padT <= 0) { padT = 0.12f; navDir = dir; }
}

void Ui::BeginFrame(Game* g, float dt) {
    game = g;
    items.clear();
    navDir = 0;
    padT -= dt;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) pressPos = GetMousePosition();
    Vector2 md = GetMouseDelta();
    mouseMoved = md.x != 0 || md.y != 0;
    if (mouseMoved || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) padNav = false;
    const bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    activate = (!alt && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) || IsKeyPressed(KEY_SPACE);
    auto key = [](int k) { return IsKeyPressed(k) || IsKeyPressedRepeat(k); };
    if (key(KEY_UP)) navDir = 1;
    else if (key(KEY_DOWN)) navDir = 2;
    else if (key(KEY_LEFT)) navDir = 3;
    else if (key(KEY_RIGHT)) navDir = 4;
}

void Ui::Move(int id) {
    if (id != focus) { focus = id; if (game) game->audio.Ui(UI_MOVE); }
}

bool Ui::Item(int id, Rectangle r, bool enabled, bool isDefault) {
    items.push_back({id, r, enabled, false, isDefault});
    const Vector2 m = GetMousePosition();
    const bool over = CheckCollisionPointRec(m, r);
    if (enabled && over && mouseMoved) Move(id);
    if (over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) pressId = id;
    bool clicked = enabled && over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && pressId == id;
    if (clicked) focus = id;
    if (!clicked && enabled && focus == id && (activate || padA)) { activate = padA = false; return true; }
    return clicked;
}

bool Ui::Clicked(Rectangle r) const {
    return IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), r) && CheckCollisionPointRec(pressPos, r);
}

bool Ui::Slider(int id, Rectangle r, float& v, float step) {
    items.push_back({id, r, true, true, false});
    const Vector2 m = GetMousePosition();
    const bool over = CheckCollisionPointRec(m, r);
    if (over && mouseMoved) Move(id);
    if (over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { pressId = id; focus = id; }
    float old = v;
    if (pressId == id && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) v = std::fmin(1.0f, std::fmax(0.0f, (m.x - r.x) / r.width));
    if (focus == id && (navDir == 3 || navDir == 4)) { v = std::fmin(1.0f, std::fmax(0.0f, v + (navDir == 4 ? step : -step))); navDir = 0; }
    return v != old;
}

void Ui::EndFrame() {
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) pressId = -1;
    if (items.empty()) return;
    const It* cur = nullptr;
    for (const It& it : items) if (it.id == focus) cur = &it;
    if (!cur || !cur->enabled) {   // nothing focused yet: the screen's default item, quietly
        const It* pick = nullptr;
        for (const It& it : items) if (it.isDefault && it.enabled) { pick = &it; break; }
        if (!pick) for (const It& it : items) if (it.enabled) { pick = &it; break; }
        if (pick) focus = pick->id;
        return;
    }
    if (!navDir) return;
    const float dx = navDir == 3 ? -1.0f : navDir == 4 ? 1.0f : 0.0f, dy = navDir == 1 ? -1.0f : navDir == 2 ? 1.0f : 0.0f;
    const Rectangle r = cur->r;
    const float cx = r.x + r.width / 2, cy = r.y + r.height / 2;
    const It* best = nullptr;
    float bestScore = 1e9f;
    for (const It& it : items) {
        if (it.id == cur->id || !it.enabled) continue;
        const Rectangle q = it.r;
        float ex = q.x + q.width / 2 - cx, ey = q.y + q.height / 2 - cy, along = ex * dx + ey * dy, perp = std::fabs(ex * dy - ey * dx);
        if (along <= 4) continue;
        if (dx != 0 ? (q.y + q.height <= r.y || q.y >= r.y + r.height) : perp > along * 1.5f) continue;   // left/right stay in the row
        float score = along + perp * 2.5f;
        if (score < bestScore) { bestScore = score; best = &it; }
    }
    if (best) Move(best->id);
}
