# Sideways City

A top-down drifting game in the style of the late-90s overhead city games. Plain HTML, CSS and JavaScript: no frameworks, no build step.

## Running it

The game loads its engine sound from `assets/`, so open it through a local web server rather than double-clicking `index.html` (browsers block file loading from `file://`). Any of these works from this folder:

- **VS Code:** install the "Live Server" extension, right-click `index.html`, choose "Open with Live Server".
- **Node:** `npx serve .`
- **Python:** `python -m http.server 8000`, then open http://localhost:8000

`sideways-city-single-file.html` is the all-in-one version with everything embedded. It works by double-clicking, but it's harder to edit.

## Desktop app (Electron)

```
npm install
npm start           # run the desktop version
npm run dist        # build a Linux AppImage into dist/ (unpacked build in dist/linux-unpacked)
npm run dist:win    # build a Windows installer
```

F11 or Alt+Enter toggles fullscreen. The desktop build serves the game from an `app://` origin (see `desktop/main.js`), so the engine sound and saved progress work the same as on the web. If you run these commands from VS Code's terminal, prefix them with `env -u ELECTRON_RUN_AS_NODE`, because VS Code sets that variable and it makes Electron behave like plain Node.

AppImages need FUSE 2 (`libfuse2`). Without it, run the unpacked build in `dist/linux-unpacked/` directly.

## Project layout

```
index.html                  page structure: HUD, menus, touch controls
css/style.css               all styling
js/tracks.js                the six stage layouts and track geometry
js/game.js                  everything else (physics, drawing, sound, scoring, menus)
assets/supra-engine.mp3     engine recording used for the car sound
assets/fonts/               Chakra Petch, bundled so the game works offline
desktop/                    Electron main process and preload
build/                      app icon (icon.svg is the source)
```

## Where to change things

In `js/tracks.js`:
- `STAGES` holds each stage: `pts` are the track's control points (the road is a smooth curve through them, driven in that order), `w` is road width, `r` is gravel run-off width, `laps`, `v` sets the time limit (higher = less time), and `theme`.
- Drift zones and clipping points are found automatically from the corners in `buildTrack()`.

In `js/game.js`:
- **Handling:** the `update()` function. Grip values (`7.5` normal, `3.0` under power, `1.0` handbrake), acceleration (`540`), top speed (`640`) and steering rate (`2.8`).
- **Scoring:** `trackTick()` and `judgeZone()` for zones and clipping points; target scores and time limits in `loadStage()`.
- **Themes:** `THEMES` for colors and scenery per stage.
- **Paint colors:** `PAINTS`.
- **Engine sound:** the granular playback section (`ENG` tables map rpm to positions in the recording).

Progress, best scores, paint choice and volume are saved in the browser's localStorage.

## Credits

Engine sound: "Import car revs on Chassis Dyno with Turbo" by editboy23 on Freesound (https://freesound.org/people/editboy23/sounds/496171/), licensed CC0. The file here is the original 48 kHz WAV converted to 320 kbps MP3 (`ffmpeg -i original.wav -codec:a libmp3lame -b:a 320k assets/supra-engine.mp3`). The engine sound jumps to fixed positions in the recording, so any replacement must keep the exact same timing.

Font: Chakra Petch (SIL Open Font License, see `assets/fonts/OFL.txt`).
