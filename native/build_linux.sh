#!/bin/sh
# Builds Sideways City on Linux. Needs: cmake, g++, git and the X11/GL/ALSA dev packages, e.g.
#   sudo apt install cmake g++ git libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libasound2-dev
set -e
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
echo "Done. Run ./build/bin/sideways_launcher"
