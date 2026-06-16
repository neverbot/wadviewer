#include "wad-generate.hpp"
#include "okinawa/config/config.hpp"
#include "okinawa/handlers/textures.hpp"
#include "okinawa/item/item.hpp"
#include "okinawa/item/texture.hpp"
#include "okinawa/utils/logger.hpp"
#include "okinawa/utils/strings.hpp"
#include "wad.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

// Initialize constants
const float WADGenerate::SCALE        = 1.0f;
const float WADGenerate::TEXTURE_SIZE = 64.0f;  // DOOM uses 64x64 flat textures

std::vector<OkItem *> WADGenerate::generateFloors(
    const WAD::Level                    &level,
    const std::vector<std::vector<int>> &sectorVertices) {

  std::vector<OkItem *> floorItems;

  for (size_t i = 0; i < level.sectors.size(); i++) {
    if (i >= sectorVertices.size()) {
      continue;
    }

    const WAD::Sector &sector = level.sectors[i];
    OkItem *floorItem = generateSectorFloor(level, sector, sectorVertices[i],
                                            static_cast<int>(i));

    if (floorItem) {
      floorItems.push_back(floorItem);
    }
  }

  return floorItems;
}

std::vector<OkItem *> WADGenerate::generateCeilings(
    const WAD::Level                    &level,
    const std::vector<std::vector<int>> &sectorVertices) {

  std::vector<OkItem *> ceilingItems;

  for (size_t i = 0; i < level.sectors.size(); i++) {
    if (i >= sectorVertices.size()) {
      continue;
    }

    const WAD::Sector &sector      = level.sectors[i];
    OkItem            *ceilingItem = generateSectorCeiling(
        level, sector, sectorVertices[i], static_cast<int>(i));

    if (ceilingItem) {
      ceilingItems.push_back(ceilingItem);
    }
  }

  return ceilingItems;
}

OkItem *WADGenerate::generateSectorFloor(const WAD::Level       &level,
                                         const WAD::Sector      &sector,
                                         const std::vector<int> &sectorVertices,
                                         int                     sectorIndex) {

  // Check if sector has valid floor texture
  std::string floorTexName =
      OkStrings::trimFixedString(sector.floor_texture, 8);
  if (floorTexName.empty() || floorTexName == "-") {
    return nullptr;
  }

  // Check if we have enough vertices
  if (sectorVertices.size() < 3) {
    return nullptr;
  }

  std::vector<float>        vertices;
  std::vector<unsigned int> indices;

  // Create geometry for this floor
  createSectorGeometry(level, sector, sectorVertices, true, vertices, indices);

  if (vertices.empty() || indices.empty()) {
    return nullptr;
  }

  // Calculate the geometric center for positioning
  float centerX, centerY, centerZ;
  float height = static_cast<float>(sector.floor_height) * SCALE;
  calculateSectorCenter(level, sectorVertices, height, centerX, centerY,
                        centerZ);

  // Convert to local coordinates (relative to center)
  for (size_t i = 0; i < vertices.size(); i += 5) {
    vertices[i] -= centerX;      // x - centerX
    vertices[i + 1] -= centerY;  // y - centerY
    vertices[i + 2] -= centerZ;  // z - centerZ
    // vertices[i + 3] and vertices[i + 4] are texture coords, leave unchanged
  }

  // Create vertex and index arrays for OkItem
  float        *vertexData = new float[vertices.size()];
  unsigned int *indexData  = new unsigned int[indices.size()];

  for (size_t i = 0; i < vertices.size(); i++) {
    vertexData[i] = vertices[i];
  }
  for (size_t i = 0; i < indices.size(); i++) {
    indexData[i] = indices[i];
  }

  // Create the item
  std::string itemName =
      "floor_sector_" + std::to_string(sectorIndex) + "_" + floorTexName;
  OkItem *item =
      new OkItem(itemName, vertexData, static_cast<long>(vertices.size()),
                 indexData, static_cast<long>(indices.size()));

  // Set the item's position to the calculated center
  item->setPosition(centerX, centerY, centerZ);

  // Try to assign texture
  OkTexture *texture =
      OkTextureHandler::getInstance()->getTexture(floorTexName);
  if (texture) {
    item->setTexture(floorTexName, texture);
    OkLogger::info("WADGenerator", "Generated floor item '" + itemName +
                                       "' with texture '" + floorTexName + "'");
  } else {
    OkLogger::warning("WADGenerator", "Could not find texture '" +
                                          floorTexName + "' for floor item '" +
                                          itemName + "'");
  }

  return item;
}

OkItem *WADGenerate::generateSectorCeiling(
    const WAD::Level &level, const WAD::Sector &sector,
    const std::vector<int> &sectorVertices, int sectorIndex) {

  // Check if sector has valid ceiling texture
  std::string ceilingTexName =
      OkStrings::trimFixedString(sector.ceiling_texture, 8);
  if (ceilingTexName.empty() || ceilingTexName == "-") {
    return nullptr;
  }

  // Check if we have enough vertices
  if (sectorVertices.size() < 3) {
    return nullptr;
  }

  std::vector<float>        vertices;
  std::vector<unsigned int> indices;

  // Create geometry for this ceiling
  createSectorGeometry(level, sector, sectorVertices, false, vertices, indices);

  if (vertices.empty() || indices.empty()) {
    return nullptr;
  }

  // Calculate the geometric center for positioning
  float centerX, centerY, centerZ;
  float height = static_cast<float>(sector.ceiling_height) * SCALE;
  calculateSectorCenter(level, sectorVertices, height, centerX, centerY,
                        centerZ);

  // Convert to local coordinates (relative to center)
  for (size_t i = 0; i < vertices.size(); i += 5) {
    vertices[i] -= centerX;      // x - centerX
    vertices[i + 1] -= centerY;  // y - centerY
    vertices[i + 2] -= centerZ;  // z - centerZ
    // vertices[i + 3] and vertices[i + 4] are texture coords, leave unchanged
  }

  // Create vertex and index arrays for OkItem
  float        *vertexData = new float[vertices.size()];
  unsigned int *indexData  = new unsigned int[indices.size()];

  for (size_t i = 0; i < vertices.size(); i++) {
    vertexData[i] = vertices[i];
  }
  for (size_t i = 0; i < indices.size(); i++) {
    indexData[i] = indices[i];
  }

  // Create the item
  std::string itemName =
      "ceiling_sector_" + std::to_string(sectorIndex) + "_" + ceilingTexName;
  OkItem *item =
      new OkItem(itemName, vertexData, static_cast<long>(vertices.size()),
                 indexData, static_cast<long>(indices.size()));

  // Set the item's position to the calculated center
  item->setPosition(centerX, centerY, centerZ);

  // Try to assign texture
  OkTexture *texture =
      OkTextureHandler::getInstance()->getTexture(ceilingTexName);
  if (texture) {
    item->setTexture(ceilingTexName, texture);
    OkLogger::info("WADGenerator", "Generated ceiling item '" + itemName +
                                       "' with texture '" + ceilingTexName +
                                       "'");
  } else {
    OkLogger::warning("WADGenerator",
                      "Could not find texture '" + ceilingTexName +
                          "' for ceiling item '" + itemName + "'");
  }

  return item;
}

void WADGenerate::createSectorGeometry(const WAD::Level       &level,
                                       const WAD::Sector      &sector,
                                       const std::vector<int> &sectorVertices,
                                       bool                    isFloor,
                                       std::vector<float>     &vertices,
                                       std::vector<unsigned int> &indices) {

  if (sectorVertices.size() < 3) {
    return;  // Need at least 3 vertices to form a polygon
  }

  // Get level center coordinates from global config
  float levelCenterX = OkConfig::getFloat("level.center.x");
  float levelCenterY = OkConfig::getFloat("level.center.y");

  float height = isFloor ? static_cast<float>(sector.floor_height) * SCALE
                         : static_cast<float>(sector.ceiling_height) * SCALE;

  // Create vertices with proper texture coordinates
  unsigned int baseIndex = 0;  // Start from 0 since this is a new item

  for (size_t i = 0; i < sectorVertices.size(); i++) {
    if (sectorVertices[i] >= static_cast<int>(level.vertices.size())) {
      continue;  // Skip invalid vertex indices
    }

    const WAD::Vertex &vertex = level.vertices[sectorVertices[i]];
    // Apply the same coordinate transformation as walls
    float x = (static_cast<float>(vertex.x) - levelCenterX) * SCALE;
    float z = (static_cast<float>(vertex.y) - levelCenterY) * SCALE;

    // Flats tile on the absolute world grid (origin 0,0) so adjacent sectors
    // align, matching vanilla DOOM. GL_REPEAT tiles, so emit raw UVs (no
    // per-vertex fmod, which would smear the texture across tile boundaries).
    float u = static_cast<float>(vertex.x) / TEXTURE_SIZE;
    float v = static_cast<float>(vertex.y) / TEXTURE_SIZE;

    vertices.push_back(x);
    vertices.push_back(height);
    vertices.push_back(-z);  // Negate Z for proper coordinate system
    vertices.push_back(u);
    vertices.push_back(v);
  }

  // Create triangles using a simple triangle fan
  for (size_t i = 1; i < sectorVertices.size() - 1; i++) {
    if (isFloor) {
      // Floor - CCW winding
      indices.push_back(baseIndex);                                 // Center
      indices.push_back(baseIndex + static_cast<unsigned int>(i));  // Current
      indices.push_back(baseIndex + static_cast<unsigned int>(i + 1));  // Next
    } else {
      // Ceiling - Reverse winding
      indices.push_back(baseIndex);  // Center
      indices.push_back(baseIndex + static_cast<unsigned int>(i + 1));  // Next
      indices.push_back(baseIndex + static_cast<unsigned int>(i));  // Current
    }
  }
}

void WADGenerate::calculateSectorCenter(const WAD::Level       &level,
                                        const std::vector<int> &sectorVertices,
                                        float height, float &centerX,
                                        float &centerY, float &centerZ) {

  if (sectorVertices.empty()) {
    centerX = centerY = centerZ = 0.0f;
    return;
  }

  // Get level center coordinates from global config
  float levelCenterX = OkConfig::getFloat("level.center.x");
  float levelCenterY = OkConfig::getFloat("level.center.y");

  float minX = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float minZ = std::numeric_limits<float>::max();
  float maxZ = std::numeric_limits<float>::lowest();

  // Calculate bounds using same coordinate transformation as walls
  for (size_t i = 0; i < sectorVertices.size(); i++) {
    if (sectorVertices[i] >= static_cast<int>(level.vertices.size())) {
      continue;  // Skip invalid vertex indices
    }

    const WAD::Vertex &vertex = level.vertices[sectorVertices[i]];
    // Apply the same coordinate transformation as walls
    float x = (static_cast<float>(vertex.x) - levelCenterX) * SCALE;
    float z = (static_cast<float>(vertex.y) - levelCenterY) * SCALE;

    minX = std::min(minX, x);
    maxX = std::max(maxX, x);
    minZ = std::min(minZ, z);
    maxZ = std::max(maxZ, z);
  }

  // Calculate center
  centerX = (minX + maxX) * 0.5f;
  centerY = height;                 // Y is the height
  centerZ = -(minZ + maxZ) * 0.5f;  // Negate Z for proper coordinate system
}

std::vector<int> WADGenerate::generateSectorVertices(const WAD::Level &level,
                                                     int sectorIndex) {
  std::vector<int> sectorVertices;

  // Iterate through all linedefs to find ones that reference this sector
  for (size_t i = 0; i < level.linedefs.size(); i++) {
    const WAD::Linedef &linedef = level.linedefs[i];

    // Skip invalid vertex indices
    if (linedef.start_vertex >= level.vertices.size() ||
        linedef.end_vertex >= level.vertices.size()) {
      continue;
    }

    // Check right side
    if (linedef.right_sidedef != 0xFFFF &&
        linedef.right_sidedef < level.sidedefs.size()) {
      const WAD::Sidedef &rightSide = level.sidedefs[linedef.right_sidedef];

      if (rightSide.sector == sectorIndex) {
        sectorVertices.push_back(linedef.start_vertex);
        sectorVertices.push_back(linedef.end_vertex);
      }
    }

    // Check left side
    if (linedef.left_sidedef != 0xFFFF &&
        linedef.left_sidedef < level.sidedefs.size()) {
      const WAD::Sidedef &leftSide = level.sidedefs[linedef.left_sidedef];

      if (leftSide.sector == sectorIndex) {
        sectorVertices.push_back(linedef.start_vertex);
        sectorVertices.push_back(linedef.end_vertex);
      }
    }
  }

  // Remove duplicates and sort
  std::sort(sectorVertices.begin(), sectorVertices.end());
  sectorVertices.erase(
      std::unique(sectorVertices.begin(), sectorVertices.end()),
      sectorVertices.end());

  return sectorVertices;
}
