#ifndef WAD_VIEWER_WAD_CONVERTER_HPP
#define WAD_VIEWER_WAD_CONVERTER_HPP

#include "../okinawa.cpp/src/item/item.hpp"
#include "./wad.hpp"
#include <vector>

class WADConverter {
public:
  WADConverter();
  ~WADConverter();

  std::vector<OkItem *> createLevelGeometry(const WAD::Level &level);
  OkPoint              *getPlayerStartPosition(const WAD::Level &level);

private:
  static float       centerX;
  static float       centerY;
  static const float SCALE;

  // Helper method to check if a point is inside a sector boundary line
  static bool pointInSector(int16_t px, int16_t py, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    // Check if point is on the right side of the line
    // Using the sign of cross product: (x2-x1)(py-y1) - (y2-y1)(px-x1)
    return ((x2 - x1) * (py - y1) - (y2 - y1) * (px - x1)) > 0;
  }

  void createWallSection(const WAD::Vertex &vertex1, const WAD::Vertex &vertex2,
                         float bottomHeight, float topHeight,
                         const WAD::Sidedef        &sidedef,
                         std::vector<float>        &vertices,
                         std::vector<unsigned int> &indices);

  void createSectorGeometry(const WAD::Level &level, const WAD::Sector &sector,
                            const std::vector<int>    &sectorVertices,
                            std::vector<float>        &vertices,
                            std::vector<unsigned int> &indices, bool isFloor);

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

  void createFlatTexture(const std::string             &flatName,
                         const WAD::FlatData           &flatData,
                         const std::vector<WAD::Color> &palette);
};

#endif  // WAD_VIEWER_WAD_CONVERTER_HPP
