#include "../okinawa.cpp/src/core/camera.hpp"
#include "../okinawa.cpp/src/core/core.hpp"
#include "../okinawa.cpp/src/handlers/scenes.hpp"
#include "../okinawa.cpp/src/importers/wavefront.hpp"
#include "../okinawa.cpp/src/input/input.hpp"
#include "../okinawa.cpp/src/scene/scene.hpp"
#include "../okinawa.cpp/src/utils/logger.hpp"
#include <cmath>

// Constants for movement and rotation
#define ROTATION_SPEED 0.5f // 0.5 rotations per second
#define ORBIT_SPEED 0.2f    // 0.05 orbits per second
#define ORBIT_RADIUS 1.0f   // Distance from center

OkItem *item, *item2;
// OkItem *model, *model2;

// Test movement and rotation for the model
static float angle = 0.0f;
static float totalTime = 0.0f;

void stepCallback(float deltaTime) {
  float timeInSeconds = deltaTime / 1000.0f;

  // ----------------------------------------
  // temp tests to move items

  // // Only update model if it exists
  // if (model) {
  //   // Accumulate time for orbital movement
  //   totalTime += timeInSeconds;

  //   // Update rotation angle
  //   angle = ROTATION_SPEED * timeInSeconds * 2.0f * M_PI;

  //   // Rotate the model around Y axis
  //   model->rotate(0.0f, angle, 0.0f);

  //   // Calculate position for circular movement using accumulated time
  //   float orbitAngle = ORBIT_SPEED * totalTime * 2.0f * M_PI;
  //   float x = ORBIT_RADIUS * cosf(orbitAngle);
  //   float z = ORBIT_RADIUS * sinf(orbitAngle);
  //   model->setPosition(x, 0.0f, z);
  // }

  // end tmp tests
  // ----------------------------------------
}

void drawCallback(float deltaTime) {
  // Do whatever is needed
  // heistAvatarDraw(avatar);

  // OkSceneHandler *sceneHandler = OkCore::getSceneHandler();
  // OkScene        *currentScene = sceneHandler->getCurrentScene();

  // if (currentScene) {
  //   OkLogger::info("Drawing scene with " +
  //                  std::to_string(currentScene->getItemCount()) + " items");
  // }
}

int main() {
  OkLogger::info("Game :: Starting up...");
  OkCore::initialize();

  // Create main scene
  OkScene *scene = new OkScene("MainScene");

  // Set up camera
  OkCamera *camera = OkCore::getCamera();
  glm::vec3 cameraPosition(0.0f, 2.0f, -5.0f);
  glm::vec3 targetPosition(0.0f, 0.0f, 0.0f);
  glm::vec3 upVector(0.0f, 1.0f, 0.0f);

  camera->setView(glm::lookAt(cameraPosition, targetPosition, upVector));
  // Add debug output
  OkLogger::info("Camera position: " + std::to_string(cameraPosition.z));

  // Create test items
  std::vector<float> vertices = {
      // Positions         // Texture coords
      0.5f,  0.5f,  0.0f, 1.0f, 1.0f, // top right
      0.5f,  -0.5f, 0.0f, 1.0f, 0.0f, // bottom right
      -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, // bottom left
      -0.5f, 0.5f,  0.0f, 0.0f, 1.0f  // top left
  };

  std::vector<unsigned int> indices = {
      0, 1, 3, // first Triangle
      1, 2, 3  // second Triangle
  };

  item = new OkItem("square", vertices.data(), vertices.size(), indices.data(),
                    indices.size());
  item->setWireframe(true);

  item2 = new OkItem("square2", vertices.data(), vertices.size(),
                     indices.data(), indices.size());
  item2->setWireframe(true);

  // Load 3D models
  // model = OkWavefrontImporter::importFile("./assets/test/cube-xyz-uv.obj");
  // if (!model) {
  //   OkLogger::error("Core :: Failed to load model");
  //   return 0;
  // }

  // model2 = OkWavefrontImporter::importFile("./assets/test/cube-xyz-uv.obj");
  // if (!model2) {
  //   OkLogger::error("Core :: Failed to load model");
  //   return 0;
  // }

  // model2->setScaling(0.5f, 0.5f, 0.5f);
  // model->attach(model2);
  // model2->setPosition(1.0f, 0.0f, 0.0f);
  // scene->addItem(model);

  // Create floor
  std::vector<float> floorVertices = {
      // Positions        // Texture coords
      5.0f,  0.0f, 5.0f,  1.0f, 1.0f, // top right
      5.0f,  0.0f, -5.0f, 1.0f, 0.0f, // bottom right
      -5.0f, 0.0f, -5.0f, 0.0f, 0.0f, // bottom left
      -5.0f, 0.0f, 5.0f,  0.0f, 1.0f  // top left
  };

  std::vector<unsigned int> floorIndices = {
      // note that we start from 0!
      0, 1, 3, // first Triangle
      1, 2, 3  // second Triangle
  };

  OkItem *floor =
      new OkItem("floor", floorVertices.data(), floorVertices.size(),
                 floorIndices.data(), floorIndices.size());
  if (!floor) {
    OkLogger::error("Game :: Failed to create floor");
    return 0;
  }

  floor->setTexture("./assets/tile-texture.png");
  floor->setWireframe(true);

  scene->addItem(item);
  scene->addItem(item2);
  scene->addItem(floor);

  OkLogger::info("Scene :: Item count: " +
                 std::to_string(scene->getItemCount()));

  // Position items in view
  item->setPosition(-2.0f, 0.0f, 0.0f);  // Left square
  item2->setPosition(2.0f, 0.0f, 0.0f);  // Right square
  floor->setPosition(0.0f, -0.5f, 0.0f); // Floor slightly below

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
