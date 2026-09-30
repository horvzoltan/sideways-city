# TODOs: releasing on Steam

Most important first. Start with the first and third items under "Code": they are small, every release needs them, and they unblock Steam Deck testing.

## Code

- [ ] **Save files in the user data folder.** `save.cfg`, `sideways.cfg` and `ghosts/` are written next to the program now. That breaks in read-only install folders, and Steam updates can overwrite them. Use `%APPDATA%\SidewaysCity` on Windows and `~/.local/share/sideways-city` on Linux.
- [ ] **Build and test on Windows.** The MinGW and Visual Studio scripts exist but have not been tried. Most Steam players are on Windows.
- [ ] **Settings inside the game, and drop the launcher.** Steam starts the game directly, and the launcher only works with a mouse. Move resolution, fullscreen, vsync and volume into the pause and main menus.
- [ ] **Steam Deck and controller-only play.** Everything must work without a keyboard, and the text must be readable at 1280×800.
- [ ] **Performance on weaker hardware.** Measure on a laptop or the Deck once the desert look is in; cache ground and track geometry instead of rebuilding it every frame.
- [ ] **Key rebinding.** Players expect it, and it matters for accessibility.

## Steam integration (Steamworks SDK)

- [ ] **Cloud saves.** Turn on Steam's Auto-Cloud for the save folder (needs the user data folder item above). No code needed.
- [ ] **Achievements.** For example: 3 stars on every stage, a clean lap of every zone, a 10,000-point drift.
- [ ] **Leaderboards** for stage scores and lap times.
- [ ] **Steam overlay:** check that it pauses the game. The game already pauses when it loses focus.

## Legal and licences

- [ ] **Credits screen in the game.** The engine sound and music are CC0, the font is SIL OFL (ship `assets/fonts/OFL.txt`) and raylib is zlib.
- [ ] **Check the name "Sideways City"** on Steam and in trademark databases before building the store page around it.
- [ ] **Steamworks paperwork:** $100 Steam Direct fee, tax and bank details, identity verification, and the content survey for age ratings.

## Store and release

- [ ] **Store page:** capsule images in Steam's sizes, 5 or more screenshots, a trailer, a description and tags.
- [ ] **Publish the page as "Coming Soon" early** to collect wishlists. It must be live at least 2 weeks before launch.
- [ ] **Builds:** upload one package for Windows and one for Linux with SteamPipe. Both the store page and the build go through Valve's review.
- [ ] **A demo,** ideally for a Steam Next Fest.

## Content and polish

- [ ] **Onboarding:** a short first stage that explains drift zones and clipping points.
- [ ] **Replay value:** more cars, time trials, daily challenges.
- [x] **Stage definitions in data files** (`assets/stages/`) instead of hard-coded in `tracks.cpp`.
- [x] **Fingerprint for ghosts:** each ghost stores a hash of its stage file, so editing a track retires old ghosts.
