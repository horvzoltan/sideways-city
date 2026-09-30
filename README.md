# Sideways City

A top-down drifting game, written in C++17 on raylib 5.5. It has:
- eight compact drift stages full of corners, with zones, clipping points and ghosts, from dawn to night
- free drive in an endless desert with a day and night cycle

The code is in `native/`. See `native/README.md` for building, controls and where to change things.

## Quick start (Linux)

    cd native
    ./build_linux.sh
    ./build/bin/sideways_launcher

## Project layout

```
native/                 the game: source, CMake project, launcher and tests
assets/supra-engine.mp3 engine recording used for the car sound
assets/music/           menu music
assets/fonts/           Chakra Petch
assets/icon.png         window icon (icon.svg is the source)
```

## Credits

Engine sound: "Import car revs on Chassis Dyno with Turbo" by editboy23 on Freesound (https://freesound.org/people/editboy23/sounds/496171/), licensed CC0. The file here is the original 48 kHz WAV converted to 320 kbps MP3. The engine sound jumps to fixed positions in the recording, so any replacement must keep the exact same timing.

Menu music: "Liquid Flame" by Of Far Different Nature, released as CC0.

Font: Chakra Petch (SIL Open Font License, see `assets/fonts/OFL.txt`).
