#ifndef WAD_CONVERTER_HPP
#define WAD_CONVERTER_HPP

#include "../okinawa.cpp/src/item/item.hpp"
#include "./wad.hpp"
#include <vector>

class WADConverter {
public:
  WADConverter();
  ~WADConverter();

  std::vector<OkItem *> createLevelGeometry(const WAD::Level &level);

private:
  static float       centerX;
  static float       centerY;
  static const float SCALE;

  void createWallSection(const WAD::Vertex &vertex1, const WAD::Vertex &vertex2,
                         float bottomHeight, float topHeight,
                         const WAD::Sidedef        &sidedef,
                         std::vector<float>        &vertices,
                         std::vector<unsigned int> &indices);

  void createSectorGeometry(const WAD::Level &level, const WAD::Sector &sector,
                            const std::vector<size_t> &sectorVertices,
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

#endif
