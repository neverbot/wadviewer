#include "gui.hpp"
#include "okinawa/core/camera.hpp"
#include "okinawa/handlers/textures.hpp"
#include "okinawa/input/input.hpp"
#include "okinawa/item/item.hpp"
#include "okinawa/item/texture.hpp"
#include "okinawa/utils/logger.hpp"
#include <vector>

GUI::GUI(OkCamera *camera) {
  // Initialize member variables
  this->camera        = camera;
  currentTextureIndex = 0;
  isInitialized       = false;

  OkLogger::info("GUI", "Initializing GUI system...");

  // Get available textures
  textureNames = OkTextureHandler::getInstance()->getTextureNames();

  OkLogger::info("GUI", "Found " + std::to_string(textureNames.size()) +
                            " textures available");

  // Create texture preview element structure (without OpenGL objects yet)
  GUIElement texturePreviewElement;
  texturePreviewElement.type        = "texture_preview";
  texturePreviewElement.visible     = true;
  texturePreviewElement.item        = nullptr;
  texturePreviewElement.initialized = false;
  guiElements.push_back(texturePreviewElement);

  OkLogger::info("GUI", "GUI structure setup complete - OpenGL objects will be "
                        "created on first step");
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
    OkLogger::info("GUI",
                   "Toggled texture preview visibility to " +
                       std::string(!currentVisibility ? "visible" : "hidden"));
  }
}

void GUI::nextTexture() {
  if (textureNames.empty() || guiElements.empty() ||
      !guiElements[0].initialized || !guiElements[0].item)
    return;

  OkLogger::info("GUI", "Switching to next texture...");

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
  OkLogger::info("GUI", "Initializing OpenGL GUI elements...");

  // Initialize texture preview element
  if (!guiElements.empty() && !guiElements[0].initialized) {
    OkLogger::info("GUI", "Creating texture preview...");
    createTexturePreview(0);
    guiElements[0].initialized = true;

    OkLogger::info("GUI", "Applying initial texture...");
    // Apply first texture
    if (!textureNames.empty()) {
      OkTexture *texture =
          OkTextureHandler::getInstance()->getTexture(textureNames[0]);
      if (texture) {
        OkLogger::info("GUI", "Setting texture on item...");
        guiElements[0].item->setTexture("texture_preview", texture);
        OkLogger::info("GUI", "Updating texture preview size...");
        updateTexturePreviewSize(0);
        OkLogger::info("GUI", "Applied initial texture: " + textureNames[0]);
      }
    }
  }

  OkLogger::info("GUI", "GUI initialization complete");
}

// Helper method to create a polygon with specific dimensions
OkItem *GUI::createPolygonWithSize(const std::string &name, float width,
                                   float height) {
  OkLogger::info("GUI", "Creating polygon with size " + std::to_string(width) +
                            "x" + std::to_string(height));

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

  OkLogger::info("GUI", "About to create OkItem...");

  OkItem *item =
      new OkItem(name, vertices.data(), static_cast<int>(vertices.size()),
                 indices.data(), static_cast<int>(indices.size()));

  OkLogger::info("GUI", "OkItem created successfully");

  // Set properties before attaching to avoid virtual function issues
  item->setWireframe(false);
  item->setDrawMode(GL_TRIANGLES);

  OkLogger::info("GUI", "About to attach to camera...");

  // Attach to camera after object is fully constructed
  item->attachTo(camera);

  OkLogger::info("GUI", "Polygon creation complete");
  return item;
}

// Helper method to create the texture preview element
void GUI::createTexturePreview(int elementIndex) {
  if (elementIndex >= static_cast<int>(guiElements.size())) {
    return;
  }

  OkLogger::info("GUI", "Creating texture preview element...");

  // Create initial square polygon (will be resized when texture is applied)
  float initialSize = 4.0f;
  guiElements[elementIndex].item =
      createPolygonWithSize("texture_preview", initialSize, initialSize);

  OkLogger::info("GUI", "Setting position...");
  // Position in front of the camera
  guiElements[elementIndex].item->setPosition(10.0f, -7.0f, -30.0f);

  OkLogger::info("GUI", "Texture preview element created successfully");
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

  OkLogger::info("GUI_SIZE",
                 "Resizing texture preview for " +
                     textureNames[currentTextureIndex] +
                     " dimensions: " + std::to_string(textureWidth) + "x" +
                     std::to_string(textureHeight) +
                     " -> display size: " + std::to_string(displayWidth) + "x" +
                     std::to_string(displayHeight));

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

  OkLogger::info("GUI_SIZE", "Texture preview resized successfully");
}
