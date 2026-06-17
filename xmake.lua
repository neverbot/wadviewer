-- wadviewer - xmake build
--
-- Build:        xmake
-- Run:          xmake run wadviewer wads/doom1.wad E1M1
--
-- The okinawa engine is built from source as a local sub-dependency
-- (see includes() below), so editing the engine and rebuilding wadviewer
-- picks the changes up directly -- no binary package, no symlinks.
-- Third-party dependencies are fetched and built by xrepo automatically.

set_project("wadviewer")
set_version("0.1.0")

set_languages("cxx17")
set_warnings("all")                 -- -Wall
add_cxflags("-Wundef", "-Wmacro-redefined", "-Wextra-semi", {tools = {"clang", "gcc"}})
set_symbols("debug")                -- always emit debug info (-g)

-- Keep compile_commands.json in sync for clangd / tooling.
add_rules("plugin.compile_commands.autoupdate", {outputdir = ".", lsp = "clangd"})

-- Build modes. Debug is the default; `xmake f -m release` for release.
add_rules("mode.debug", "mode.release")
set_defaultmode("debug")

-- wadviewer's own third-party dependencies. glm / glfw / stb / opengl come
-- transitively from the engine (it exposes them as public packages).
add_requires("nlohmann_json")
-- Robust polygon-with-holes triangulation for sector floors/ceilings
-- (header-only; used by wad-generate).
add_requires("mapbox_earcut")

-- Build the okinawa engine from source. It is vendored as a git submodule
-- at ./okinawa.cpp (run `git submodule update --init` after cloning), so the
-- build does not depend on the local directory layout.
includes("okinawa.cpp")

-- =========================================================================
-- Viewer executable
-- =========================================================================
target("wadviewer")
    set_kind("binary")
    add_files("src/*.cpp")
    add_deps("okinawa")
    add_packages("nlohmann_json", "mapbox_earcut")

    -- Run from the project root: the app resolves wads/ by relative path and
    -- the engine discovers its assets by walking up from the working dir to
    -- ../okinawa.cpp/assets.
    set_rundir("$(projectdir)")

    -- macOS windowing/runtime frameworks (the engine links them privately;
    -- the final executable needs them too).
    if is_plat("macosx") then
        add_frameworks("Cocoa", "IOKit", "CoreVideo", "OpenGL")
    end
