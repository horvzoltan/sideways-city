// ui.h - immediate-mode menu focus. Each frame the screens register their items with a
// rectangle; the mouse, the arrow keys and a controller move the focus between them, using
// a direction score that prefers items straight ahead.
#pragma once
#include "raylib.h"
#include <vector>

class Game;

class Ui {
public:
    void BeginFrame(Game* g, float dt);
    void EndFrame();
    void ResetFocus() { focus = -1; }          // the screen's default item takes the focus
    void SetPadNav(int dir, bool a, bool menuActive);   // dir: 0 none, 1 up, 2 down, 3 left, 4 right
    // A focusable item. Returns true when it is activated (click, Enter, Space or controller A).
    bool Item(int id, Rectangle r, bool enabled = true, bool isDefault = false);
    // A value from 0 to 1 changed by dragging along r or with left/right while focused.
    bool Slider(int id, Rectangle r, float& v, float step);
    // A mouse-only button (HUD): never takes the keyboard focus, so Space and the arrows keep driving.
    bool Clicked(Rectangle r) const;
    bool Hot(int id) const { return focus == id; }
    int Focus() const { return focus; }
    void SetFocus(int id) { focus = id; }
    bool padNav = false;   // show controller hints

private:
    struct It { int id; Rectangle r; bool enabled, slider, isDefault; };
    std::vector<It> items;
    Game* game = nullptr;
    int focus = -1, pressId = -1;
    Vector2 pressPos{-1, -1};
    int navDir = 0;          // this frame
    bool activate = false, mouseMoved = false;
    int padDir = 0; float padT = 0; bool padA = false;
    void Move(int id);
};
