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
                                    const WAD::Vertex &vertex2,
                                    float bottomHeight, float topHeight,
                                    const WAD::Sidedef        &sidedef,
                                    std::vector<float>        &vertices,
                                    std::vector<unsigned int> &indices) {
  // Get level center coordinates from global config
  float       levelCenterX = OkConfig::getFloat("level.center.x");
  float       levelCenterY = OkConfig::getFloat("level.center.y");
  const float SCALE        = 1.0f;

  // Calculate normalized positions
  float x1 = (static_cast<float>(vertex1.x) - levelCenterX) * SCALE;
  float z1 = (static_cast<float>(vertex1.y) - levelCenterY) * SCALE;
  float x2 = (static_cast<float>(vertex2.x) - levelCenterX) * SCALE;
  float z2 = (static_cast<float>(vertex2.y) - levelCenterY) * SCALE;

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

  // Default DOOM texture constants (fallback values)
  float textureWidth  = 64.0f;
  float textureHeight = 128.0f;

  // Try to get actual texture dimensions from the texture handler
  // We need to extract texture name from sidedef to get proper dimensions
  std::string textureName = "";
  for (int i = 0; i < 8 && sidedef.middle_texture[i] != '\0'; i++) {
    textureName += sidedef.middle_texture[i];
  }

  OkTexture *texture = OkTextureHandler::getInstance()->getTexture(textureName);
  if (texture) {
    textureWidth  = static_cast<float>(texture->getWidth());
    textureHeight = static_cast<float>(texture->getHeight());
  }

  // Get texture offsets from sidedef
  float uOffset = static_cast<float>(sidedef.x_offset);
  float vOffset = static_cast<float>(sidedef.y_offset);

  // Calculate U coordinates - texture repeats horizontally based on wall length
  float uOffsetNormalized = uOffset / textureWidth;
  float uRepeat           = wallLength / textureWidth;
  float u1                = uOffsetNormalized;
  float u2                = u1 + uRepeat;

  // DOOM V coordinate system to OpenGL conversion:
  //
  // DOOM texture coordinate system:
  // - V=0 is at the TOP of the texture image
  // - Textures are anchored at the BOTTOM of the wall
  // - Positive vOffset moves texture DOWN relative to wall bottom
  // - V increases downward in the texture image
  //
  // OpenGL texture coordinate system:
  // - V=0 is at the BOTTOM of the texture image
  // - V=1 is at the TOP of the texture image
  // - V increases upward in the texture image

  // Calculate texture coordinates for DOOM->OpenGL conversion
  float wallHeightInTexture = wallHeight / textureHeight;
  float vOffsetInTexture    = vOffset / textureHeight;

  // DOOM texture anchoring algorithm:
  // 1. Texture is anchored at wall bottom
  // 2. vOffset shifts the texture downward (positive = texture moves down)
  // 3. We sample from texture starting at vOffset pixels from texture bottom

  // In DOOM texture space (normalized coordinates where 0=top, 1=bottom):
  // Wall bottom samples from: 1.0 - vOffsetInTexture (texture bottom + offset)
  // Wall top samples from: 1.0 - vOffsetInTexture - wallHeightInTexture

  float vDoomBottom = 1.0f - vOffsetInTexture;  // Wall bottom in DOOM coords
  float vDoomTop =
      vDoomBottom - wallHeightInTexture;  // Wall top in DOOM coords

  // Convert DOOM V coordinates to OpenGL V coordinates (flip Y axis)
  // DOOM V=0 (texture top) -> OpenGL V=1
  // DOOM V=1 (texture bottom) -> OpenGL V=0
  float vBottom = 1.0f - vDoomBottom;  // Wall bottom in OpenGL
  float vTop    = 1.0f - vDoomTop;     // Wall top in OpenGL

  // Handle texture wrapping for walls taller than texture
  if (wallHeight > textureHeight) {
    // Allow V coordinates to extend beyond [0,1] for tiling
    vTop = vBottom + wallHeightInTexture;
  }

  // Handle negative vOffset (texture shifted up)
  if (vOffset < 0.0f) {
    float shift = (-vOffset) / textureHeight;
    vBottom += shift;
    vTop += shift;
  }

  // Debug V coordinates (enhanced logging for texture issues)
  // Log walls over 100 length OR door textures for debugging
  bool shouldLog = (wallLength > 100.0f) || (sidedef.middle_texture[0] == 'D' &&
                                             sidedef.middle_texture[1] == 'O');

  // Ensure UV coordinates match the real size of the polygon
  float vRepeat = wallHeight / textureHeight;

  if (shouldLog) {
    std::string texName = "";
    for (int i = 0; i < 8 && sidedef.middle_texture[i] != '\0'; i++) {
      texName += sidedef.middle_texture[i];
    }
    OkLogger::info("UV_DEBUG", "Texture: " + texName + ", Wall length: " +
                                   std::to_string(wallLength) +
                                   ", Height: " + std::to_string(wallHeight) +
                                   ", U repeat: " + std::to_string(uRepeat) +
                                   ", V repeat: " + std::to_string(vRepeat));
  }

  // Flip V axis for OpenGL
  vBottom = 1.0f - vBottom;
  vTop    = 1.0f - vTop;

  // Add vertices with texture coordinates
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

  // Create a sector group for each sector
  for (size_t i = 0; i < level.sectors.size(); i++) {
    const WAD::Sector &sector = level.sectors[i];

    // Generate vertices for this sector
    std::vector<int> sectorVertices =
        WADGenerate::generateSectorVertices(level, static_cast<int>(i));

    // Create the sector group
    OkItemGroup *sectorGroup =
        createSectorGroup(level, sector, static_cast<int>(i), sectorVertices);

    if (sectorGroup) {
      sectorGroups.push_back(sectorGroup);
    }
  }

  return sectorGroups;
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
  // Get level center coordinates from global config
  float       levelCenterX = OkConfig::getFloat("level.center.x");
  float       levelCenterY = OkConfig::getFloat("level.center.y");
  const float SCALE        = 1.0f;

  // Calculate normalized positions
  float x1 = (static_cast<float>(vertex1.x) - levelCenterX) * SCALE;
  float z1 = (static_cast<float>(vertex1.y) - levelCenterY) * SCALE;
  float x2 = (static_cast<float>(vertex2.x) - levelCenterX) * SCALE;
  float z2 = (static_cast<float>(vertex2.y) - levelCenterY) * SCALE;

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
  // DOOM texture offsets are in texture pixels, normalize to 0-1 range
  float u1 = uOffset / TEXTURE_WIDTH;
  float u2 = u1 + (wallLength / TEXTURE_WIDTH);  // Texture repeats along length
  // DOOM V coordinates: textures are anchored at bottom-left
  // vOffset = 0 means texture starts at bottom of wall
  // For OpenGL: v=0 is bottom, v=1 is top
  float vBottom = vOffset / TEXTURE_HEIGHT;                 // Bottom of texture
  float vTop    = vBottom + (wallHeight / TEXTURE_HEIGHT);  // Top of texture

  // Ensure textureHeight and shouldLog are defined in the correct scope
  float textureHeight = 128.0f;  // Default DOOM texture height

  // Adjust UV mapping for upper, middle, and lower textures
  float upperWallHeight =
      static_cast<float>(sector1.ceiling_height - sector2.ceiling_height);
  float vUpperRepeat = upperWallHeight / textureHeight;
  vTop               = vBottom + vUpperRepeat;

  float lowerWallHeight =
      static_cast<float>(sector2.floor_height - sector1.floor_height);
  float vLowerRepeat = lowerWallHeight / textureHeight;
  vTop               = vBottom + vLowerRepeat;

  float middleWallHeight = wallHeight;
  float vMiddleRepeat    = middleWallHeight / textureHeight;
  vTop                   = vBottom + vMiddleRepeat;

  // Debug UV mapping for textures

  OkLogger::info("UV_DEBUG",
                 "Adjusted UV mapping: vBottom=" + std::to_string(vBottom) +
                     ", vTop=" + std::to_string(vTop));

  // Add vertices for the wall quad with proper texture coordinates
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

  // Add indices for two triangles (CCW winding)
  indices.push_back(baseIndex);      // Bottom left
  indices.push_back(baseIndex + 1);  // Top left
  indices.push_back(baseIndex + 2);  // Bottom right

  indices.push_back(baseIndex + 1);  // Top left
  indices.push_back(baseIndex + 3);  // Top right
  indices.push_back(baseIndex + 2);  // Bottom right
}

OkItemGroup *
WADGeometry::createSectorGroup(const WAD::Level  &level,
                               const WAD::Sector &sector, int sectorIndex,
                               const std::vector<int> &sectorVertices) {
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
                                sector2.floor_height, rightSide, group.vertices,
                                group.indices);
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
        // For one-sided walls, we need to handle upper, middle, and lower
        // textures Since there's no adjacent sector, we use the full sector
        // height and split it based on texture availability and standard DOOM
        // conventions

        float wallFloor       = static_cast<float>(sector.floor_height);
        float wallCeiling     = static_cast<float>(sector.ceiling_height);
        float totalWallHeight = wallCeiling - wallFloor;

        // Standard DOOM texture height for calculating splits
        const float STANDARD_TEXTURE_HEIGHT = 128.0f;

        // Check which textures are defined
        std::string upperTexName =
            OkStrings::trimFixedString(rightSide.upper_texture, 8);
        std::string middleTexName =
            OkStrings::trimFixedString(rightSide.middle_texture, 8);
        std::string lowerTexName =
            OkStrings::trimFixedString(rightSide.lower_texture, 8);

        bool hasUpper  = !upperTexName.empty() && upperTexName != "-";
        bool hasMiddle = !middleTexName.empty() && middleTexName != "-";
        bool hasLower  = !lowerTexName.empty() && lowerTexName != "-";

        // Calculate height splits based on which textures are present
        if (hasUpper && hasMiddle && hasLower) {
          // All three textures: split wall into three equal parts or use
          // texture-based proportions
          float upperHeight  = STANDARD_TEXTURE_HEIGHT;
          float lowerHeight  = STANDARD_TEXTURE_HEIGHT;
          float middleHeight = totalWallHeight - upperHeight - lowerHeight;

          // Ensure middle section is not negative
          if (middleHeight < 0.0f) {
            // If wall is too short, scale proportionally
            float scale = totalWallHeight /
                          (upperHeight + lowerHeight + STANDARD_TEXTURE_HEIGHT);
            upperHeight *= scale;
            lowerHeight *= scale;
            middleHeight = totalWallHeight - upperHeight - lowerHeight;
          }

          // Create upper wall section
          GeometryGroup &upperGroup = geometryGroups[upperTexName];
          upperGroup.textureName    = upperTexName;
          createWallSection(v1, v2, wallCeiling - upperHeight, wallCeiling,
                            rightSide, upperGroup.vertices, upperGroup.indices);

          // Create middle wall section
          GeometryGroup &middleGroup = geometryGroups[middleTexName];
          middleGroup.textureName    = middleTexName;
          createWallSection(v1, v2, wallFloor + lowerHeight,
                            wallCeiling - upperHeight, rightSide,
                            middleGroup.vertices, middleGroup.indices);

          // Create lower wall section
          GeometryGroup &lowerGroup = geometryGroups[lowerTexName];
          lowerGroup.textureName    = lowerTexName;
          createWallSection(v1, v2, wallFloor, wallFloor + lowerHeight,
                            rightSide, lowerGroup.vertices, lowerGroup.indices);
        } else if (hasUpper && hasMiddle) {
          // Upper and middle: upper takes standard height, middle takes the
          // rest
          float upperHeight =
              std::min(STANDARD_TEXTURE_HEIGHT, totalWallHeight * 0.5f);

          GeometryGroup &upperGroup = geometryGroups[upperTexName];
          upperGroup.textureName    = upperTexName;
          createWallSection(v1, v2, wallCeiling - upperHeight, wallCeiling,
                            rightSide, upperGroup.vertices, upperGroup.indices);

          GeometryGroup &middleGroup = geometryGroups[middleTexName];
          middleGroup.textureName    = middleTexName;
          createWallSection(v1, v2, wallFloor, wallCeiling - upperHeight,
                            rightSide, middleGroup.vertices,
                            middleGroup.indices);
        } else if (hasMiddle && hasLower) {
          // Middle and lower: lower takes standard height, middle takes the
          // rest
          float lowerHeight =
              std::min(STANDARD_TEXTURE_HEIGHT, totalWallHeight * 0.5f);

          GeometryGroup &lowerGroup = geometryGroups[lowerTexName];
          lowerGroup.textureName    = lowerTexName;
          createWallSection(v1, v2, wallFloor, wallFloor + lowerHeight,
                            rightSide, lowerGroup.vertices, lowerGroup.indices);

          GeometryGroup &middleGroup = geometryGroups[middleTexName];
          middleGroup.textureName    = middleTexName;
          createWallSection(v1, v2, wallFloor + lowerHeight, wallCeiling,
                            rightSide, middleGroup.vertices,
                            middleGroup.indices);
        } else if (hasUpper && hasLower) {
          // Upper and lower only: split wall in half
          float halfHeight = totalWallHeight * 0.5f;

          GeometryGroup &upperGroup = geometryGroups[upperTexName];
          upperGroup.textureName    = upperTexName;
          createWallSection(v1, v2, wallFloor + halfHeight, wallCeiling,
                            rightSide, upperGroup.vertices, upperGroup.indices);

          GeometryGroup &lowerGroup = geometryGroups[lowerTexName];
          lowerGroup.textureName    = lowerTexName;
          createWallSection(v1, v2, wallFloor, wallFloor + halfHeight,
                            rightSide, lowerGroup.vertices, lowerGroup.indices);
        } else if (hasUpper) {
          // Upper texture only: use full wall height
          GeometryGroup &upperGroup = geometryGroups[upperTexName];
          upperGroup.textureName    = upperTexName;
          createWallSection(v1, v2, wallFloor, wallCeiling, rightSide,
                            upperGroup.vertices, upperGroup.indices);
        } else if (hasLower) {
          // Lower texture only: use full wall height
          GeometryGroup &lowerGroup = geometryGroups[lowerTexName];
          lowerGroup.textureName    = lowerTexName;
          createWallSection(v1, v2, wallFloor, wallCeiling, rightSide,
                            lowerGroup.vertices, lowerGroup.indices);
        } else if (hasMiddle) {
          // Middle texture only: use full wall height (original behavior)
          GeometryGroup &middleGroup = geometryGroups[middleTexName];
          middleGroup.textureName    = middleTexName;
          createWallSection(v1, v2, wallFloor, wallCeiling, rightSide,
                            middleGroup.vertices, middleGroup.indices);
        }
        // If no textures are defined, create no wall sections
      }
    }

    // Handle left sector walls (walls facing INTO this sector)
    if (isLeftSector && !isRightSector) {
      const WAD::Sidedef &leftSide  = level.sidedefs[linedef.left_sidedef];
      const WAD::Sidedef &rightSide = level.sidedefs[linedef.right_sidedef];

      if (rightSide.sector < level.sectors.size()) {
        const WAD::Sector &sector1 = level.sectors[leftSide.sector];
        const WAD::Sector &sector2 = level.sectors[rightSide.sector];

        // Create upper wall if ceilings differ (from left side perspective)
        if (sector2.ceiling_height > sector1.ceiling_height) {
          std::string textureName =
              OkStrings::trimFixedString(leftSide.upper_texture, 8);
          if (!textureName.empty() && textureName != "-") {
            GeometryGroup &group = geometryGroups[textureName];
            group.textureName    = textureName;
            // Note: reverse vertex order for left side
            createWallSection(v2, v1, sector1.ceiling_height,
                              sector2.ceiling_height, leftSide, group.vertices,
                              group.indices);
          }
        }

        // Create lower wall if floors differ (from left side perspective)
        if (sector1.floor_height > sector2.floor_height) {
          std::string textureName =
              OkStrings::trimFixedString(leftSide.lower_texture, 8);
          if (!textureName.empty() && textureName != "-") {
            GeometryGroup &group = geometryGroups[textureName];
            group.textureName    = textureName;
            // Note: reverse vertex order for left side
            createWallSection(v2, v1, sector2.floor_height,
                              sector1.floor_height, leftSide, group.vertices,
                              group.indices);
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
            // Note: reverse vertex order for left side
            createWallSection(v2, v1, bottom, top, leftSide, group.vertices,
                              group.indices);
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
