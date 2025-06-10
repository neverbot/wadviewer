#include "wad-converter.hpp"
#include "../okinawa.cpp/src/config/config.hpp"
#include "wad-geometry.hpp"
#include <algorithm>

/**
 * @brief Get the player's starting position in the level as a 3D point.
 * @param level The level to get the player start position from.
 * @return A pointer to an OkPoint containing the player start position, or
 * nullptr if no start position exists.
 * @note The returned position represents a camera position for FPS view, with Y
 * coordinate at eye level.
 */
OkPoint *WADConverter::getPlayerStartPosition(const WAD::Level &level) {
  if (!level.has_player_start)
    return nullptr;

  // Get level center coordinates from global config
  float       levelCenterX = OkConfig::getFloat("level.center.x");
  float       levelCenterY = OkConfig::getFloat("level.center.y");
  const float SCALE        = 1.0f;

  // Convert DOOM coordinates to our coordinate system
  float x = (static_cast<float>(level.player_start.x) - levelCenterX) * SCALE;
  float z = (static_cast<float>(level.player_start.y) - levelCenterY) * SCALE;

  // Find the sector the player is in to get the floor height
  float floorHeight = 0.0f;
  for (int i = 0; i < (int)level.linedefs.size(); i++) {
    const WAD::Linedef &linedef = level.linedefs[i];
    if (linedef.right_sidedef >= level.sidedefs.size())
      continue;

    const WAD::Sidedef &sidedef = level.sidedefs[linedef.right_sidedef];
    if (sidedef.sector >= level.sectors.size())
      continue;

    const WAD::Sector &sector = level.sectors[sidedef.sector];

    // Check if point is inside this sector (simplified check)
    const WAD::Vertex &v1 = level.vertices[linedef.start_vertex];
    const WAD::Vertex &v2 = level.vertices[linedef.end_vertex];

    if (pointInSector(level.player_start.x, level.player_start.y, v1.x, v1.y,
                      v2.x, v2.y)) {
      floorHeight = static_cast<float>(sector.floor_height);
      break;
    }
  }

  // DOOM's player eye height is approximately 41 units
  const float PLAYER_EYE_HEIGHT = 41.0f * SCALE;
  float       y                 = (floorHeight + PLAYER_EYE_HEIGHT) * SCALE;

  // Note: Z is negated because DOOM's coordinate system is different from
  // OpenGL
  return new OkPoint(x, y, -z);
}

/**
 * @brief Calculate the bounds of the level for centering purposes.
 * @param level The level to calculate bounds for.
 */
void WADConverter::calculateLevelBounds(const WAD::Level &level) {
  if (level.vertices.empty()) {
    OkConfig::setFloat("level.center.x", 0.0f);
    OkConfig::setFloat("level.center.y", 0.0f);
    return;
  }

  int16_t minX = level.vertices[0].x;
  int16_t maxX = level.vertices[0].x;
  int16_t minY = level.vertices[0].y;
  int16_t maxY = level.vertices[0].y;

  for (size_t i = 1; i < level.vertices.size(); i++) {
    const WAD::Vertex &vertex = level.vertices[i];

    minX = std::min(minX, vertex.x);
    maxX = std::max(maxX, vertex.x);
    minY = std::min(minY, vertex.y);
    maxY = std::max(maxY, vertex.y);
  }

  float levelCenterX = static_cast<float>(minX + maxX) / 2.0f;
  float levelCenterY = static_cast<float>(minY + maxY) / 2.0f;

  // Store level center coordinates in global config
  OkConfig::setFloat("level.center.x", levelCenterX);
  OkConfig::setFloat("level.center.y", levelCenterY);
}

/**
 * @brief Convert a WAD level to renderable geometry item groups.
 * @param level The WAD level to convert.
 * @return Vector of OkItemGroup pointers representing the level geometry
 * organized by sectors.
 */
std::vector<OkItemGroup *> WADConverter::convertLevel(const WAD::Level &level) {
  // Calculate level bounds for centering
  calculateLevelBounds(level);

  // Convert level geometry to renderable item groups and return directly
  return WADGeometry::createLevelGeometry(level);
}
