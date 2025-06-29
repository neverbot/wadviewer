#ifndef WAD_VIEWER_WAD_GEOMETRY_HPP
#define WAD_VIEWER_WAD_GEOMETRY_HPP

#include "./wad.hpp"
#include "okinawa/item/group.hpp"
#include "okinawa/item/item.hpp"
#include <vector>

class WADGeometry {
public:
  static std::vector<OkItemGroup *>
  createLevelGeometry(const WAD::Level &level);

private:
  // Create individual sector group with all its geometry
  static OkItemGroup *createSectorGroup(const WAD::Level       &level,
                                        const WAD::Sector      &sector,
                                        int                     sectorIndex,
                                        const std::vector<int> &sectorVertices);

  // Create walls for a specific sector
  static std::vector<OkItem *>
  createSectorWalls(const WAD::Level &level, const WAD::Sector &sector,
                    int sectorIndex, const std::vector<int> &sectorVertices);
  // Create a wall section between two vertices with given heights
  // and sidedef information
  static void createWallSection(const WAD::Vertex &vertex1,
                                const WAD::Vertex &vertex2, float bottomHeight,
                                float topHeight, const WAD::Sidedef &sidedef,
                                std::vector<float>        &vertices,
                                std::vector<unsigned int> &indices);

  static void
  createWallFace(const WAD::Vertex &vertex1, const WAD::Vertex &vertex2,
                 const WAD::Sector &sector1, const WAD::Sector &sector2,
                 const WAD::Sidedef &sidedef, std::vector<float> &vertices,
                 std::vector<unsigned int> &indices);
};

#endif  // WAD_VIEWER_WAD_GEOMETRY_HPP
