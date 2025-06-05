#ifndef WAD_VIEWER_GUI_HPP
#define WAD_VIEWER_GUI_HPP

#include "../okinawa.cpp/src/core/camera.hpp"
#include "../okinawa.cpp/src/input/input.hpp"
#include "../okinawa.cpp/src/item/item.hpp"
#include <string>
#include <vector>

class GUI {
private:
  OkItem                  *texturePreview;
  std::vector<std::string> textureNames;
  int                      currentTextureIndex;

  void createTexturePreview();

public:
  GUI(OkCamera *camera);
  ~GUI();

  void step(const OkInputState &input);
  void toggleVisibility();
  void nextTexture();
};

#endif
