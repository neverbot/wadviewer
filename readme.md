# WAD Viewer

A WAD file viewer built with C++ and OpenGL using the [Okinawa engine](https://github.com/okinawa-dev/okinawa.cpp). This tool allows you to view WAD ([Where's All the Data](https://doomwiki.org/wiki/WAD)) files, commonly used in games like DOOM, displaying their 3D geometry and textures.

<p align="center">
  <img width="500" alt="screenshot" src="/assets/project/screenshot.png">
</p>

## Features

- Load and view WAD file geometry.
- Load a base IWAD + PWAD together so PWAD maps render with the IWAD's textures.
- Interactive 3D camera controls.
- Texture display support.

## Tools used

- [xmake](https://xmake.io/): A cross-platform build utility with a built-in package manager.
- [clang](https://clang.llvm.org/): A compiler for C and C++ languages.

## Building from Source

The project is built with [xmake](https://xmake.io), which also manages
the third-party dependencies through its package manager (xrepo). There
is no separate dependency-installation step.

### Prerequisites

The viewer is built against the [Okinawa engine](https://github.com/okinawa-dev/okinawa.cpp),
which is vendored as a **git submodule** at `./okinawa.cpp` and built from
source. Clone the repository recursively so the engine comes with it:

```bash
git clone --recursive https://github.com/neverbot/wadviewer.git
```

If you already cloned without `--recursive`, fetch the engine with:

```bash
git submodule update --init
```

xmake builds the engine from the submodule automatically; there is no
binary package step. To work on the engine, edit it inside the
`okinawa.cpp/` submodule and rebuild wadviewer — the changes are picked
up directly.

### Build

```bash
cd wadviewer

# Build (debug by default). On the first run xmake downloads and builds
# the dependencies automatically.
xmake

# Release build
xmake f -m release && xmake
```

## Usage

Run through xmake (it runs from the project root, where the `wads/`
folder lives):

```bash
# Load a WAD (the -wad selector is optional; WAD is the only format)
xmake run wadviewer <content_file> [<level_name>]

# With a base IWAD for shared resources (textures/flats/palette)
xmake run wadviewer -iwad <iwad_file> <content_file> [<level_name>]
```

Example:
```bash
xmake run wadviewer wads/doom1.wad E1M1

# A DOOM II PWAD (only maps inside) rendered with doom2.wad's textures:
xmake run wadviewer -iwad wads/doom2.wad wads/mymegawad.wad MAP01
```

### IWAD + PWAD loading

Most third-party WADs are **PWADs**: they contain only the level lumps
(geometry) and reuse the textures, flats and palette from the base game's
**IWAD** (`doom2.wad` / `doom.wad`). Loaded on their own they render
all-white, because the texture names resolve to nothing.

Pass the base IWAD with `-iwad`. Sources are merged in load order — the IWAD
first, then the content WAD on top — following DOOM's **last-wins** rule: a
lump in the later file (the PWAD) overrides the same-named lump in the IWAD,
and anything the PWAD does not provide (textures, flats, palette, even other
maps) falls through to the IWAD. A PWAD level shadows the IWAD's level of the
same name, so `-iwad doom2.wad mymegawad.wad MAP01` shows the PWAD's MAP01 with
doom2's textures.

### Command Line Arguments

- `-wad`: WAD-format selector (the default and only format; optional)
- `-iwad <file>`: Optional base IWAD loaded first for shared resources
  (e.g. `doom2.wad` so a DOOM II PWAD renders with textures instead of white).
- `content_file`: Path to the input file
- `level_name`: Optional. Name of the level to display. If not specified, the first level in the file will be used.
- `--mcp`: Optional. Start the in-engine MCP server (only effective if the
  server was compiled in — see "Compiling the MCP server in or out").
- `--no-input`: Optional. Ignore physical mouse/keyboard so the instance is
  driven only through the MCP. The window stays visible.
- `--verbose`: Optional. Enable verbose debug logging.

### Controls

```
╔══════════════════════════════════════════════════════════════╗
║                        CONTROLS HELP                         ║
╠══════════════════════════════════════════════════════════════╣
║  TEXTURE VIEWER:                                             ║
║    SPACE BAR  - Cycle through textures                       ║
║    T          - Toggle texture viewer (hidden by default)    ║
║                                                              ║
║  DEBUG / INSPECT:                                            ║
║    F          - Toggle debug gizmos (origin axes, camera     ║
║                 boxes) (hidden by default)                   ║
║    R          - Toggle ceiling/floor visibility              ║
║                                                              ║
║  CAMERAS:                                                    ║
║    1          - Overview camera                              ║
║    2          - Player start camera                          ║
║    3          - Origin camera                                ║
║                                                              ║
║  MOVEMENT:                                                   ║
║    W A S D    - Move forward/left/backward/right             ║
║    MOUSE      - Look around                                  ║
║    ESC        - Exit application                             ║
╚══════════════════════════════════════════════════════════════╝
```

## Project Structure

- `src/`: Source code files.
  - `main.cpp`: Main application entry point.
  - `wad.hpp/cpp`: WAD file parsing and handling.
  - `wad-converter.hpp/cpp`: WAD to 3D geometry conversion.
- `wads/`: Example WAD files.

## Agent control (MCP)

The Okinawa engine ships an optional MCP server, so an agent (e.g. Claude
Code) can connect to a running wadviewer and visually inspect what it is
rendering. Launch the viewer with `--mcp`:

```bash
xmake run wadviewer wads/doom1.wad E1M1 --mcp
```

It logs `MCP server listening on http://127.0.0.1:8765/mcp`. Add
`--no-input` to ignore your physical mouse/keyboard so the instance is
driven only through the MCP (the window stays visible to watch). Then
register it in Claude Code:

```bash
claude mcp add --transport http okinawa http://127.0.0.1:8765/mcp
```

Tools exposed by the server:

- `view_frame` — return the current frame as an image (for the agent).
- `screenshot` — write the frame to a PNG file (for a human).
- `press_key` / `press_keys` — hold a key (or several) for a duration to
  drive the avatar (W/A/S/D move, 1-9 switch camera, SPACE/T/R/F actions).
- `look` — rotate the active camera by yaw/pitch degrees.
- `set_camera_pose` — teleport/orient the active camera directly.
- `get_state` — camera pose, fps, scene counts, window size, memory.

### Compiling the MCP server in or out

Whether the MCP server is *compiled into* the binary is a build-time choice
with a **mode-dependent default**: it is **included in debug builds and
excluded from release builds** (release defines `NDEBUG`, which the engine's
`mcp-config.hpp` uses to decide). Override the default with at most one of:

```bash
xmake f --mcp=y      # force the MCP server IN  (e.g. to use it in a release build)
xmake f --no-mcp=y   # force the MCP server OUT (e.g. a lean debug build)
```

When excluded, the server code is an empty translation unit — no MCP/HTTP code
or its dependencies land in the binary, and the runtime `--mcp` flag logs a
warning and does nothing. (This is the okinawa engine's `mcp` build option; see
its readme.)

## Dependencies

Third-party dependencies are managed by xmake (xrepo) and fetched
automatically; the engine is built from source via a git submodule:

- [okinawa](https://github.com/okinawa-dev/okinawa.cpp): 3D game engine providing core functionality (built from source, git submodule).
- [GLM](https://github.com/g-truc/glm): OpenGL Mathematics library.
- [STB](https://github.com/nothings/stb): Single file libraries (Image loading).
- [mapbox/earcut](https://github.com/mapbox/earcut.hpp): Header-only polygon-with-holes triangulation (sector floors/ceilings).
- [OpenGL](https://www.opengl.org/): 3D graphics.
- [GLFW](https://github.com/glfw/glfw): OpenGL context and window management.
