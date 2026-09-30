cmake_minimum_required(VERSION 3.16)
project(sideways_city CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---- raylib 5.5 (window, input, OpenGL, audio) ----
# Uses ./third_party/raylib if present (offline builds), otherwise downloads it.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib/CMakeLists.txt)
  add_subdirectory(third_party/raylib)
else()
  include(FetchContent)
  FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(raylib)
endif()

if(MSVC)
  add_compile_options(/W3 /permissive- /utf-8)
else()
  add_compile_options(-Wall -Wno-missing-field-initializers)
endif()

# World generation and the car handling: no raylib, shared by the game and the tests.
set(WORLD_SRC
  src/game/tracks.cpp
  src/game/desert.cpp
  src/game/world.cpp
  src/game/car.cpp
  src/game/terrain.cpp)

# ---- the game ----
add_executable(sideways
  ${WORLD_SRC}
  src/common/config.cpp
  src/game/draw.cpp
  src/game/desert_scene.cpp
  src/game/audio.cpp
  src/game/game.cpp
  src/game/render.cpp
  src/game/ui.cpp
  src/game/menus.cpp
  src/game_main.cpp)
target_include_directories(sideways PRIVATE src)
target_link_libraries(sideways PRIVATE raylib)

# ---- the launcher ----
add_executable(sideways_launcher
  src/common/config.cpp
  src/launcher/process.cpp
  src/launcher/launcher_main.cpp)
target_link_libraries(sideways_launcher PRIVATE raylib)

if(WIN32)
  # No console windows.
  set_target_properties(sideways sideways_launcher PROPERTIES WIN32_EXECUTABLE ON)
  if(MSVC)
    set_target_properties(sideways sideways_launcher PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
  endif()
  if(MINGW)
    target_link_options(sideways PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(sideways_launcher PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
endif()

# Linux: bundle the C++ runtime so the binaries run on more distros.
if(UNIX AND NOT APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_options(sideways PRIVATE -static-libstdc++ -static-libgcc)
  target_link_options(sideways_launcher PRIVATE -static-libstdc++ -static-libgcc)
endif()

# Put both executables and the assets (the repository's top-level assets/ folder) next to each other.
foreach(t sideways sideways_launcher)
  set_target_properties(${t} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
endforeach()
set(ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../assets)
add_custom_command(TARGET sideways POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:sideways>/assets/fonts $<TARGET_FILE_DIR:sideways>/assets/music
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ASSETS_DIR}/supra-engine.mp3 ${ASSETS_DIR}/icon.png $<TARGET_FILE_DIR:sideways>/assets/
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ASSETS_DIR}/music/menu-liquid-flame.mp3 $<TARGET_FILE_DIR:sideways>/assets/music/
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSETS_DIR}/stages $<TARGET_FILE_DIR:sideways>/assets/stages
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${ASSETS_DIR}/fonts/chakra-petch-latin-500-normal.ttf ${ASSETS_DIR}/fonts/chakra-petch-latin-700-normal.ttf
    ${ASSETS_DIR}/fonts/OFL.txt $<TARGET_FILE_DIR:sideways>/assets/fonts/)

# ---- headless checks: stages, the Miami map and the car handling ----
option(SIDEWAYS_BUILD_TESTS "Build sideways_tests" ON)
if(SIDEWAYS_BUILD_TESTS)
  add_executable(sideways_tests ${WORLD_SRC} tests/world_test.cpp)
  target_include_directories(sideways_tests PRIVATE src)
  target_compile_definitions(sideways_tests PRIVATE STAGES_DIR="${ASSETS_DIR}/stages")
  set_target_properties(sideways_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
  if(MINGW)
    target_link_options(sideways_tests PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
endif()
