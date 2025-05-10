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

#include "./wad-converter.hpp"
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
  OkInput     *input  = OkCore::getInput();
  OkInputState state  = input->getState();
  OkCamera    *camera = OkCore::getCamera();

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

  // Log only once per second for debugging
  static int frameCount = 0;
  if (frameCount++ % 60 == 0) {  // Assuming 60 FPS, adjust if different
    OkPoint position = camera->getPosition();
    OkLogger::info("Camera pos: " + position.toString());
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
 * @brief Position the camera to view the level geometry.
 * @param camera The camera to position.
 * @param items Vector of level items.
 */
void positionCameraForLevel(OkCamera                    *camera,
                            const std::vector<OkItem *> &items) {
  // Find level bounds
  float minX = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float minY = std::numeric_limits<float>::max();
  float maxY = std::numeric_limits<float>::lowest();
  float minZ = std::numeric_limits<float>::max();
  float maxZ = std::numeric_limits<float>::lowest();

  for (size_t i = 0; i < items.size(); ++i) {
    float   radius = items[i]->getRadius();
    OkPoint pos    = items[i]->getPosition();
    minX           = std::min(minX, pos.x() - radius);
    maxX           = std::max(maxX, pos.x() + radius);
    minY           = std::min(minY, pos.y() - radius);
    maxY           = std::max(maxY, pos.y() + radius);
    minZ           = std::min(minZ, pos.z() - radius);
    maxZ           = std::max(maxZ, pos.z() + radius);
  }

  // Calculate level dimensions
  float width       = maxX - minX;
  float height      = maxY - minY;
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
  float fov       = 45.0f;
  float nearPlane = 0.1f;
  float farPlane  = distance * 4.0f;  // Make sure we can see the whole level
  camera->setPerspective(fov, nearPlane, farPlane);

  OkLogger::info("Level bounds: (" + std::to_string(minX) + "," +
                 std::to_string(minY) + "," + std::to_string(minZ) + ") to (" +
                 std::to_string(maxX) + "," + std::to_string(maxY) + "," +
                 std::to_string(maxZ) + ")");
  OkLogger::info("Camera positioned at: " + cameraPos.toString());
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
    OkLogger::info("Level name: " +
                   std::string(level.name, strnlen(level.name, 8)));

    // Create level geometry using the converter
    WADConverter          converter;
    std::vector<OkItem *> levelItems = converter.createLevelGeometry(level);

    // Create a secondary camera in the player start position
    OkPoint  *playerStart = converter.getPlayerStartPosition(level);
    OkCamera *povCamera   = new OkCamera(OkConfig::getInt("window.width"),
                                         OkConfig::getInt("window.height"));

    OkCore::addCamera(povCamera);
    // Slower speed for POV camera
    povCamera->setMaxVelocity(cameraSpeed * 0.5f);
    povCamera->setPosition(*playerStart);
    povCamera->setRotation(0.0f, 0.0f, 0.0f);
    povCamera->setPerspective(45.0f, 0.1f, 2000.0f);

    for (size_t i = 0; i < levelItems.size(); ++i) {
      levelItems[i]->setWireframe(false);
      scene->addItem(levelItems[i]);
    }

    // Position camera to view the entire level
    positionCameraForLevel(camera, levelItems);

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
