# Sideways City (native)

The same game as the web version, rewritten in C++17 on raylib 5.5, the engine and build setup used by RECOIL. It has everything the web version has:
- free drive around Miami Beach
- the twelve drift stages, with zones, clipping points, stars and ghosts
- "Survive the night" in the endless city
- the garage
- keyboard and controller support
- the menus in the same style

The Miami map, the tracks and the endless city are ported one to one and come out identical to the web version (`sideways_tests` checks this).

## Build

raylib 5.5 is downloaded automatically on the first configure, or put a copy in `third_party/raylib` for offline builds.

**Linux**

    sudo apt install cmake g++ git libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libasound2-dev
    ./build_linux.sh

**Windows (Visual Studio 2022 with "Desktop development with C++")**

    build_windows.bat

**Windows exe from Linux (MinGW)**

    cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake && cmake --build build-win

Every build puts its output in `build*/bin/`:
- `sideways`: the game
- `sideways_launcher`: resolution, fullscreen, vsync, FPS limit and anti-aliasing, then PLAY
- `sideways_tests`: headless checks against numbers taken from the JavaScript version
- `assets/`: the engine recording, the menu music and the fonts, copied from the repository's top-level `assets/` folder, which the web version also uses

## Files next to the game

- `sideways.cfg`: display settings, written by the launcher. Any setting can be overridden on the command line, e.g. `./sideways +fullscreen 1 +width 1920 +height 1080`.
- `save.cfg`: best drift, stage progress, survival records, paint and volume (the web version keeps these in localStorage).
- `ghosts/`: the best run on each stage, replayed as a ghost.

## Controls

The same as the web version, plus F11 (or Alt+Enter) for fullscreen.

| Key | Action |
|---|---|
| ↑ / W | gas |
| ↓ / S | brake and reverse |
| ← → / A D | steer |
| Space | handbrake |
| R | reset the car |
| Esc / P | pause |
| M, − / + | mute, volume |

On a controller, the left stick or D-pad steers and RT/LT are analog gas and brake. A or RB is the handbrake, Y resets the car and Start pauses. In menus, A selects and B goes back.

## How it maps to the web version

    src/game/tracks.*    js/tracks.js: stages and track geometry
    src/game/city.*      js/city.js: the endless city
    src/game/miami.*     the Miami map from js/game.js (same seeded generator, same layout)
    src/game/game.*      car physics, drift scoring, stages, ghosts, input, saving
    src/game/survival.*  "Survive the night"
    src/game/render.*    drawing the world, the car, the minimaps and the menu previews
    src/game/menus.*     menus, HUD and overlays (the HTML and CSS of the web version)
    src/game/ui.*        menu focus: mouse, arrow keys and controller
    src/game/draw.*      the Canvas2D drawing, on raylib's rlgl
    src/game/audio.*     the Web Audio sound, as a mixer on a raylib audio stream
    src/launcher/        launcher window + process start (from RECOIL)
    src/common/config.*  key = value files (from RECOIL)

Tuning lives in the same places, under the same names: `Update()` in game.cpp (grip, acceleration, top speed, steering), `ETYPES` and `UpDesc()` in survival.cpp, `THEMES` and `PAINTS` in game.cpp.

**Sound.** The engine is the same granular playback of the Supra dyno recording: short overlapping slices taken from the moment the engine was at the wanted pitch. The web version's Web Audio nodes are rebuilt on the audio thread:
- oscillators with exponential ramps
- noise through biquad filters
- a compressor

The menu music streams from the MP3.

## Developer switches

- `+debug_start stage:N`, `survive`, `free`, `garage`, `stages` or `controls` jumps to that screen.
- `+debug_gas`, `+debug_steer` and `+debug_hand` hold those inputs.
- `+debug_autoshot N` saves `autoshot.png` after N frames and quits.
