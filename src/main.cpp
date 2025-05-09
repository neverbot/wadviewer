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
 * @brief Convert a WAD level to an OkItem.
 * @param level The WAD level to convert.
 * @return A pointer to the created OkItem.
 */
OkItem *WADToOkItem(const WAD::Level &level) {
  // Calculate level bounds
  float minX = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float minY = std::numeric_limits<float>::max();
  float maxY = std::numeric_limits<float>::lowest();

  for (const auto &vertex : level.vertices) {
    minX = std::min(minX, static_cast<float>(vertex.x));
    maxX = std::max(maxX, static_cast<float>(vertex.x));
    minY = std::min(minY, static_cast<float>(vertex.y));
    maxY = std::max(maxY, static_cast<float>(vertex.y));
  }

  // Calculate center point and dimensions
  float centerX = (minX + maxX) / 2.0f;
  float centerY = (minY + maxY) / 2.0f;

  // Optional: Calculate scale to normalize size
  float       width        = maxX - minX;
  float       height       = maxY - minY;
  float       maxDimension = std::max(width, height);
  const float SCALE        = 1.0f;  // Adjust to scale the level

  // Convert WAD vertices to OpenGL format, centered around origin
  std::vector<float> levelVertices;
  for (const auto &vertex : level.vertices) {
    // Subtract center to normalize around origin
    float normalizedX = (static_cast<float>(vertex.x) - centerX) * SCALE;
    float normalizedY = (static_cast<float>(vertex.y) - centerY) * SCALE;

    levelVertices.push_back(normalizedX);   // x
    levelVertices.push_back(0.0f);          // y (flat map)
    levelVertices.push_back(-normalizedY);  // z
    levelVertices.push_back(0.0f);          // u texture coord
    levelVertices.push_back(0.0f);          // v texture coord
  }

  // Rest of the function remains the same
  std::vector<unsigned int> levelIndices;
  for (const auto &linedef : level.linedefs) {
    unsigned int start = linedef.start_vertex;
    unsigned int end   = linedef.end_vertex;

    levelIndices.push_back(start);
    levelIndices.push_back(end);
    levelIndices.push_back(start + 1);

    levelIndices.push_back(end);
    levelIndices.push_back(end + 1);
    levelIndices.push_back(start + 1);
  }

  OkItem *levelItem =
      new OkItem("level_geometry", levelVertices.data(), levelVertices.size(),
                 levelIndices.data(), levelIndices.size());

  // Log the normalization info
  OkLogger::info("Level bounds: (" + std::to_string(minX) + "," +
                 std::to_string(minY) + ") to (" + std::to_string(maxX) + "," +
                 std::to_string(maxY) + ")");
  OkLogger::info("Level center: (" + std::to_string(centerX) + "," +
                 std::to_string(centerY) + ")");
  OkLogger::info("Level dimensions: " + std::to_string(width) + " x " +
                 std::to_string(height));

  return levelItem;
}

/**
 * @brief Position the camera to view an item.
 * @param camera The camera to position.
 * @param item   The item to look at.
 */
void positionCameraForItem(OkCamera *camera, const OkItem *item) {
  float radius   = item->getRadius();
  float distance = radius * 0.5f;
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
  float fov       = 90.0f;
  float nearPlane = 0.1f;
  float farPlane  = item->getRadius() * 5.0f;

  camera->setPerspective(fov, nearPlane, farPlane);

  OkLogger::info("Camera positioned at height: " + std::to_string(height) +
                 ", distance: " + std::to_string(distance));
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

  camera->setPosition(position);
  camera->setRotation(rotation);
  camera->setSpeed(cameraSpeed, cameraSpeed,
                   cameraSpeed);                 // Set speed in all directions
  camera->setPerspective(45.0f, 0.1f, 2000.0f);  // Increased far plane

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

  OkItem *levelItem = nullptr;

  try {
    WAD wad(contentFile);
    wad.processWAD();

    // If no level name was provided, use the first level
    if (levelName.empty()) {
      levelName = wad.getLevelNameByIndex(0);
      // OkLogger::info("Using first level: " + levelName);
    }

    WAD::Level level = wad.getLevel(levelName);
    OkLogger::info("Level name: " + level.name);

    // Create level geometry
    levelItem = WADToOkItem(level);
    levelItem->setWireframe(true);
    scene->addItem(levelItem);

    // Position camera to view the level
    positionCameraForItem(camera, levelItem);

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
