#ifndef WAD_VIEWER_GUI_HPP
#define WAD_VIEWER_GUI_HPP

#include "okinawa/core/camera.hpp"
#include "okinawa/input/input.hpp"
#include "okinawa/item/item.hpp"
#include <string>
#include <vector>

class GUI {
private:
  // GUI elements structure for future expansion
  struct GUIElement {
    OkItem     *item;
    std::string type;
    bool        visible;
    bool        initialized;
  };

  std::vector<GUIElement>  guiElements;
  OkCamera                *camera;
  std::vector<std::string> textureNames;
  int                      currentTextureIndex;
  bool                     isInitialized;

  // Helper methods
  void    initializeGUIElements();
  void    createTexturePreview(int elementIndex);
  void    updateTexturePreviewSize(int elementIndex);
  OkItem *createPolygonWithSize(const std::string &name, float width,
                                float height);

public:
  GUI(OkCamera *camera);
  ~GUI();

  void step(const OkInputState &input);
  void toggleVisibility();
  void nextTexture();
};

#endif
