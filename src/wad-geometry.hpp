#ifndef WAD_VIEWER_WAD_GEOMETRY_HPP
#define WAD_VIEWER_WAD_GEOMETRY_HPP

#include "../okinawa.cpp/src/item/item.hpp"
#include "./wad.hpp"
#include <vector>

class WADGeometry {
public:
  static std::vector<OkItem *> createLevelGeometry(const WAD::Level &level);

private:
  static float       centerX;
  static float       centerY;
  static const float SCALE;

  static void createWallSection(const WAD::Vertex &vertex1,
                                const WAD::Vertex &vertex2, float bottomHeight,
                                float topHeight, const WAD::Sidedef &sidedef,
                                std::vector<float>        &vertices,
                                std::vector<unsigned int> &indices);

  static void createSectorGeometry(const WAD::Level          &level,
                                   const WAD::Sector         &sector,
                                   const std::vector<int>    &sectorVertices,
                                   std::vector<float>        &vertices,
                                   std::vector<unsigned int> &indices,
                                   bool                       isFloor);

  static void
  createWallFace(const WAD::Vertex &vertex1, const WAD::Vertex &vertex2,
                 const WAD::Sector &sector1, const WAD::Sector &sector2,
                 const WAD::Sidedef &sidedef, std::vector<float> &vertices,
                 std::vector<unsigned int> &indices);
};

#endif  // WAD_VIEWER_WAD_GEOMETRY_HPP
