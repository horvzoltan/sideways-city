# Sideways City

A top-down drifting game in C++17 on raylib 5.5, with the same engine and build setup as RECOIL. It started as a port of an earlier browser version, which has since been removed.

It has:
- eight compact drift stages, with zones, clipping points, stars and ghosts
- free drive in an endless desert, with a day and night cycle and passing sandstorms
- a garage for the paint
- keyboard and controller support

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
- `sideways_tests`: headless checks of the stages (every file loads, every track has drift zones and no shortcuts), the open desert and the car handling
- `assets/`: the stages, the engine recording, the menu music, the fonts and the icon, copied from the repository's top-level `assets/` folder

## Files next to the game

- `sideways.cfg`: display settings, written by the launcher. Any setting can be overridden on the command line, e.g. `./sideways +fullscreen 1 +width 1920 +height 1080`.
- `save.cfg`: best drift, stage progress (saved by stage name), paint and volume, and the view (`view = top` or `iso`; `+view iso` on the command line tries it without saving).
- `ghosts/`: the best run on each stage, replayed as a ghost. A ghost stores a fingerprint of its stage file, so editing a stage retires its old ghost.

## Stages

Each stage is a text file in `assets/stages/`, loaded in file-name order (which is also the unlock order). The comment at the top of every file lists the settings; the track itself is a list of `pt x y` control points that the road curves through. Drift zones, clipping points, the start line, the target score and the time limit are all worked out from the track. After editing a stage, run `sideways_tests`: it checks that every track has drift zones, drivable corners and no shortcuts (parts of the lap that pass close enough to cut across).

## Controls

F11 (or Alt+Enter) toggles fullscreen.

| Key | Action |
|---|---|
| ↑ / W | gas |
| ↓ / S | brake and reverse |
| ← → / A D | steer |
| Space | handbrake |
| R | reset the car |
| V | switch between the top-down and the isometric view |
| Esc / P | pause |
| M, − / + | mute, volume |

On a controller (PlayStation / Xbox names):
- the left stick or D-pad steers
- R2 / RT is the gas and L2 / LT the brake and reverse, both analog like pedals
- X / A or R1 / RB is the handbrake
- Y resets the car and Start pauses
- in menus, X / A selects and B / circle goes back

## How it is built

    src/game/tracks.*    stage files and track geometry
    src/game/car.*       the car handling (the arcade drift model)
    src/game/terrain.*   the desert: dune heightfield and scenery placement around a stage
    src/game/desert.*    free roam: the endless desert, generated in chunks (lakes, salt, rock plates, ruins...)
    src/game/desert_scene.*  how the desert looks: dunes, road, scenery, light, weather, and free roam's day
    src/game/game.*      drift scoring, stages, ghosts, input, saving
    src/game/render.*    drawing the world, the car, the minimaps and the menu previews
    src/game/menus.*     menus, HUD and overlays
    src/game/ui.*        menu focus: mouse, arrow keys and controller
    src/game/draw.*      2D vector drawing on raylib's rlgl
    src/game/audio.*     the sound mixer on a raylib audio stream
    src/launcher/        launcher window + process start (from RECOIL)
    src/common/config.*  key = value files (from RECOIL)

Tuning:
- **Handling:** the `Handling` struct in car.h (grip, how much throttle and the handbrake loosen the rear, steering, top speed). Press F3 in game for live speed, slide angle and inputs.
- **Stages:** the files in `assets/stages/`.
- **Stage looks:** `PALS` in desert_scene.cpp: sand, road and shadow colours, sun direction and colour grade for each time of day (dawn, morning, noon, afternoon, sunset, storm, night, predawn), picked by the stage file's `sky`.
- **Free roam:** `Generate()` in desert.cpp (how often chunks get a lake, salt flat, ruins...), the day length and storms in `DrawFree()` in desert_scene.cpp.
- **Desert scenery:** `MIXES` in terrain.cpp: how many rocks, hoodoos, ruins, lanterns, grass tufts, flags and balloons each time of day gets.
- **Paint colours:** `PAINTS` in game.cpp.
- **Engine sound and gear changes:** `UpdateEngine()` in audio.cpp.

**Sound.** The engine is granular playback of a real Supra dyno recording: short overlapping slices taken from the moment the engine was at the wanted pitch. Each slice is lined up with the one already playing, so they add up in phase, and every slice follows the current rpm while it plays. The other sounds are synthesized:
- oscillators
- noise through filters
- a compressor

The menu music streams from the MP3.

## Developer switches

- `+debug_start stage:N`, `free`, `garage`, `stages` or `controls` jumps to that screen.
- `+debug_gas`, `+debug_steer` and `+debug_hand` hold those inputs.
- `+debug_autoshot N` saves `autoshot.png` after N frames and quits.
