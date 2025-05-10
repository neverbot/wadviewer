#include "../okinawa.cpp/src/config/config.hpp"
#include "../okinawa.cpp/src/core/camera.hpp"
#include "../okinawa.cpp/src/core/core.hpp"
#include "../okinawa.cpp/src/handlers/scenes.hpp"
#include "../okinawa.cpp/src/importers/wavefront.hpp"
#include "../okinawa.cpp/src/input/input.hpp"
#include "../okinawa.cpp/src/scene/scene.hpp"
#include "../okinawa.cpp/src/utils/logger.hpp"
#include <cmath>
#include <iostream>
#include <map>

#include "./wad-renderer.hpp"
#include "./wad.hpp"

// enum with the possible formats for the file to view
enum class Format {
  WAD,
  JSON,
  JSON_VERBOSE,
  DSL,
  DSL_VERBOSE
};

/**
 * @brief callback function for the step phase of the engine loop.
 * @param deltaTime Time since the last frame in milliseconds.
 */
void stepCallback(float deltaTime) {
  float timeInSeconds = deltaTime / 1000.0f;

  OkInput     *input  = OkCore::getInput();
  OkInputState state  = input->getState();
  OkCamera    *camera = OkCore::getCamera();

  float velocity = camera->getSpeedMagnitude() * timeInSeconds;

  OkPoint position = camera->getPosition();
  OkPoint forward  = camera->getRotation().getForwardVector();
  OkPoint right    = camera->getRotation().getRightVector();
  OkPoint up       = camera->getRotation().getUpVector();

  // Forward/Backward movement along forward vector
  if (state.forward) {
    position = position + (forward * velocity);
  }
  if (state.backward) {
    position = position - (forward * velocity);
  }

  // Left/Right movement along right vector
  if (state.strafeLeft) {
    position = position - (right * velocity);
  }
  if (state.strafeRight) {
    position = position + (right * velocity);
  }

  // Update camera position
  camera->setPosition(position);

  // Debug logging
  static int frameCount = 0;
  if (frameCount++ % 60 == 0) {
    OkLogger::info("Camera pos: " + std::to_string(position.x()) + ", " +
                   std::to_string(position.y()) + ", " +
                   std::to_string(position.z()));
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

  OkLogger::info("Camera positioned at: " + cameraPos.toString());
  OkLogger::info(
      "Camera looking at pitch: " + std::to_string(glm::degrees(pitch)) +
      " yaw: " + std::to_string(glm::degrees(yaw)));
}

/**
 * @brief Main function for the WAD viewer application.
 * @param argc Number of command line arguments.
 * @param argv Command line arguments.
 * @return Exit status.
 */
int main(int argc, char *argv[]) {
  OkLogger::info("Main :: Starting up...");
  OkCore::initialize();

  const float cameraSpeed = 200.0f;  // Increased from 40.0f to 200.0f

  // Set initial camera
  OkCamera  *camera = OkCore::getCamera();
  OkPoint    position(0.0f, 100.0f, 200.0f);  // Lower height, moved back
  float      pitch = glm::radians(-30.0f);    // Looking down 30 degrees
  float      yaw   = 0.0f;                    // Looking towards -Z
  OkRotation rotation(pitch, yaw, 0.0f);

  camera->setSpeed(cameraSpeed, cameraSpeed,
                   cameraSpeed);  // Set speed in all directions
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
    OkLogger::info("Game :: Current scene: " + currentScene->getName());
  } else {
    OkLogger::error("Game :: No current scene found");
  }

  // ******************************************************************************************
  // ******************************************************************************************
  // ******************************************************************************************
  // ******************************************************************************************
  // ******************************************************************************************
  // ******************************************************************************************

  // clang-format off
  if (argc < 2 || argc > 4) {
    std::cout << "Usage: wadviewer [-format] <content_file> [<level_name>]\n";
    std::cout << "  -format     : Optional format of input file (-wad, -json, -dsl). Default: wad\n";
    std::cout << "  content_file: Path to the input file (WAD/JSON/DSL format)\n";
    std::cout << "  level_name  : Optional. Name of the level to display. Default: first level in the file\n";
    return 1;
  }
  // clang-format on

  Format      format = Format::WAD;  // Default format
  std::string contentFile;
  std::string levelName = "";  // Empty string means use first level

  // Check if first argument is a format specification
  if (argv[1][0] == '-') {
    std::string formatStr = argv[1];
    formatStr             = formatStr.substr(1);  // Remove the leading '-'

    if (formatStr == "wad") {
      format = Format::WAD;
    } else if (formatStr == "json") {
      format = Format::JSON;
    } else if (formatStr == "dsl") {
      format = Format::DSL;
    } else {
      std::cerr << "Invalid format specified. Using default (wad)\n";
    }

    contentFile = argv[2];
    if (argc == 4) {
      levelName = argv[3];
    }
  } else {
    // No format specified, use defaults
    contentFile = argv[1];
    if (argc == 3) {
      levelName = argv[2];
    }
  }

  try {
    WAD wad(contentFile);  // Verbose mode
    wad.processWAD();

    // If no level name was provided, use the first level
    if (levelName.empty()) {
      levelName = wad.getLevelNameByIndex(0);
      // OkLogger::info("Using first level: " + levelName);
    }

    WAD::Level level = wad.getLevel(levelName);
    OkLogger::info("Level name: " + level.name);

    // Create level geometry using the renderer
    WADRenderer           renderer;
    std::vector<OkItem *> levelItems = renderer.createLevelGeometry(level);

    for (size_t i = 0; i < levelItems.size(); ++i) {
      levelItems[i]->setWireframe(false);
      scene->addItem(levelItems[i]);
    }

    // Position camera to view the level
    positionCameraForItem(camera, levelItems[0]);

    // Add coordinate axes for reference
    float              axisLength = 100.0f;
    std::vector<float> axisVerts  = {
        0, 0, 0, axisLength, 0,          0,           // X axis
        0, 0, 0, 0,          axisLength, 0,           // Y axis
        0, 0, 0, 0,          0,          -axisLength  // Z axis (-Z is forward)
    };
    std::vector<unsigned int> axisIndices = {0, 1, 2, 3, 4, 5};
    OkItem *axes = new OkItem("axes", axisVerts.data(), axisVerts.size(),
                              axisIndices.data(), axisIndices.size());
    axes->setDrawMode(GL_LINES);
    scene->addItem(axes);

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

  OkLogger::info("Scene :: Item count: " +
                 std::to_string(scene->getItemCount()));

  // Start game loop
  OkCore::loop(stepCallback, drawCallback);

  // Cleanup
  // delete item;
  // delete item2;
  // delete model;
  // delete model2;
  // delete floor;

  // objects are deleted in the scene destructor
  delete scene;

  return 1;
}
