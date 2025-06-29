#include "gui.hpp"
#include "okinawa/core/camera.hpp"
#include "okinawa/handlers/textures.hpp"
#include "okinawa/input/input.hpp"
#include "okinawa/item/item.hpp"
#include "okinawa/item/texture.hpp"
#include "okinawa/utils/logger.hpp"
#include <cstddef>
#include <string>
#include <vector>

GUI::GUI(OkCamera *camera) {
  // Initialize member variables
  this->camera        = camera;
  currentTextureIndex = 0;
  isInitialized       = false;

  // Get available textures
  textureNames = OkTextureHandler::getInstance()->getTextureNames();

  OkLogger::info("GUI", "Initialized GUI with " +
                            std::to_string(textureNames.size()) +
                            " textures available");

  // Create texture preview element structure (without OpenGL objects yet)
  GUIElement texturePreviewElement;
  texturePreviewElement.type        = "texture_preview";
  texturePreviewElement.visible     = true;
  texturePreviewElement.item        = nullptr;
  texturePreviewElement.initialized = false;
  guiElements.push_back(texturePreviewElement);
}

GUI::~GUI() {
  // Clean up all GUI elements
  for (size_t i = 0; i < guiElements.size(); i++) {
    if (guiElements[i].initialized && guiElements[i].item) {
      delete guiElements[i].item;
    }
  }
  guiElements.clear();
}

void GUI::step(const OkInputState &input) {
  // Initialize OpenGL objects on first step call (when context is ready)
  if (!isInitialized) {
    initializeGUIElements();
    isInitialized = true;
  }

  if (input.action1) {  // Space bar
    nextTexture();
  }

  if (input.action2) {  // T key
    toggleVisibility();
  }
}

void GUI::toggleVisibility() {
  // Toggle visibility of texture preview element
  if (!guiElements.empty() && guiElements[0].initialized &&
      guiElements[0].item) {
    bool currentVisibility = guiElements[0].item->getVisible();
    guiElements[0].item->setVisible(!currentVisibility);
    guiElements[0].visible = !currentVisibility;
  }
}

void GUI::nextTexture() {
  if (textureNames.empty() || guiElements.empty() ||
      !guiElements[0].initialized || !guiElements[0].item)
    return;

  currentTextureIndex =
      (currentTextureIndex + 1) % static_cast<int>(textureNames.size());
  OkTexture *texture = OkTextureHandler::getInstance()->getTexture(
      textureNames[currentTextureIndex]);

  if (texture) {
    guiElements[0].item->setTexture("texture_preview", texture);
    updateTexturePreviewSize(0);
    OkLogger::info("GUI",
                   "Switched to texture: " + textureNames[currentTextureIndex]);
  }
}

// Initialize GUI elements when OpenGL context is ready
void GUI::initializeGUIElements() {
  // Initialize texture preview element
  if (!guiElements.empty() && !guiElements[0].initialized) {
    createTexturePreview(0);
    guiElements[0].initialized = true;

    // Apply first texture
    if (!textureNames.empty()) {
      OkTexture *texture =
          OkTextureHandler::getInstance()->getTexture(textureNames[0]);
      if (texture) {
        guiElements[0].item->setTexture("texture_preview", texture);
        updateTexturePreviewSize(0);
        OkLogger::info("GUI", "Initialized with texture: " + textureNames[0]);
      }
    }
  }
}

// Helper method to create a polygon with specific dimensions
OkItem *GUI::createPolygonWithSize(const std::string &name, float width,
                                   float height) {
  // Create vertices for a rectangle with the specified dimensions
  // Center the rectangle around origin for proper positioning
  float halfWidth  = width / 2.0f;
  float halfHeight = height / 2.0f;

  std::vector<float> vertices = {
      // Position (XYZ)        // Texture coords (UV)
      -halfWidth, halfHeight,  0.0f, 0.0f, 1.0f,  // Top left
      halfWidth,  halfHeight,  0.0f, 1.0f, 1.0f,  // Top right
      halfWidth,  -halfHeight, 0.0f, 1.0f, 0.0f,  // Bottom right
      -halfWidth, -halfHeight, 0.0f, 0.0f, 0.0f   // Bottom left
  };

  std::vector<unsigned int> indices = {
      0, 1, 2,  // First triangle
      0, 2, 3   // Second triangle
  };

  OkItem *item =
      new OkItem(name, vertices.data(), static_cast<int>(vertices.size()),
                 indices.data(), static_cast<int>(indices.size()));

  // 2Attach to camera after object is fully constructed
  item->attachTo(camera);

  return item;
}

// Helper method to create the texture preview element
void GUI::createTexturePreview(int elementIndex) {
  if (elementIndex >= static_cast<int>(guiElements.size())) {
    return;
  }

  // Create initial square polygon (will be resized when texture is applied)
  float initialSize = 4.0f;
  guiElements[elementIndex].item =
      createPolygonWithSize("texture_preview", initialSize, initialSize);

  // Position in front of the camera
  guiElements[elementIndex].item->setPosition(10.0f, -7.0f, -30.0f);
}

// Helper method to update texture preview size based on texture dimensions
void GUI::updateTexturePreviewSize(int elementIndex) {
  if (elementIndex >= static_cast<int>(guiElements.size()) ||
      elementIndex >= static_cast<int>(textureNames.size()) ||
      !guiElements[elementIndex].item) {
    return;
  }

  OkTexture *texture = OkTextureHandler::getInstance()->getTexture(
      textureNames[currentTextureIndex]);

  if (!texture) {
    return;
  }

  // Get texture dimensions
  int textureWidth  = texture->getWidth();
  int textureHeight = texture->getHeight();

  // Calculate display scale to keep textures at reasonable size
  // Use a base scale that makes a 64x64 texture appear as 4x4 units
  float baseScale     = 4.0f / 64.0f;
  float displayWidth  = static_cast<float>(textureWidth) * baseScale;
  float displayHeight = static_cast<float>(textureHeight) * baseScale;

  // Create new vertex data for the resized polygon
  float halfWidth  = displayWidth / 2.0f;
  float halfHeight = displayHeight / 2.0f;

  std::vector<float> newVertices = {
      // Position (XYZ)        // Texture coords (UV)
      -halfWidth, halfHeight,  0.0f, 0.0f, 1.0f,  // Top left
      halfWidth,  halfHeight,  0.0f, 1.0f, 1.0f,  // Top right
      halfWidth,  -halfHeight, 0.0f, 1.0f, 0.0f,  // Bottom right
      -halfWidth, -halfHeight, 0.0f, 0.0f, 0.0f   // Bottom left
  };

  // Update the vertex data safely without recreating the item
  guiElements[elementIndex].item->updateVertexData(
      newVertices.data(), static_cast<long>(newVertices.size()));
}
