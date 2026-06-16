#ifndef WAD_VIEWER_WAD_GEOMETRY_HPP
#define WAD_VIEWER_WAD_GEOMETRY_HPP

#include "./wad.hpp"
#include "okinawa/item/group.hpp"
#include "okinawa/item/item.hpp"
#include <cstdint>
#include <string>
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
  // Create a wall quad with DOOM-correct texture mapping: real texture
  // dimensions (looked up by name) + linedef pegging.
  // sectionType: 0 = upper, 1 = lower, 2 = middle (one- or two-sided).
  // frontCeil is the front sector's ceiling height (used by lower-unpegged).
  static void createWallSection(const WAD::Vertex &vertex1,
                                const WAD::Vertex &vertex2, float wallBottom,
                                float wallTop, const std::string &textureName,
                                int sectionType, float frontCeil,
                                uint16_t flags, const WAD::Sidedef &sidedef,
                                std::vector<float>        &vertices,
                                std::vector<unsigned int> &indices);
};

#endif  // WAD_VIEWER_WAD_GEOMETRY_HPP
