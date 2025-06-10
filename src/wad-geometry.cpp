#include "wad-geometry.hpp"
#include "../okinawa.cpp/src/handlers/textures.hpp"
#include "../okinawa.cpp/src/utils/logger.hpp"
#include "../okinawa.cpp/src/utils/strings.hpp"
#include "wad-generate.hpp"
#include "wad-textures.hpp"
#include <cmath>
#include <limits>

// Initialize static members
float       WADGeometry::centerX = 0.0f;
float       WADGeometry::centerY = 0.0f;
const float WADGeometry::SCALE   = 1.0f;

void WADGeometry::createWallSection(const WAD::Vertex &vertex1,
                                    const WAD::Vertex &vertex2,
                                    float bottomHeight, float topHeight,
                                    const WAD::Sidedef        &sidedef,
                                    std::vector<float>        &vertices,
                                    std::vector<unsigned int> &indices) {
  // Calculate normalized positions
  float x1 = (static_cast<float>(vertex1.x) - centerX) * SCALE;
  float z1 = (static_cast<float>(vertex1.y) - centerY) * SCALE;
  float x2 = (static_cast<float>(vertex2.x) - centerX) * SCALE;
  float z2 = (static_cast<float>(vertex2.y) - centerY) * SCALE;

  // Calculate wall dimensions
  float wallBottom = bottomHeight * SCALE;
  float wallTop    = topHeight * SCALE;
  float wallHeight = wallTop - wallBottom;

  if (wallHeight <= 0.0f) {
    return;
  }

  // Calculate real-world wall length (before scaling)
  float wallLength =
      sqrtf(powf(static_cast<float>(vertex2.x - vertex1.x), 2.0f) +
            powf(static_cast<float>(vertex2.y - vertex1.y), 2.0f));

  // DOOM texture constants
  const float TEXTURE_WIDTH  = 64.0f;
  const float TEXTURE_HEIGHT = 128.0f;

  // Calculate texture coordinates
  float uOffset = static_cast<float>(sidedef.x_offset);
  float vOffset = static_cast<float>(sidedef.y_offset);

  // Calculate number of texture repeats based on unscaled wall length
  float numRepeats = wallLength / TEXTURE_WIDTH;

  // Apply texture coordinates
  float u1 = uOffset / TEXTURE_WIDTH;
  float u2 = u1 + numRepeats;
  float v1 = vOffset / TEXTURE_HEIGHT;
  float v2 = v1 + (wallHeight / (TEXTURE_HEIGHT * SCALE));

  // Add vertices with texture coordinates
  // Bottom left
  vertices.push_back(x1);
  vertices.push_back(wallBottom);
  vertices.push_back(-z1);
  vertices.push_back(u1);
  vertices.push_back(v1);

  // Top left
  vertices.push_back(x1);
  vertices.push_back(wallTop);
  vertices.push_back(-z1);
  vertices.push_back(u1);
  vertices.push_back(v2);

  // Bottom right
  vertices.push_back(x2);
  vertices.push_back(wallBottom);
  vertices.push_back(-z2);
  vertices.push_back(u2);
  vertices.push_back(v1);

  // Top right
  vertices.push_back(x2);
  vertices.push_back(wallTop);
  vertices.push_back(-z2);
  vertices.push_back(u2);
  vertices.push_back(v2);

  // Add indices (CCW winding)
  unsigned int baseIndex = vertices.size() / 5 - 4;  // We just added 4 vertices
  indices.push_back(baseIndex);
  indices.push_back(baseIndex + 1);
  indices.push_back(baseIndex + 2);
  indices.push_back(baseIndex + 1);
  indices.push_back(baseIndex + 3);
  indices.push_back(baseIndex + 2);
}

/**
 * @brief Creates all the geometry for a level.
 * @param level The level to create geometry for.
 * @return A vector of OkItem pointers representing the level geometry.
 */
std::vector<OkItem *>
WADGeometry::createLevelGeometry(const WAD::Level &level) {
  std::vector<OkItem *> items;

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

  centerX = (minX + maxX) / 2.0f;
  centerY = (minY + maxY) / 2.0f;

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

  // Track vertices for each sector
  std::vector<std::vector<int>> sectorVertices(level.sectors.size());

  // Structure to hold geometry for each texture
  struct GeometryGroup {
    std::vector<float>        vertices;
    std::vector<unsigned int> indices;
    std::string               textureName;
  };
  std::map<std::string, GeometryGroup> geometryGroups;

  // First pass: collect vertices for each sector and create walls
  for (size_t i = 0; i < level.linedefs.size(); i++) {
    const WAD::Linedef &linedef = level.linedefs[i];

    // Skip invalid vertex indices
    if (linedef.start_vertex >= level.vertices.size() ||
        linedef.end_vertex >= level.vertices.size()) {
      continue;
    }

    const WAD::Vertex &v1 = level.vertices[linedef.start_vertex];
    const WAD::Vertex &v2 = level.vertices[linedef.end_vertex];

    // Handle right side (always present for valid linedefs)
    if (linedef.right_sidedef != 0xFFFF &&
        linedef.right_sidedef < level.sidedefs.size()) {
      const WAD::Sidedef &rightSide = level.sidedefs[linedef.right_sidedef];

      if (rightSide.sector < level.sectors.size()) {
        // Add vertices to sector
        sectorVertices[rightSide.sector].push_back(linedef.start_vertex);
        sectorVertices[rightSide.sector].push_back(linedef.end_vertex);

        // Handle two-sided linedef case
        if (linedef.left_sidedef != 0xFFFF &&
            linedef.left_sidedef < level.sidedefs.size()) {
          const WAD::Sidedef &leftSide = level.sidedefs[linedef.left_sidedef];

          if (leftSide.sector < level.sectors.size()) {
            const WAD::Sector &sector1 = level.sectors[leftSide.sector];
            const WAD::Sector &sector2 = level.sectors[rightSide.sector];

            // Create upper wall if ceilings differ
            if (sector1.ceiling_height > sector2.ceiling_height) {
              std::string textureName =
                  OkStrings::trimFixedString(rightSide.upper_texture, 8);
              if (!textureName.empty() && textureName != "-") {
                GeometryGroup &group = geometryGroups[textureName];
                group.textureName    = textureName;
                createWallSection(v1, v2, sector2.ceiling_height,
                                  sector1.ceiling_height, rightSide,
                                  group.vertices, group.indices);
              }
            }

            // Create lower wall if floors differ
            if (sector2.floor_height > sector1.floor_height) {
              std::string textureName =
                  OkStrings::trimFixedString(rightSide.lower_texture, 8);
              if (!textureName.empty() && textureName != "-") {
                GeometryGroup &group = geometryGroups[textureName];
                group.textureName    = textureName;
                createWallSection(v1, v2, sector1.floor_height,
                                  sector2.floor_height, rightSide,
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
                  createWallSection(v1, v2, bottom, top, rightSide,
                                    group.vertices, group.indices);
                }
              }
            }
          }
        }
        // One-sided linedef case
        else {
          const WAD::Sector &sector = level.sectors[rightSide.sector];
          std::string        textureName =
              OkStrings::trimFixedString(rightSide.middle_texture, 8);
          if (!textureName.empty() && textureName != "-") {
            GeometryGroup &group = geometryGroups[textureName];
            group.textureName    = textureName;
            createWallSection(v1, v2, sector.floor_height,
                              sector.ceiling_height, rightSide, group.vertices,
                              group.indices);
          }
        }
      }
    }
  }

  // Process sector vertices (remove duplicates)
  for (size_t i = 0; i < level.sectors.size(); i++) {
    // Remove duplicate vertices
    std::sort(sectorVertices[i].begin(), sectorVertices[i].end());
    sectorVertices[i].erase(
        std::unique(sectorVertices[i].begin(), sectorVertices[i].end()),
        sectorVertices[i].end());
  }

  // Second pass: create floor and ceiling geometry for each sector
  std::vector<OkItem *> floorItems =
      WADGenerate::generateFloors(level, sectorVertices);
  std::vector<OkItem *> ceilingItems =
      WADGenerate::generateCeilings(level, sectorVertices);

  // Pre-allocate space for better performance
  items.reserve(items.size() + floorItems.size() + ceilingItems.size());

  // Add floor and ceiling items to the result
  for (size_t i = 0; i < floorItems.size(); i++) {
    items.push_back(floorItems[i]);
  }
  for (size_t i = 0; i < ceilingItems.size(); i++) {
    items.push_back(ceilingItems[i]);
  }

  // Create OkItems from geometry groups
  for (std::map<std::string, GeometryGroup>::iterator it =
           geometryGroups.begin();
       it != geometryGroups.end(); it++) {
    const GeometryGroup &group = it->second;

    if (group.vertices.empty() || group.indices.empty()) {
      continue;
    }

    std::string itemName = "level_" + group.textureName;

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
    for (int i = 0; i < (int)group.indices.size(); i++) {
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
      OkLogger::info("Assigned texture '" + group.textureName + "' to item '" +
                     itemName + "'");
    } else {
      OkLogger::error("Could not find texture '" + group.textureName +
                      "' for item '" + itemName + "'");
    }

    items.push_back(item);
  }

  return items;
}

/**
 * @brief Creates a vertical wall face between two sectors with different
 * heights.
 * @param vertex1 First vertex of the wall
 * @param vertex2 Second vertex of the wall
 * @param sector1 First sector
 * @param sector2 Second sector
 * @param sidedef Sidedef containing texture information
 * @param vertices Output vertex data
 * @param indices Output index data
 */
void WADGeometry::createWallFace(const WAD::Vertex         &vertex1,
                                 const WAD::Vertex         &vertex2,
                                 const WAD::Sector         &sector1,
                                 const WAD::Sector         &sector2,
                                 const WAD::Sidedef        &sidedef,
                                 std::vector<float>        &vertices,
                                 std::vector<unsigned int> &indices) {
  // Calculate normalized positions
  float x1 = (static_cast<float>(vertex1.x) - centerX) * SCALE;
  float z1 = (static_cast<float>(vertex1.y) - centerY) * SCALE;
  float x2 = (static_cast<float>(vertex2.x) - centerX) * SCALE;
  float z2 = (static_cast<float>(vertex2.y) - centerY) * SCALE;

  // Get ceiling and floor heights, applying the same scale
  float floor1 = static_cast<float>(sector1.floor_height) * SCALE;
  float ceil1  = static_cast<float>(sector1.ceiling_height) * SCALE;
  float floor2 = static_cast<float>(sector2.floor_height) * SCALE;
  float ceil2  = static_cast<float>(sector2.ceiling_height) * SCALE;

  // Calculate wall height and length for texture mapping
  float wallBottom, wallTop;
  float wallHeight;

  if (sector1.ceiling_height > sector2.ceiling_height) {
    // Upper wall section - from sector2's ceiling to sector1's ceiling
    wallBottom = ceil2;  // Lower ceiling
    wallTop    = ceil1;  // Higher ceiling
    wallHeight = ceil1 - ceil2;
  } else if (sector2.floor_height > sector1.floor_height) {
    // Lower wall section - from lower floor to higher floor
    wallBottom = floor1;  // Lower floor
    wallTop    = floor2;  // Higher floor
    wallHeight = floor2 - floor1;
  } else {
    // Middle wall section - use full height between sectors
    wallBottom = std::max(floor1, floor2);
    wallTop    = std::min(ceil1, ceil2);
    wallHeight = wallTop - wallBottom;
  }

  float wallLength = sqrtf(powf(x2 - x1, 2.0f) + powf(z2 - z1, 2.0f));

  // Texture coordinates handling
  const float TEXTURE_WIDTH  = 64.0f;   // Standard DOOM texture width
  const float TEXTURE_HEIGHT = 128.0f;  // Standard DOOM texture height

  // Get texture offsets from sidedef
  float uOffset = static_cast<float>(sidedef.x_offset);
  float vOffset = static_cast<float>(sidedef.y_offset);

  // Calculate vertex indices
  unsigned int baseIndex =
      static_cast<unsigned int>(vertices.size() / 5);  // 5 floats per vertex

  // Calculate texture coordinates
  float u1 = uOffset / TEXTURE_WIDTH;
  float u2 = u1 + (wallLength / TEXTURE_WIDTH);  // Texture repeats along length
  float v1 = vOffset / TEXTURE_HEIGHT;
  float v2 = v1 + (wallHeight /
                   (TEXTURE_HEIGHT * SCALE));  // Scale height for texturing

  // Add vertices for the wall quad with proper texture coordinates
  // Bottom left
  vertices.push_back(x1);
  vertices.push_back(wallBottom);
  vertices.push_back(-z1);
  vertices.push_back(u1);
  vertices.push_back(v1);

  // Top left
  vertices.push_back(x1);
  vertices.push_back(wallTop);
  vertices.push_back(-z1);
  vertices.push_back(u1);
  vertices.push_back(v2);

  // Bottom right
  vertices.push_back(x2);
  vertices.push_back(wallBottom);
  vertices.push_back(-z2);
  vertices.push_back(u2);
  vertices.push_back(v1);

  // Top right
  vertices.push_back(x2);
  vertices.push_back(wallTop);
  vertices.push_back(-z2);
  vertices.push_back(u2);
  vertices.push_back(v2);

  // Add indices for two triangles (CCW winding)
  indices.push_back(baseIndex);      // Bottom left
  indices.push_back(baseIndex + 1);  // Top left
  indices.push_back(baseIndex + 2);  // Bottom right

  indices.push_back(baseIndex + 1);  // Top left
  indices.push_back(baseIndex + 3);  // Top right
  indices.push_back(baseIndex + 2);  // Bottom right
}
