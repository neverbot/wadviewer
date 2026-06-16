#include "wad-geometry.hpp"
#include "okinawa/config/config.hpp"
#include "okinawa/handlers/textures.hpp"
#include "okinawa/item/group.hpp"
#include "okinawa/item/item.hpp"
#include "okinawa/item/texture.hpp"
#include "okinawa/utils/logger.hpp"
#include "okinawa/utils/strings.hpp"
#include "wad-generate.hpp"
#include "wad-textures.hpp"
#include "wad.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <string>
#include <vector>

void WADGeometry::createWallSection(const WAD::Vertex &vertex1,
                                    const WAD::Vertex &vertex2, float wallBottom,
                                    float wallTop, const std::string &textureName,
                                    int sectionType, float frontCeil,
                                    uint16_t flags, const WAD::Sidedef &sidedef,
                                    std::vector<float>        &vertices,
                                    std::vector<unsigned int> &indices) {
  if (wallTop - wallBottom <= 0.0f) {
    return;
  }

  float levelCenterX = OkConfig::getFloat("level.center.x");
  float levelCenterY = OkConfig::getFloat("level.center.y");

  float x1 = static_cast<float>(vertex1.x) - levelCenterX;
  float z1 = static_cast<float>(vertex1.y) - levelCenterY;
  float x2 = static_cast<float>(vertex2.x) - levelCenterX;
  float z2 = static_cast<float>(vertex2.y) - levelCenterY;

  // Real dimensions of the texture actually applied to this section (fall back
  // to the common DOOM size only if it can't be resolved).
  float      textureWidth  = 64.0f;
  float      textureHeight = 128.0f;
  OkTexture *texture = OkTextureHandler::getInstance()->getTexture(textureName);
  if (texture != nullptr) {
    textureWidth  = static_cast<float>(texture->getWidth());
    textureHeight = static_cast<float>(texture->getHeight());
  }

  float wallLength =
      sqrtf(powf(static_cast<float>(vertex2.x - vertex1.x), 2.0f) +
            powf(static_cast<float>(vertex2.y - vertex1.y), 2.0f));

  // Horizontal U: x_offset plus distance along the wall, in texture widths.
  float xOffset = static_cast<float>(sidedef.x_offset);
  float u1      = xOffset / textureWidth;
  float u2      = u1 + wallLength / textureWidth;

  // Vertical: world height of the texture's top row (row 0) following DOOM
  // pegging (linedef ML_DONTPEGTOP / ML_DONTPEGBOTTOM).
  const uint16_t ML_DONTPEGTOP    = 0x0008;
  const uint16_t ML_DONTPEGBOTTOM = 0x0010;
  bool           dontPegTop       = (flags & ML_DONTPEGTOP) != 0;
  bool           dontPegBottom    = (flags & ML_DONTPEGBOTTOM) != 0;

  float texTopWorld;
  if (sectionType == 0) {
    // Upper: bottom-pegged by default; DONTPEGTOP pegs it to the top.
    texTopWorld = dontPegTop ? wallTop : (wallBottom + textureHeight);
  } else if (sectionType == 1) {
    // Lower: top-of-section by default; DONTPEGBOTTOM pegs it to the ceiling.
    texTopWorld = dontPegBottom ? frontCeil : wallTop;
  } else {
    // Middle: top-pegged by default; DONTPEGBOTTOM pegs it to the bottom.
    texTopWorld = dontPegBottom ? (wallBottom + textureHeight) : wallTop;
  }
  // Sidedef y_offset (DOOM rowoffset) is added to the texture anchor, like
  // r_segs.c does (rw_*texturemid += rowoffset): a positive offset raises the
  // texture's top-row world height, scrolling the texture upward on the wall.
  texTopWorld += static_cast<float>(sidedef.y_offset);

  // OpenGL V (engine convention: smaller V = texture top). May exceed [0,1];
  // textures use GL_REPEAT, so they tile.
  float vTop    = (texTopWorld - wallTop) / textureHeight;
  float vBottom = (texTopWorld - wallBottom) / textureHeight;

  // Bottom left
  vertices.push_back(x1);
  vertices.push_back(wallBottom);
  vertices.push_back(-z1);
  vertices.push_back(u1);
  vertices.push_back(vBottom);
  // Top left
  vertices.push_back(x1);
  vertices.push_back(wallTop);
  vertices.push_back(-z1);
  vertices.push_back(u1);
  vertices.push_back(vTop);
  // Bottom right
  vertices.push_back(x2);
  vertices.push_back(wallBottom);
  vertices.push_back(-z2);
  vertices.push_back(u2);
  vertices.push_back(vBottom);
  // Top right
  vertices.push_back(x2);
  vertices.push_back(wallTop);
  vertices.push_back(-z2);
  vertices.push_back(u2);
  vertices.push_back(vTop);

  unsigned int baseIndex = static_cast<unsigned int>(vertices.size() / 5 - 4);
  indices.push_back(baseIndex);
  indices.push_back(baseIndex + 1);
  indices.push_back(baseIndex + 2);
  indices.push_back(baseIndex + 1);
  indices.push_back(baseIndex + 3);
  indices.push_back(baseIndex + 2);
}

/**
 * @brief Creates all the geometry for a level organized by sectors.
 * @param level The level to create geometry for.
 * @return A vector of OkItemGroup pointers representing sectors with their
 * geometry.
 */
std::vector<OkItemGroup *>
WADGeometry::createLevelGeometry(const WAD::Level &level) {
  std::vector<OkItemGroup *> sectorGroups;

  // Calculate level bounds and set center
  float minX = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float minY = std::numeric_limits<float>::max();
  float maxY = std::numeric_limits<float>::lowest();

  for (size_t i = 0; i < level.vertices.size(); i++) {
    const WAD::Vertex &vertex = level.vertices[i];
    minX                      = std::min(minX, static_cast<float>(vertex.x));
    maxX                      = std::max(maxX, static_cast<float>(vertex.x));
    minY                      = std::min(minY, static_cast<float>(vertex.y));
    maxY                      = std::max(maxY, static_cast<float>(vertex.y));
  }

  float levelCenterX = (minX + maxX) / 2.0f;
  float levelCenterY = (minY + maxY) / 2.0f;

  // Log level bounds and center for debugging
  OkLogger::info("LEVEL_DEBUG", "Level bounds - X: [" + std::to_string(minX) +
                                    ", " + std::to_string(maxX) + "], Y: [" +
                                    std::to_string(minY) + ", " +
                                    std::to_string(maxY) + "]");
  OkLogger::info("LEVEL_DEBUG",
                 "Level center - X: " + std::to_string(levelCenterX) +
                     ", Y: " + std::to_string(levelCenterY));

  // Store level center coordinates in global config for use by other modules
  OkConfig::setFloat("level.center.x", levelCenterX);
  OkConfig::setFloat("level.center.y", levelCenterY);

  // First, create all flat (floor/ceiling) textures
  for (size_t i = 0; i < level.flats.size(); i++) {
    const WAD::FlatData &flat = level.flats[i];
    WADTextures::createFlatTexture(flat.name, flat, level.palette);
  }

  // Then load all wall textures we'll need
  for (size_t i = 0; i < level.sidedefs.size(); i++) {
    const WAD::Sidedef &sidedef = level.sidedefs[i];
    std::string upperTex = OkStrings::trimFixedString(sidedef.upper_texture, 8);
    std::string middleTex =
        OkStrings::trimFixedString(sidedef.middle_texture, 8);
    std::string lowerTex = OkStrings::trimFixedString(sidedef.lower_texture, 8);

    // Also get floor/ceiling textures
    if (sidedef.sector < level.sectors.size()) {
      const WAD::Sector &sector = level.sectors[sidedef.sector];
      std::string        floorTex =
          OkStrings::trimFixedString(sector.floor_texture, 8);
      std::string ceilTex =
          OkStrings::trimFixedString(sector.ceiling_texture, 8);

      // Load all needed textures
      for (int j = 0; j < (int)level.texture_defs.size(); j++) {
        const WAD::TextureDef &texDef = level.texture_defs[j];
        std::string texName = OkStrings::trimFixedString(texDef.name, 8);
        if (!texName.empty() && (texName == upperTex || texName == middleTex ||
                                 texName == lowerTex || texName == floorTex ||
                                 texName == ceilTex)) {
          WADTextures::createTextureFromDef(texDef, level.patches,
                                            level.palette);
        }
      }
    }
  }

  // Create geometry for all sectors in the level
  for (size_t i = 0; i < level.sectors.size(); i++) {
    const WAD::Sector &sector      = level.sectors[i];
    int                sectorIndex = static_cast<int>(i);

    // Generate vertices for this sector
    std::vector<int> sectorVertices =
        WADGenerate::generateSectorVertices(level, sectorIndex);

    // Create the sector group
    OkItemGroup *sectorGroup =
        createSectorGroup(level, sector, sectorIndex, sectorVertices);

    if (sectorGroup) {
      sectorGroups.push_back(sectorGroup);
    }

    OkLogger::info("SECTOR_DEBUG", "Created sector " + std::to_string(i));
  }

  return sectorGroups;
}


OkItemGroup *
WADGeometry::createSectorGroup(const WAD::Level  &level,
                               const WAD::Sector &sector, int sectorIndex,
                               const std::vector<int> &sectorVertices) {
  // Log sector dimensions for debugging
  int sectorHeight = sector.ceiling_height - sector.floor_height;
  OkLogger::info("SECTOR_DEBUG",
                 "Creating sector " + std::to_string(sectorIndex) +
                     " - Floor: " + std::to_string(sector.floor_height) +
                     ", Ceiling: " + std::to_string(sector.ceiling_height) +
                     ", Height: " + std::to_string(sectorHeight));

  // Create the sector group
  std::string  groupName   = "sector_" + std::to_string(sectorIndex);
  OkItemGroup *sectorGroup = new OkItemGroup(groupName);

  // Calculate sector center for group positioning using normalized coordinates
  float centerX = 0.0f, centerY = 0.0f, centerZ = 0.0f;
  if (!sectorVertices.empty()) {
    // Get level center coordinates from global config for normalization
    float levelCenterX = OkConfig::getFloat("level.center.x");
    float levelCenterY = OkConfig::getFloat("level.center.y");

    // Calculate X and Z center from sector vertices using normalized
    // coordinates DOOM coordinate system: X stays X, Y becomes Z in 3D space
    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();

    for (size_t i = 0; i < sectorVertices.size(); i++) {
      if (sectorVertices[i] < static_cast<int>(level.vertices.size())) {
        const WAD::Vertex &vertex = level.vertices[sectorVertices[i]];
        // Apply the same coordinate transformation as walls/floors/ceilings
        // DOOM Y becomes OpenGL Z, DOOM X becomes OpenGL X
        // Z coordinate is negated to match geometry coordinate system
        float x = (static_cast<float>(vertex.x) - levelCenterX);
        float z = -((static_cast<float>(vertex.y) - levelCenterY));
        minX    = std::min(minX, x);
        maxX    = std::max(maxX, x);
        minZ    = std::min(minZ, z);
        maxZ    = std::max(maxZ, z);
      }
    }

    centerX = (minX + maxX) * 0.5f;
    centerZ = (minZ + maxZ) * 0.5f;
    // Y center is middle between floor and ceiling (height in 3D)
    centerY = (static_cast<float>(sector.floor_height) +
               static_cast<float>(sector.ceiling_height)) *
              0.5f;
  }

  // Position the group at the sector center
  sectorGroup->setPosition(centerX, centerY, centerZ);

  // Add sector tag
  std::vector<std::string> sectorTags;
  sectorTags.push_back("sector");
  sectorTags.push_back("geometry");

  // Create floor item
  OkItem *floorItem = WADGenerate::generateSectorFloor(
      level, sector, sectorVertices, sectorIndex);
  if (floorItem) {
    sectorGroup->addItem(floorItem, sectorTags);
    std::vector<std::string> floorTags = sectorTags;
    floorTags.push_back("floor");
    sectorGroup->setItemTags(floorItem, floorTags);
  }

  // Create ceiling item
  OkItem *ceilingItem = WADGenerate::generateSectorCeiling(
      level, sector, sectorVertices, sectorIndex);
  if (ceilingItem) {
    sectorGroup->addItem(ceilingItem, sectorTags);
    std::vector<std::string> ceilingTags = sectorTags;
    ceilingTags.push_back("ceiling");
    sectorGroup->setItemTags(ceilingItem, ceilingTags);
  }

  // Create wall items for this sector
  std::vector<OkItem *> wallItems =
      createSectorWalls(level, sector, sectorIndex, sectorVertices);
  for (size_t i = 0; i < wallItems.size(); i++) {
    sectorGroup->addItem(wallItems[i], sectorTags);
    std::vector<std::string> wallTags = sectorTags;
    wallTags.push_back("wall");
    sectorGroup->setItemTags(wallItems[i], wallTags);
  }

  // Only return the group if it has items
  if (sectorGroup->getItemCount() > 0) {
    return sectorGroup;
  }

  delete sectorGroup;
  return nullptr;
}

std::vector<OkItem *>
WADGeometry::createSectorWalls(const WAD::Level  &level,
                               const WAD::Sector &sector, int sectorIndex,
                               const std::vector<int> &sectorVertices) {
  std::vector<OkItem *> wallItems;

  // Structure to hold geometry for each texture
  struct GeometryGroup {
    std::vector<float>        vertices;
    std::vector<unsigned int> indices;
    std::string               textureName;
  };
  std::map<std::string, GeometryGroup> geometryGroups;

  // Find all linedefs that reference this sector
  for (size_t i = 0; i < level.linedefs.size(); i++) {
    const WAD::Linedef &linedef = level.linedefs[i];

    // Skip invalid vertex indices
    if (linedef.start_vertex >= level.vertices.size() ||
        linedef.end_vertex >= level.vertices.size()) {
      continue;
    }

    const WAD::Vertex &v1 = level.vertices[linedef.start_vertex];
    const WAD::Vertex &v2 = level.vertices[linedef.end_vertex];

    // Check if this linedef affects our sector
    bool isRightSector = false;
    bool isLeftSector  = false;

    if (linedef.right_sidedef != 0xFFFF &&
        linedef.right_sidedef < level.sidedefs.size()) {
      const WAD::Sidedef &rightSide = level.sidedefs[linedef.right_sidedef];
      if (rightSide.sector == sectorIndex) {
        isRightSector = true;
      }
    }

    if (linedef.left_sidedef != 0xFFFF &&
        linedef.left_sidedef < level.sidedefs.size()) {
      const WAD::Sidedef &leftSide = level.sidedefs[linedef.left_sidedef];
      if (leftSide.sector == sectorIndex) {
        isLeftSector = true;
      }
    }

    // Process walls for this sector
    if (isRightSector) {
      const WAD::Sidedef &rightSide = level.sidedefs[linedef.right_sidedef];

      // Handle two-sided linedef case
      if (linedef.left_sidedef != 0xFFFF &&
          linedef.left_sidedef < level.sidedefs.size()) {
        const WAD::Sidedef &leftSide = level.sidedefs[linedef.left_sidedef];

        if (leftSide.sector < level.sectors.size()) {
          const WAD::Sector &sector1 = level.sectors[leftSide.sector];
          const WAD::Sector &sector2 = level.sectors[rightSide.sector];

          // Upper wall: DOOM draws this side's upper texture when THIS sector's
          // ceiling is higher than the neighbour's; it fills the gap from the
          // neighbour ceiling up to this ceiling.
          if (sector2.ceiling_height > sector1.ceiling_height) {
            std::string textureName =
                OkStrings::trimFixedString(rightSide.upper_texture, 8);
            if (!textureName.empty() && textureName != "-") {
              GeometryGroup &group = geometryGroups[textureName];
              group.textureName    = textureName;
              createWallSection(v1, v2, sector1.ceiling_height,
                                sector2.ceiling_height, textureName, 0,
                                sector2.ceiling_height, linedef.flags, rightSide,
                                group.vertices, group.indices);
            }
          }

          // Lower wall (step riser): drawn when THIS sector's floor is lower
          // than the neighbour's, filling from this floor up to the neighbour
          // floor with this side's lower texture.
          if (sector2.floor_height < sector1.floor_height) {
            std::string textureName =
                OkStrings::trimFixedString(rightSide.lower_texture, 8);
            if (!textureName.empty() && textureName != "-") {
              GeometryGroup &group = geometryGroups[textureName];
              group.textureName    = textureName;
              createWallSection(v1, v2, sector2.floor_height,
                                sector1.floor_height, textureName, 1,
                                sector2.ceiling_height, linedef.flags, rightSide,
                                group.vertices, group.indices);
            }
          }

          // Create middle wall in gaps
          std::string middleTexName =
              OkStrings::trimFixedString(rightSide.middle_texture, 8);
          if (!middleTexName.empty() && middleTexName != "-") {
            float upperWallBottom = sector2.ceiling_height;
            float lowerWallTop    = sector2.floor_height;

            if ((sector1.ceiling_height == sector2.ceiling_height &&
                 sector1.floor_height == sector2.floor_height) ||
                (upperWallBottom > lowerWallTop)) {

              float bottom =
                  std::max(sector1.floor_height, sector2.floor_height);
              float top =
                  std::min(sector1.ceiling_height, sector2.ceiling_height);

              if (top > bottom) {
                GeometryGroup &group = geometryGroups[middleTexName];
                group.textureName    = middleTexName;
                createWallSection(v1, v2, bottom, top, middleTexName, 2,
                                  sector2.ceiling_height, linedef.flags,
                                  rightSide, group.vertices, group.indices);
              }
            }
          }
        }
      }
      // One-sided linedef case
      else {
        // One-sided line: render the wall (middle) texture full height,
        // floor to ceiling. Upper/lower on one-sided lines are unused in DOOM.
        std::string oneSidedTex =
            OkStrings::trimFixedString(rightSide.middle_texture, 8);
        if (oneSidedTex.empty() || oneSidedTex == "-") {
          oneSidedTex = OkStrings::trimFixedString(rightSide.lower_texture, 8);
        }
        if (oneSidedTex.empty() || oneSidedTex == "-") {
          oneSidedTex = OkStrings::trimFixedString(rightSide.upper_texture, 8);
        }
        if (!oneSidedTex.empty() && oneSidedTex != "-") {
          GeometryGroup &group = geometryGroups[oneSidedTex];
          group.textureName    = oneSidedTex;
          createWallSection(v1, v2, sector.floor_height, sector.ceiling_height,
                            oneSidedTex, 2, sector.ceiling_height, linedef.flags,
                            rightSide, group.vertices, group.indices);
        }
      }
    }

    // Handle left sector walls (walls facing INTO this sector)
    if (isLeftSector && !isRightSector) {
      const WAD::Sidedef &leftSide  = level.sidedefs[linedef.left_sidedef];
      const WAD::Sidedef &rightSide = level.sidedefs[linedef.right_sidedef];

      if (rightSide.sector < level.sectors.size()) {
        const WAD::Sector &sector1 = level.sectors[leftSide.sector];
        const WAD::Sector &sector2 = level.sectors[rightSide.sector];

        // Upper wall: drawn when THIS (left) sector's ceiling is higher than
        // the neighbour's, filling from the neighbour ceiling up to this one.
        // Keep the linedef's intrinsic v1->v2 order (same as the right side) so
        // the texture U runs in the same direction on both sides; otherwise
        // adjacent faces of e.g. a column come out mirrored. The engine does no
        // backface culling, so winding does not matter here.
        if (sector1.ceiling_height > sector2.ceiling_height) {
          std::string textureName =
              OkStrings::trimFixedString(leftSide.upper_texture, 8);
          if (!textureName.empty() && textureName != "-") {
            GeometryGroup &group = geometryGroups[textureName];
            group.textureName    = textureName;
            createWallSection(v1, v2, sector2.ceiling_height,
                              sector1.ceiling_height, textureName, 0,
                              sector1.ceiling_height, linedef.flags, leftSide,
                              group.vertices, group.indices);
          }
        }

        // Lower wall (step riser): drawn when THIS (left) sector's floor is
        // lower than the neighbour's, filling from this floor up to theirs.
        if (sector1.floor_height < sector2.floor_height) {
          std::string textureName =
              OkStrings::trimFixedString(leftSide.lower_texture, 8);
          if (!textureName.empty() && textureName != "-") {
            GeometryGroup &group = geometryGroups[textureName];
            group.textureName    = textureName;
            // v1->v2 order (see upper-section note): consistent U on both sides.
            createWallSection(v1, v2, sector1.floor_height,
                              sector2.floor_height, textureName, 1,
                              sector1.ceiling_height, linedef.flags, leftSide,
                              group.vertices, group.indices);
          }
        }

        // Create middle wall (from left side perspective)
        std::string middleTexName =
            OkStrings::trimFixedString(leftSide.middle_texture, 8);
        if (!middleTexName.empty() && middleTexName != "-") {
          float bottom = std::max(sector1.floor_height, sector2.floor_height);
          float top = std::min(sector1.ceiling_height, sector2.ceiling_height);

          if (top > bottom) {
            GeometryGroup &group = geometryGroups[middleTexName];
            group.textureName    = middleTexName;
            // v1->v2 order (see upper-section note): consistent U on both sides.
            createWallSection(v1, v2, bottom, top, middleTexName, 2,
                              sector1.ceiling_height, linedef.flags, leftSide,
                              group.vertices, group.indices);
          }
        }
      }
    }
  }

  // Create OkItems from geometry groups
  for (std::map<std::string, GeometryGroup>::iterator it =
           geometryGroups.begin();
       it != geometryGroups.end(); it++) {
    const GeometryGroup &group = it->second;

    if (group.vertices.empty() || group.indices.empty()) {
      continue;
    }

    std::string itemName =
        "sector_" + std::to_string(sectorIndex) + "_wall_" + group.textureName;

    // Calculate bounding box to find the geometric center
    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();

    // Find bounds (vertices are stored as [x, y, z, u, v])
    for (size_t i = 0; i < group.vertices.size(); i += 5) {
      float x = group.vertices[i];
      float y = group.vertices[i + 1];
      float z = group.vertices[i + 2];

      minX = std::min(minX, x);
      maxX = std::max(maxX, x);
      minY = std::min(minY, y);
      maxY = std::max(maxY, y);
      minZ = std::min(minZ, z);
      maxZ = std::max(maxZ, z);
    }

    // Calculate geometric center
    float centerX = (minX + maxX) * 0.5f;
    float centerY = (minY + maxY) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;

    float        *vertexData = new float[group.vertices.size()];
    unsigned int *indexData  = new unsigned int[group.indices.size()];

    // Copy vertex data and translate to local coordinates (relative to center)
    for (size_t i = 0; i < group.vertices.size(); i += 5) {
      vertexData[i]     = group.vertices[i] - centerX;      // x - centerX
      vertexData[i + 1] = group.vertices[i + 1] - centerY;  // y - centerY
      vertexData[i + 2] = group.vertices[i + 2] - centerZ;  // z - centerZ
      vertexData[i + 3] = group.vertices[i + 3];            // u (texture coord)
      vertexData[i + 4] = group.vertices[i + 4];            // v (texture coord)
    }

    // Copy index data unchanged
    for (size_t i = 0; i < group.indices.size(); i++) {
      indexData[i] = group.indices[i];
    }

    OkItem *item = new OkItem(
        itemName, vertexData, static_cast<long>(group.vertices.size()),
        indexData, static_cast<long>(group.indices.size()));

    // Set the item's position to the calculated center
    item->setPosition(centerX, centerY, centerZ);

    OkTexture *texture =
        OkTextureHandler::getInstance()->getTexture(group.textureName);
    if (texture) {
      item->setTexture(group.textureName, texture);
      OkLogger::info("WADGeometry", "Assigned texture '" + group.textureName +
                                        "' to wall item '" + itemName + "'");
    } else {
      OkLogger::error("WADGeometry", "Could not find texture '" +
                                         group.textureName +
                                         "' for wall item '" + itemName + "'");
    }

    wallItems.push_back(item);
  }

  return wallItems;
}
