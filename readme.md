# WAD Viewer

A WAD file viewer built with C++ and OpenGL using the [Okinawa engine](https://github.com/okinawa-dev/okinawa.cpp). This tool allows you to view WAD ([Where's All the Data](https://doomwiki.org/wiki/WAD)) files, commonly used in games like DOOM, displaying their 3D geometry and textures.

<p align="center">
  <img width="500" alt="screenshot" src="/assets/project/screenshot.png">
</p>

## Features

- Load and view WAD file geometry.
- Multiple input formats supported (WAD, JSON, DSL).
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
# Using default WAD format
xmake run wadviewer <content_file> [<level_name>]

# Specifying format explicitly
xmake run wadviewer -wad <content_file> [<level_name>]
xmake run wadviewer -json <content_file> [<level_name>]
xmake run wadviewer -dsl <content_file> [<level_name>]
```

Example:
```bash
xmake run wadviewer wads/doom1.wad E1M1
```

### Command Line Arguments

- `-wad`: Use WAD format (default if no format specified)
- `-json`: Use JSON format
- `-dsl`: Use DSL format
- `content_file`: Path to the input file
- `level_name`: Optional. Name of the level to display. If not specified, the first level in the file will be used.

### Controls

```
╔══════════════════════════════════════════════════════════════╗
║                        CONTROLS HELP                         ║
╠══════════════════════════════════════════════════════════════╣
║  TEXTURE VIEWER:                                             ║
║    SPACE BAR  - Cycle through textures                       ║
║    T          - Toggle texture viewer visibility             ║
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

It logs `MCP server listening on http://127.0.0.1:8765/mcp`. Then register
it in Claude Code:

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

The server is compiled in by default (debug builds); exclude it with
`xmake f --mcp=n`.

## Dependencies

Third-party dependencies are managed by xmake (xrepo) and fetched
automatically; the engine is built from source via a git submodule:

- [okinawa](https://github.com/okinawa-dev/okinawa.cpp): 3D game engine providing core functionality (built from source, git submodule).
- [GLM](https://github.com/g-truc/glm): OpenGL Mathematics library.
- [STB](https://github.com/nothings/stb): Single file libraries (Image loading).
- [nlohmann_json](https://github.com/nlohmann/json): JSON parsing.
- [OpenGL](https://www.opengl.org/): 3D graphics.
- [GLFW](https://github.com/glfw/glfw): OpenGL context and window management.
