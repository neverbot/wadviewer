#include "gui.hpp"
#include "okinawa/core/camera.hpp"
#include "okinawa/core/gl_config.hpp"
#include "okinawa/handlers/textures.hpp"
#include "okinawa/input/input.hpp"
#include "okinawa/item/item.hpp"
#include "okinawa/item/texture.hpp"
#include "okinawa/utils/logger.hpp"
#include <vector>

GUI::GUI(OkCamera *camera) {
  // Initialize member variables
  currentTextureIndex = 0;

  // Create texture preview square
  std::vector<float> squareVerts = {
      // Position (XYZ)    // Texture coords (UV)
      -4.0f, 4.0f,  0.0f, 0.0f, 1.0f,  // Top left
      4.0f,  4.0f,  0.0f, 1.0f, 1.0f,  // Top right
      4.0f,  -4.0f, 0.0f, 1.0f, 0.0f,  // Bottom right
      -4.0f, -4.0f, 0.0f, 0.0f, 0.0f   // Bottom left
  };

  std::vector<unsigned int> squareIndices = {
      0, 1, 2,  // First triangle
      0, 2, 3   // Second triangle
  };

  texturePreview =
      new OkItem("texture_preview", squareVerts.data(),
                 static_cast<int>(squareVerts.size()), squareIndices.data(),
                 static_cast<int>(squareIndices.size()));

  texturePreview->setWireframe(false);
  texturePreview->setDrawMode(GL_TRIANGLES);
  texturePreview->attachTo(camera);

  // Position in front of the camera
  texturePreview->setPosition(10.0f, -7.0f, -30.0f);

  // Get available textures
  textureNames = OkTextureHandler::getInstance()->getTextureNames();

  // Apply first texture
  if (!textureNames.empty()) {
    OkTexture *texture =
        OkTextureHandler::getInstance()->getTexture(textureNames[0]);
    if (texture) {
      texturePreview->setTexture("texture_preview", texture);
    }
  }
}

GUI::~GUI() {
  delete texturePreview;
}

void GUI::step(const OkInputState &input) {
  if (input.action1) {  // Space bar
    nextTexture();
  }

  if (input.action2) {  // T key
    toggleVisibility();
  }
}

void GUI::toggleVisibility() {
  texturePreview->setVisible(!texturePreview->getVisible());
}

void GUI::nextTexture() {
  if (textureNames.empty())
    return;

  currentTextureIndex =
      (currentTextureIndex + 1) % static_cast<int>(textureNames.size());
  OkTexture *texture = OkTextureHandler::getInstance()->getTexture(
      textureNames[currentTextureIndex]);

  if (texture) {
    texturePreview->setTexture("texture_preview", texture);
    OkLogger::info("GUI :: Switched to texture: " +
                   textureNames[currentTextureIndex]);
  }
}
