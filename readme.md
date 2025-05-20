# WAD Viewer

A WAD file viewer built with C++ and OpenGL using the [Okinawa engine](https://github.com/okinawa-dev/okinawa.cpp). This tool allows you to view WAD ([Where's All the Data](https://doomwiki.org/wiki/WAD)) files, commonly used in games like DOOM, displaying their 3D geometry and textures.

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

### Debug Build

```bash
# Clone the repository with submodules
git clone --recursive https://github.com/yourusername/wadviewer.git
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
# Clone the repository with submodules
git clone --recursive https://github.com/yourusername/wadviewer.git
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

- WASD: Move camera forward/left/backward/right.
- Mouse: Look around.
- Esc: Exit program.

## Project Structure

- `src/`: Source code files.
  - `main.cpp`: Main application entry point.
  - `wad.hpp/cpp`: WAD file parsing and handling.
  - `wad-converter.hpp/cpp`: WAD to 3D geometry conversion.
- `okinawa.cpp/`: Submodule containing the Okinawa game engine.
- `wads/`: Example WAD files.

## Dependencies

All dependencies are managed through Conan:
- GLM: OpenGL Mathematics library
- GLFW: OpenGL context and window management
- STB: Image loading
- nlohmann_json: JSON parsing
- OpenGL: 3D graphics
