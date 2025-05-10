#ifndef WAD_RENDERER_HPP
#define WAD_RENDERER_HPP

#include "../okinawa.cpp/src/handlers/textures.hpp"
#include "../okinawa.cpp/src/item/item.hpp"
#include "./wad.hpp"
#include <string>

class WADRenderer {
public:
  WADRenderer();
  ~WADRenderer();

  std::vector<OkItem *> createLevelGeometry(const WAD::Level &level);

private:
  void createWallFace(const WAD::Vertex &vertex1, const WAD::Vertex &vertex2,
                      const WAD::Sector &sector1, const WAD::Sector &sector2,
                      const WAD::Sidedef &sidedef, std::vector<float> &vertices,
                      std::vector<unsigned int> &indices);

  void createTextureFromDef(const WAD::TextureDef             &texDef,
                            const std::vector<WAD::PatchData> &patches,
                            const std::vector<WAD::Color>     &palette);
  void compositePatch(std::vector<unsigned char> &textureData, int texWidth,
                      int texHeight, const WAD::PatchData &patch, int originX,
                      int originY, const std::vector<WAD::Color> &palette);

  static float       centerX;
  static float       centerY;
  static const float SCALE;
};

#endif
