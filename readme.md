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

- [clang](https://clang.llvm.org/): A compiler for C and C++ languages.
- [cmake](https://cmake.org/): A cross-platform build system generator.
- [conan](https://conan.io/): A package manager for C and C++ libraries.

## Building from Source

The executable will be created in `build/bin/wadviewer`.

### Prerequisites

The project depends on the Okinawa engine, which must be built and installed as a local Conan package first:

```bash
# Clone and create the Okinawa package
git clone https://github.com/okinawa-dev/okinawa.cpp.git
cd okinawa.cpp
conan create . --build=missing
cd ..
```

#### Developing the engine alongside wadviewer (Conan editable mode)

If you are actively editing the Okinawa engine, you can avoid
re-running `conan create` on every change by putting it in **editable
mode**, so wadviewer links directly against the engine source tree:

```bash
# Mark the engine editable (once)
conan editable add ../okinawa.cpp

# Build the engine WITHOUT tests (coverage instrumentation would
# otherwise leak into the static lib and break the consumer link)
cd ../okinawa.cpp
conan install . --output-folder=build -s build_type=Debug --build=missing
cmake --preset debug -DOKINAWA_BUILD_TESTS=OFF
cmake --build --preset debug
cd ../wadviewer

# Then build wadviewer as usual; iterate: edit engine -> rebuild engine -> rebuild wadviewer
# To go back to the packaged engine:
#   conan editable remove --refs=okinawa/0.1.0
```

### Debug Build

```bash
# Clone the repository
git clone https://github.com/neverbot/wadviewer.git
cd wadviewer

# Install dependencies using Conan
conan install . --output-folder=build -s build_type=Debug --build=missing

# Configure with CMake
cmake --preset debug

# Build the project
cmake --build --preset debug
```

### Release Build

```bash
# Clone the repository (if not already done)
git clone https://github.com/neverbot/wadviewer.git
cd wadviewer

# Install dependencies using Conan
conan install . --output-folder=build -s build_type=Release --build=missing

# Configure with CMake
cmake --preset release

# Build the project
cmake --build --preset release
```

## Usage

The program can be run in these ways:

```bash
# Using default WAD format
./build/bin/wadviewer <content_file> [<level_name>]

# Specifying format explicitly
./build/bin/wadviewer -wad <content_file> [<level_name>]
./build/bin/wadviewer -json <content_file> [<level_name>]
./build/bin/wadviewer -dsl <content_file> [<level_name>]
```

Example: 
```bash
./build/bin/wadviewer wads/doom1.wad E1M1
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

## Dependencies

All dependencies are managed through Conan:

- [okinawa](https://github.com/okinawa-dev/okinawa.cpp): 3D game engine providing core functionality (local package).
- [GLM](https://github.com/g-truc/glm): OpenGL Mathematics library.
- [STB](https://github.com/nothings/stb): Single file libraries (Image loading).
- [nlohmann_json](https://github.com/nlohmann/json): JSON parsing.
- [OpenGL](https://www.opengl.org/): 3D graphics.
- [GLFW](https://github.com/glfw/glfw): OpenGL context and window management.
