#include "okinawa/config/config.hpp"
#include "okinawa/core/camera.hpp"
#include "okinawa/core/core.hpp"
#include "okinawa/handlers/scenes.hpp"
#include "okinawa/input/input.hpp"
#include "okinawa/item/group.hpp"
#include "okinawa/item/item.hpp"
#include "okinawa/math/math.hpp"
#include "okinawa/math/point.hpp"
#include "okinawa/math/rotation.hpp"
#include "okinawa/scene/scene.hpp"
#include "okinawa/utils/logger.hpp"
#include <_string.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <glm/trigonometric.hpp>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "./gui.hpp"
#include "./wad-converter.hpp"
#include "./wad.hpp"

OkItem *item  = nullptr;
OkItem *item2 = nullptr;
GUI    *gui   = nullptr;

// Global level groups for sector-based organization
std::vector<OkItemGroup *> sectorGroups;

/**
 * @brief callback function for the step phase of the engine loop.
 * @param deltaTime Time since the last frame in milliseconds.
 */
void stepCallback(float deltaTime) {
  OkInput     *input  = OkCore::getInput();
  OkInputState state  = input->getState();
  OkCamera    *camera = OkCore::getCamera();  // current camera

  // Check for exit condition
  if (state.exit) {
    OkCore::askForExit();
    return;
  }

  // Update GUI
  if (gui) {
    gui->step(state);
  }

  // Toggle ceiling and floor visibility for all sectors
  static bool lastAction1State = false;
  if (state.action3 && !lastAction1State) {
    // Toggle visibility for ceiling and floor items in all sectors
    bool ceilingsFloorsVisible =
        OkConfig::getBool("viewer.ceilings-floors-visible");
    ceilingsFloorsVisible = !ceilingsFloorsVisible;
    OkConfig::setBool("viewer.ceilings-floors-visible", ceilingsFloorsVisible);

    for (size_t i = 0; i < sectorGroups.size(); i++) {
      // Get ceiling items and toggle visibility
      std::vector<OkItem *> ceilingItems =
          sectorGroups[i]->getItemsWithTag("ceiling");
      for (size_t j = 0; j < ceilingItems.size(); j++) {
        ceilingItems[j]->setVisible(ceilingsFloorsVisible);
      }

      // Get floor items and toggle visibility
      std::vector<OkItem *> floorItems =
          sectorGroups[i]->getItemsWithTag("floor");
      for (size_t j = 0; j < floorItems.size(); j++) {
        floorItems[j]->setVisible(ceilingsFloorsVisible);
      }
    }

    OkLogger::info("UI", "Ceiling/Floor visibility toggled: " +
                             std::string(ceilingsFloorsVisible ? "ON" : "OFF"));
  }
  lastAction1State = state.action3;

  // Toggle all debug gizmos (origin axes + the wireframe debug cubes) with F.
  // The texture-preview square has its own toggle (T) handled by the GUI.
  if (state.action4) {
    bool debugVisible = !OkConfig::getBool("viewer.debug-gizmos-visible");
    OkConfig::setBool("viewer.debug-gizmos-visible", debugVisible);

    // Green wireframe boxes for the inactive cameras (engine-drawn gizmo).
    OkConfig::setBool("graphics.drawCameras", debugVisible);

    for (size_t i = 0; i < sectorGroups.size(); i++) {
      sectorGroups[i]->setDrawOriginAxis(debugVisible);
    }
    if (item) {
      item->setVisible(debugVisible);
      item->setDrawOriginAxis(debugVisible);
    }
    if (item2) {
      item2->setVisible(debugVisible);
      item2->setDrawOriginAxis(debugVisible);
    }

    OkLogger::info("UI", "Debug gizmos toggled: " +
                             std::string(debugVisible ? "ON" : "OFF"));
  }

  OkPoint forward = camera->getRotation().getForwardVector();
  OkPoint right   = camera->getRotation().getRightVector();
  OkPoint direction(0.0f, 0.0f, 0.0f);

  // Calculate movement direction based on input
  if (state.forward) {
    direction = direction + forward;
  }
  if (state.backward) {
    direction = direction - forward;
  }
  if (state.strafeLeft) {
    direction = direction - right;
  }
  if (state.strafeRight) {
    direction = direction + right;
  }

  // Base movement speed (units per second)
  const float baseSpeed = 50.0f;

  // Apply movement speed if there's input
  if (direction.x() != 0 || direction.y() != 0 || direction.z() != 0) {
    float magnitude =
        sqrt(direction.x() * direction.x() + direction.y() * direction.y() +
             direction.z() * direction.z());

    if (magnitude > 0.0001f) {  // Small epsilon to avoid floating point errors
      direction = direction.normalize() * baseSpeed;
    } else {
      direction = OkPoint(0.0f, 0.0f, 0.0f);
    }
  }

  // Set the camera's speed - this will be applied in OkObject::step
  camera->setSpeed(direction.x(), direction.y(), direction.z());

  // Rotate item2 on the XY plane
  // if (item2) {
  //   // Rotate 10 degree per second
  //   item2->rotate(0.0f, 0.0f, glm::radians(0.1f * deltaTime));
  // }
  if (item) {
    // Rotate 10 degree per second
    item->rotate(0.0f, glm::radians(0.1f * deltaTime), 0.0f);
  }

  // Log only once per second for debugging
  static int frameCount = 0;
  if (frameCount++ % 60 == 0) {  // Assuming 60 FPS, adjust if different
    OkPoint position = camera->getPosition();
    OkLogger::info("Camera", "pos: " + position.toString());
  }
}

/**
 * @brief callback function for the draw phase of the engine loop.
 * @param deltaTime Time since the last frame in milliseconds.
 */
void drawCallback(float deltaTime) {
  // Do whatever is needed, probably nothing here
}

/**
 * @brief Position the camera to view an item.
 * @param camera The camera to position.
 * @param item   The item to look at.
 */
void positionCameraForItem(OkCamera *camera, const OkItem *item) {
  float radius   = item->getRadius();
  float distance = radius * 2.0f;
  float height   = distance * 0.5f;

  // Position camera above and behind the origin (item center)
  OkPoint cameraPos(0.0f, height, distance);
  camera->setPosition(cameraPos);

  OkPoint targetPos(0.0f, 0.0f, 0.0f);  // Looking at origin
  OkPoint direction = targetPos - cameraPos;

  float pitch, yaw;
  OkMath::directionVectorToAngles(direction.normalize(), pitch, yaw);
  camera->setRotation(pitch, yaw, 0.0f);

  // Adjust perspective for item size
  float fov       = 45.0f;
  float nearPlane = 0.1f;
  float farPlane  = item->getRadius() * 5.0f;

  camera->setPerspective(fov, nearPlane, farPlane);

  OkLogger::info("Camera", "positioned at: " + cameraPos.toString());
  OkLogger::info("Camera",
                 "looking at pitch: " + std::to_string(glm::degrees(pitch)) +
                     " yaw: " + std::to_string(glm::degrees(yaw)));
}

/**
 * @brief Position the camera to view the level geometry.
 * @param camera The camera to position.
 * @param sectorGroups Vector of sector groups containing level geometry.
 */
void positionCameraForLevel(OkCamera                         *camera,
                            const std::vector<OkItemGroup *> &sectorGroups) {
  // Find level bounds
  float minX = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float minY = std::numeric_limits<float>::max();
  float maxY = std::numeric_limits<float>::lowest();
  float minZ = std::numeric_limits<float>::max();
  float maxZ = std::numeric_limits<float>::lowest();

  // Iterate through all sector groups and their items
  for (size_t i = 0; i < sectorGroups.size(); ++i) {
    std::vector<OkItem *> groupItems = sectorGroups[i]->getAllItems();
    for (size_t j = 0; j < groupItems.size(); ++j) {
      float   radius = groupItems[j]->getRadius();
      OkPoint pos    = groupItems[j]->getPosition();
      minX           = std::min(minX, pos.x() - radius);
      maxX           = std::max(maxX, pos.x() + radius);
      minY           = std::min(minY, pos.y() - radius);
      maxY           = std::max(maxY, pos.y() + radius);
      minZ           = std::min(minZ, pos.z() - radius);
      maxZ           = std::max(maxZ, pos.z() + radius);
    }
  }

  // Calculate level dimensions
  float width = maxX - minX;
  // float height      = maxY - minY;
  float depth       = maxZ - minZ;
  float levelRadius = sqrt(width * width + depth * depth) * 0.5f;

  // Position camera to see the whole level
  float distance     = levelRadius;  // * 2.0f;
  float cameraHeight = maxY + levelRadius * 0.1f;

  // Position camera above and behind the level center
  float   centerX = (minX + maxX) * 0.5f;
  float   centerZ = (minZ + maxZ) * 0.5f;
  OkPoint cameraPos(centerX, cameraHeight, centerZ + distance);
  camera->setPosition(cameraPos);

  // Look at level center
  OkPoint targetPos(centerX, (minY + maxY) * 0.5f, centerZ);
  OkPoint direction = targetPos - cameraPos;

  float pitch, yaw;
  OkMath::directionVectorToAngles(direction.normalize(), pitch, yaw);
  camera->setRotation(pitch, yaw, 0.0f);

  // Set up perspective to view entire level
  // DOOM uses a 90 degree HORIZONTAL FOV; with glm's vertical FOV at a 4:3
  // aspect that is ~74 degrees. (45 was far too narrow / zoomed in.)
  float fov       = 74.0f;
  float nearPlane = 0.1f;
  float farPlane  = distance * 4.0f;  // Make sure we can see the whole level
  camera->setPerspective(fov, nearPlane, farPlane);

  OkLogger::info("Level",
                 "bounds: (" + std::to_string(minX) + "," +
                     std::to_string(minY) + "," + std::to_string(minZ) +
                     ") to (" + std::to_string(maxX) + "," +
                     std::to_string(maxY) + "," + std::to_string(maxZ) + ")");
  OkLogger::info("Camera", "positioned at: " + cameraPos.toString());
}

/**
 * @brief Main function for the WAD viewer application.
 * @param argc Number of command line arguments.
 * @param argv Command line arguments.
 * @return Exit status.
 */
int main(int argc, char *argv[]) {
  try {
    OkLogger::info("Main", "Starting up...");

    // Display control instructions with ASCII frame
    // clang-format off
    std::string helpMessage = "\n"
        "╔══════════════════════════════════════════════════════════════╗\n"
        "║                        CONTROLS HELP                         ║\n"
        "╠══════════════════════════════════════════════════════════════╣\n"
        "║  TEXTURE VIEWER:                                             ║\n"
        "║    SPACE BAR  - Cycle through textures                       ║\n"
        "║    T          - Toggle texture viewer visibility             ║\n"
        "║    R          - Toggle ceiling/floor visibility              ║\n"
        "║                                                              ║\n"
        "║  CAMERAS:                                                    ║\n"
        "║    1          - Overview camera                              ║\n"
        "║    2          - Player start camera                          ║\n"
        "║    3          - Origin camera                                ║\n"
        "║                                                              ║\n"
        "║  MOVEMENT:                                                   ║\n"
        "║    W A S D    - Move forward/left/backward/right             ║\n"
        "║    MOUSE      - Look around                                  ║\n"
        "║    ESC        - Exit application                             ║\n"
        "╚══════════════════════════════════════════════════════════════╝";
    // clang-format on

    OkLogger::info("Help", helpMessage);

    // Initialize the engine
    if (!OkCore::initialize()) {
      OkLogger::error("Main", "Failed to initialize the engine, exiting");
      return 1;
    }

    // Initialize wadviewer-specific config values
    OkConfig::setBool("viewer.ceilings-floors-visible", true);

    // Configure logger filtering - disable some logs to reduce noise
    OkLogger::disableLogType("Item");
    OkLogger::disableLogType("Scene");
    OkLogger::disableLogType("Level");
    OkLogger::disableLogType("ItemGroup");
    OkLogger::disableLogType("TextureHandler");

    OkLogger::disableLogType("WADTextures");
    OkLogger::disableLogType("WADGeometry");
    OkLogger::disableLogType("WADGenerator");

    // Set initial camera
    OkCamera  *camera = OkCore::getCamera();
    OkPoint    position(0.0f, 100.0f, 200.0f);  // Lower height, moved back
    float      pitch = glm::radians(-30.0f);    // Looking down 30 degrees
    float      yaw   = 0.0f;                    // Looking towards -Z
    OkRotation rotation(pitch, yaw, 0.0f);

    // Set maximum velocity (don't set speed directly)
    const float cameraSpeed = 10.0f;  // Units per second
    camera->setMaxVelocity(cameraSpeed);

    // Not needed, will be set in positionCameraForItem
    // camera->setPosition(position);
    // camera->setRotation(rotation);
    // camera->setPerspective(45.0f, 0.1f, 2000.0f);  // Increased far plane

    // Create main scene
    OkScene *scene = new OkScene("MainScene");

    // Set up scene
    OkSceneHandler *sceneHandler = OkCore::getSceneHandler();
    sceneHandler->addScene(scene, "MainScene");
    sceneHandler->setScene(0);

    OkScene *currentScene = sceneHandler->getCurrentScene();
    if (currentScene) {
      OkLogger::info("Game", "Current scene: " + currentScene->getName());
    } else {
      OkLogger::error("Game", "No current scene found");
    }

    // ******************************************************************************************
    // ******************************************************************************************
    // ******************************************************************************************
    // ******************************************************************************************
    // ******************************************************************************************
    // ******************************************************************************************

    // clang-format off
    if (argc < 2 || argc > 11) {
      std::cout << "Usage: wadviewer [-wad] [-iwad <iwad_file>] <content_file> [<level_name>] [--verbose] [--mcp] [--no-input]\n";
      std::cout << "  -wad        : Optional WAD-format selector (the default and only format)\n";
      std::cout << "  -iwad <file>: Optional base IWAD loaded first for shared resources (e.g. doom2.wad for DOOM II PWADs)\n";
      std::cout << "  content_file: Path to the WAD file\n";
      std::cout << "  level_name  : Optional. Name of the level to display. Default: first level in the file\n";
      std::cout << "  --verbose   : Optional. Enable verbose debug output\n";
      std::cout << "  --mcp       : Optional. Start the MCP server (http://127.0.0.1:8765/mcp) for agent control\n";
      std::cout << "  --no-input  : Optional. Ignore physical mouse/keyboard input (control only via the MCP)\n";
      return 1;
    }
    // clang-format on

    std::string contentFile;
    std::string iwadFile   = "";     // Optional base IWAD (resources)
    std::string levelName  = "";     // Empty string means use first level
    bool        verbose    = false;  // Default: not verbose
    bool        mcpEnabled = false;  // Enable the in-engine MCP server
    bool        noInput    = false;  // Ignore physical input (MCP-only)

    // Single pass over the arguments: collect flags, the optional base IWAD
    // (-iwad <path>) and the positional content file / level name. Anything not
    // recognised as a flag is positional: the first positional is the content
    // file, the second the level name.
    std::vector<std::string> positionals;
    for (int i = 1; i < argc; i++) {
      std::string arg = argv[i];
      if (arg == "--verbose") {
        verbose = true;
      } else if (arg == "--mcp") {
        mcpEnabled = true;
      } else if (arg == "--no-input") {
        noInput = true;
      } else if (arg == "-iwad") {
        if (i + 1 < argc) {
          iwadFile = argv[++i];  // consume the path that follows
        } else {
          std::cerr << "-iwad requires a path argument\n";
          return 1;
        }
      } else if (arg == "-wad") {
        // WAD-format selector (the only format); accepted as a no-op.
      } else {
        positionals.push_back(arg);
      }
    }

    if (positionals.empty()) {
      std::cerr << "No content file given.\n";
      return 1;
    }
    contentFile = positionals[0];
    if (positionals.size() >= 2) {
      levelName = positionals[1];
    }

    try {
      // With a base IWAD, load it first (resources) then the content WAD on top
      // so a DOOM II PWAD's maps resolve the IWAD's textures/flats/palette.
      // Without one, the list holds just the content file (unchanged
      // behaviour).
      std::vector<std::string> wadFiles;
      if (!iwadFile.empty()) {
        wadFiles.push_back(iwadFile);
      }
      wadFiles.push_back(contentFile);

      WAD wad(wadFiles, verbose);
      wad.processWAD();

      // If no level name was provided, use the first level
      if (levelName.empty()) {
        levelName = wad.getLevelNameByIndex(0);
        // OkLogger::info("Using first level: " + levelName);
      }

      WAD::Level level = wad.getLevel(levelName);
      OkLogger::info("Level name: " +
                     std::string(level.name, strnlen(level.name, 8)));

      // Create level geometry using the converter
      sectorGroups = WADConverter::convertLevel(level);

      // Add sector groups directly to the scene
      for (size_t i = 0; i < sectorGroups.size(); i++) {
        scene->addObject(sectorGroups[i]);
        OkLogger::info("Scene", "Added sector group " + std::to_string(i) +
                                    " (" + sectorGroups[i]->getName() +
                                    ") to scene at position: " +
                                    sectorGroups[i]->getPosition().toString());
      }

      // Log sector group information
      OkLogger::info("Level", "converted to " +
                                  std::to_string(sectorGroups.size()) +
                                  " sector groups");

      for (size_t i = 0; i < sectorGroups.size(); i++) {
        OkItemGroup *group        = sectorGroups[i];
        int          wallCount    = group->getItemCountWithTag("wall");
        int          floorCount   = group->getItemCountWithTag("floor");
        int          ceilingCount = group->getItemCountWithTag("ceiling");

        OkLogger::info("Level",
                       "Sector " + std::to_string(i) + " (" + group->getName() +
                           "): " + std::to_string(group->getItemCount()) +
                           " items - Walls: " + std::to_string(wallCount) +
                           ", Floors: " + std::to_string(floorCount) +
                           ", Ceilings: " + std::to_string(ceilingCount));

        group->setDrawOriginAxisForAll(false);
        group->setDrawOriginAxis(false);
      }

      // Debug gizmos (origin axes + wireframe cubes) start HIDDEN; the F key
      // toggles them all on/off (see stepCallback). Starting clean keeps the
      // default view uncluttered for inspecting geometry.
      OkConfig::setBool("viewer.debug-gizmos-visible", false);
      OkConfig::setBool("graphics.drawCameras", false);

      // Position camera to view the entire level
      positionCameraForLevel(camera, sectorGroups);

      // ************************************************************************
      // Secondary Camera
      // Create a secondary camera in the player start position
      OkPoint  *playerStart = WADConverter::getPlayerStartPosition(level);
      OkCamera *povCamera =
          new OkCamera("Player Camera", OkConfig::getInt("window.width"),
                       OkConfig::getInt("window.height"));

      OkCore::addCamera(povCamera);
      // Slower speed for POV camera
      povCamera->setMaxVelocity(cameraSpeed * 0.5f);
      povCamera->setPosition(*playerStart);
      povCamera->setRotation(0.0f, 0.0f, 0.0f);
      // DOOM-like field of view: 90 deg horizontal ~= 74 deg vertical at 4:3.
      povCamera->setPerspective(74.0f, 0.1f, 2000.0f);
      // ************************************************************************

      // ************************************************************************
      // Third camera in 0,0,0
      OkCamera *originCamera =
          new OkCamera("Origin Camera", OkConfig::getInt("window.width"),
                       OkConfig::getInt("window.height"));

      OkCore::addCamera(originCamera);
      // Slower speed for POV camera
      originCamera->setMaxVelocity(cameraSpeed * 0.5f);
      originCamera->setPosition(0.0f, 0.0f, 0.0f);
      originCamera->setRotation(0.0f, 0.0f, 0.0f);
      originCamera->setPerspective(74.0f, 0.1f, 2000.0f);
      // ************************************************************************

      // Initialize GUI
      gui = new GUI(camera);

      // ************************************************************************

      // ************************************************************************
      // Create test items
      std::vector<float> vertices = {
          // Positions         // Texture coords
          0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  // top right
          0.5f,  -0.5f, 0.0f, 1.0f, 0.0f,  // bottom right
          -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,  // bottom left
          -0.5f, 0.5f,  0.0f, 0.0f, 1.0f   // top left
      };

      std::vector<unsigned int> indices = {
          0, 1, 3,  // first Triangle
          1, 2, 3   // second Triangle
      };

      item =
          new OkItem("cube", vertices.data(), static_cast<int>(vertices.size()),
                     indices.data(), static_cast<int>(indices.size()));
      item->setWireframe(true);
      item->setDrawOriginAxis(false);
      item->setVisible(false);  // Debug gizmo: hidden until toggled with F.

      item2 = new OkItem("cube2", vertices.data(),
                         static_cast<int>(vertices.size()), indices.data(),
                         static_cast<int>(indices.size()));
      item2->setWireframe(true);
      item2->rotate(0.0f, glm::radians(90.0f), 0.0f);
      item2->setDrawOriginAxis(false);
      item2->setVisible(false);  // Debug gizmo: hidden until toggled with F.

      scene->addObject(item);
      item2->attachTo(item);
      // scene->addItem(item2);
      item->setPosition(-2.0f, 0.0f, -10.0f);  // Left square
      item2->setPosition(2.0f, 0.0f,
                         0.0f);  // Right square (will be relative to item)
      // ************************************************************************

    } catch (const std::exception &e) {
      std::cerr << "Error: " << e.what() << "\n";
      return 1;
    }

    // ******************************************************************************************
    // ******************************************************************************************
    // ******************************************************************************************
    // ******************************************************************************************
    // ******************************************************************************************
    // ******************************************************************************************

    OkLogger::info("Scene",
                   "Object count: " + std::to_string(scene->getObjectCount()));

    // Enable the MCP server if requested (--mcp), so an external agent can
    // connect over local HTTP and observe/drive the app while it runs.
    if (mcpEnabled) {
      OkCore::enableMcpServer();
    }

    // Ignore the user's physical input (mouse/keyboard) if requested, so an
    // MCP-driven instance can only be controlled through the MCP server.
    if (noInput) {
      OkCore::setIgnoreUserInput(true);
    }

    // Verify engine is properly initialized before starting the loop
    if (OkCore::getWindow() != nullptr && OkCore::getShaderProgram() != 0) {
      // Start game loop
      OkCore::loop(stepCallback, drawCallback);
    } else {
      OkLogger::error(
          "Main :: Engine not properly initialized, cannot start main loop");
    }

    // Cleanup
    delete gui;  // Clean up GUI

    // objects are deleted in the scene destructor
    delete scene;

    return 1;
  } catch (const std::exception &e) {
    OkLogger::error("Main", "Unhandled exception: " + std::string(e.what()));
    return 1;
  } catch (...) {
    OkLogger::error("Main", "Unknown unhandled exception");
    return 1;
  }
}
