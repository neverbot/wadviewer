#include "wad-converter.hpp"
#include "wad-geometry.hpp"

// Initialize static members
float       WADConverter::centerX = 0.0f;
float       WADConverter::centerY = 0.0f;
const float WADConverter::SCALE   = 1.0f;

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

  // Convert DOOM coordinates to our coordinate system
  float x = (static_cast<float>(level.player_start.x) - centerX) * SCALE;
  float z = (static_cast<float>(level.player_start.y) - centerY) * SCALE;

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
    centerX = 0.0f;
    centerY = 0.0f;
    return;
  }

  int16_t minX = level.vertices[0].x;
  int16_t maxX = level.vertices[0].x;
  int16_t minY = level.vertices[0].y;
  int16_t maxY = level.vertices[0].y;

  for (size_t i = 1; i < level.vertices.size(); i++) {
    const WAD::Vertex &vertex = level.vertices[i];
    if (vertex.x < minX)
      minX = vertex.x;
    if (vertex.x > maxX)
      maxX = vertex.x;
    if (vertex.y < minY)
      minY = vertex.y;
    if (vertex.y > maxY)
      maxY = vertex.y;
  }

  centerX = static_cast<float>(minX + maxX) / 2.0f;
  centerY = static_cast<float>(minY + maxY) / 2.0f;
}

/**
 * @brief Convert a WAD level to renderable geometry items.
 * @param level The WAD level to convert.
 * @return Vector of OkItem pointers representing the level geometry.
 */
std::vector<OkItem *> WADConverter::convertLevel(const WAD::Level &level) {
  std::vector<OkItem *> items;

  // Calculate level bounds for centering
  calculateLevelBounds(level);

  // Convert level geometry to renderable items
  std::vector<OkItem *> geometryItems = WADGeometry::createLevelGeometry(level);

  // Add all geometry items to the result
  for (size_t i = 0; i < geometryItems.size(); i++) {
    items.push_back(geometryItems[i]);
  }

  return items;
}
