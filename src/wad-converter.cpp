#include "wad-converter.hpp"
#include "../okinawa.cpp/src/handlers/textures.hpp"
#include "../okinawa.cpp/src/utils/logger.hpp"
#include "../okinawa.cpp/src/utils/strings.hpp"
#include <cmath>
#include <limits>

// Initialize static members
float       WADConverter::centerX = 0.0f;
float       WADConverter::centerY = 0.0f;
const float WADConverter::SCALE   = 1.0f;

// Empty constructor/destructor
WADConverter::WADConverter() {}
WADConverter::~WADConverter() {}

void WADConverter::createWallSection(const WAD::Vertex &vertex1,
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
      sqrt(pow(vertex2.x - vertex1.x, 2) + pow(vertex2.y - vertex1.y, 2));

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
WADConverter::createLevelGeometry(const WAD::Level &level) {
  std::vector<OkItem *> items;

  // Calculate level bounds and set center
  float minX = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float minY = std::numeric_limits<float>::max();
  float maxY = std::numeric_limits<float>::lowest();

  for (int i = 0; i < (int)level.vertices.size(); i++) {
    const WAD::Vertex &vertex = level.vertices[i];
    minX                      = std::min(minX, static_cast<float>(vertex.x));
    maxX                      = std::max(maxX, static_cast<float>(vertex.x));
    minY                      = std::min(minY, static_cast<float>(vertex.y));
    maxY                      = std::max(maxY, static_cast<float>(vertex.y));
  }

  centerX = (minX + maxX) / 2.0f;
  centerY = (minY + maxY) / 2.0f;

  // First, create all flat (floor/ceiling) textures
  for (int i = 0; i < (int)level.flats.size(); i++) {
    const WAD::FlatData &flat = level.flats[i];
    createFlatTexture(flat.name, flat, level.palette);
  }

  // Then load all wall textures we'll need
  for (int i = 0; i < (int)level.sidedefs.size(); i++) {
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
          createTextureFromDef(texDef, level.patches, level.palette);
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
  for (int i = 0; i < (int)level.linedefs.size(); i++) {
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

  // Second pass: create floor and ceiling geometry for each sector
  for (int i = 0; i < (int)level.sectors.size(); i++) {
    const WAD::Sector &sector = level.sectors[i];

    // Remove duplicate vertices
    std::sort(sectorVertices[i].begin(), sectorVertices[i].end());
    sectorVertices[i].erase(
        std::unique(sectorVertices[i].begin(), sectorVertices[i].end()),
        sectorVertices[i].end());

    // Create floor
    std::string floorTexName =
        OkStrings::trimFixedString(sector.floor_texture, 8);
    if (!floorTexName.empty() && floorTexName != "-") {
      GeometryGroup &group = geometryGroups[floorTexName];
      group.textureName    = floorTexName;
      createSectorGeometry(level, sector, sectorVertices[i], group.vertices,
                           group.indices, true);
    }

    // Create ceiling
    std::string ceilingTexName =
        OkStrings::trimFixedString(sector.ceiling_texture, 8);
    if (!ceilingTexName.empty() && ceilingTexName != "-") {
      GeometryGroup &group = geometryGroups[ceilingTexName];
      group.textureName    = ceilingTexName;
      createSectorGeometry(level, sector, sectorVertices[i], group.vertices,
                           group.indices, false);
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

    std::string   itemName   = "level_" + group.textureName;
    float        *vertexData = new float[group.vertices.size()];
    unsigned int *indexData  = new unsigned int[group.indices.size()];

    // Copy vertex and index data
    for (int i = 0; i < (int)group.vertices.size(); i++) {
      vertexData[i] = group.vertices[i];
    }
    for (int i = 0; i < (int)group.indices.size(); i++) {
      indexData[i] = group.indices[i];
    }

    OkItem *item = new OkItem(itemName, vertexData, group.vertices.size(),
                              indexData, group.indices.size());

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

void WADConverter::createSectorGeometry(const WAD::Level       &level,
                                        const WAD::Sector      &sector,
                                        const std::vector<int> &sectorVertices,
                                        std::vector<float>     &vertices,
                                        std::vector<unsigned int> &indices,
                                        bool                       isFloor) {

  if (sectorVertices.size() < 3) {
    return;  // Need at least 3 vertices to form a polygon
  }

  float height = isFloor ? static_cast<float>(sector.floor_height) * SCALE
                         : static_cast<float>(sector.ceiling_height) * SCALE;

  // First create all vertices for the sector
  unsigned int baseIndex    = vertices.size() / 5;  // 5 floats per vertex
  const float  TEXTURE_SIZE = 64.0f;  // DOOM uses 64x64 flat textures

  // Calculate sector bounds for texture mapping
  float minX = std::numeric_limits<float>::max();
  float minY = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float maxY = std::numeric_limits<float>::lowest();

  // First pass - get bounds for texture coordinates
  for (int i = 0; i < (int)sectorVertices.size(); i++) {
    const WAD::Vertex &vertex = level.vertices[sectorVertices[i]];
    float              worldX = static_cast<float>(vertex.x);
    float              worldY = static_cast<float>(vertex.y);

    minX = std::min(minX, worldX);
    maxX = std::max(maxX, worldX);
    minY = std::min(minY, worldY);
    maxY = std::max(maxY, worldY);
  }

  // Create vertices with proper texture coordinates
  for (int i = 0; i < (int)sectorVertices.size(); i++) {
    const WAD::Vertex &vertex = level.vertices[sectorVertices[i]];
    float              x = (static_cast<float>(vertex.x) - centerX) * SCALE;
    float              z = (static_cast<float>(vertex.y) - centerY) * SCALE;

    // Calculate UV coordinates based on world position
    float u = fmod((vertex.x - minX) / TEXTURE_SIZE, 1.0f);
    float v = fmod((vertex.y - minY) / TEXTURE_SIZE, 1.0f);

    if (!isFloor) {
      v = 1.0f - v;  // Flip V coordinate for ceiling
    }

    vertices.push_back(x);
    vertices.push_back(height);
    vertices.push_back(-z);
    vertices.push_back(u);
    vertices.push_back(v);
  }

  // Create triangles using a simple triangle fan
  for (int i = 1; i < (int)sectorVertices.size() - 1; i++) {
    if (isFloor) {
      // Floor - CCW winding
      indices.push_back(baseIndex);          // Center
      indices.push_back(baseIndex + i);      // Current
      indices.push_back(baseIndex + i + 1);  // Next
    } else {
      // Ceiling - Reverse winding
      indices.push_back(baseIndex);          // Center
      indices.push_back(baseIndex + i + 1);  // Next
      indices.push_back(baseIndex + i);      // Current
    }
  }
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
void WADConverter::createWallFace(const WAD::Vertex         &vertex1,
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

  float wallLength = sqrt(pow(x2 - x1, 2) + pow(z2 - z1, 2));

  // Texture coordinates handling
  const float TEXTURE_WIDTH  = 64.0f;   // Standard DOOM texture width
  const float TEXTURE_HEIGHT = 128.0f;  // Standard DOOM texture height

  // Get texture offsets from sidedef
  float uOffset = static_cast<float>(sidedef.x_offset);
  float vOffset = static_cast<float>(sidedef.y_offset);

  // Calculate vertex indices
  unsigned int baseIndex = vertices.size() / 5;  // 5 floats per vertex

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

/**
 * @brief Composite a patch onto a texture.
 * @param textureData The texture data to composite onto.
 * @param texWidth The width of the texture.
 * @param texHeight The height of the texture.
 * @param patch The patch data to composite.
 * @param originX The X origin for the patch.
 * @param originY The Y origin for the patch.
 * @param palette The color palette to use for the patch.
 */
void WADConverter::compositePatch(std::vector<unsigned char> &textureData,
                                  int texWidth, int texHeight,
                                  const WAD::PatchData &patch, int originX,
                                  int                            originY,
                                  const std::vector<WAD::Color> &palette) {
  // Validate patch data
  if (patch.pixels.empty() || patch.width <= 0 || patch.height <= 0) {
    OkLogger::error("Invalid patch data for patch " +
                    std::string(patch.name, strnlen(patch.name, 8)));
    return;
  }

  // Validate texture data size
  if (textureData.size() < (size_t)(texWidth * texHeight * 4)) {
    OkLogger::error("Invalid texture data size for patch " +
                    std::string(patch.name, strnlen(patch.name, 8)));
    return;
  }

  // For each pixel in the patch
  for (int x = 0; x < patch.width; x++) {
    int destX = originX + x;
    if (destX < 0 || destX >= texWidth) {
      continue;
    }

    for (int y = 0; y < patch.height; y++) {
      int destY = originY + y;
      if (destY < 0 || destY >= texHeight) {
        continue;
      }

      // Calculate source and destination indices with bounds checking
      int srcIndex = y * patch.width + x;
      if (srcIndex >= (int)patch.pixels.size()) {
        OkLogger::error("Source index out of bounds in patch " +
                        std::string(patch.name, strnlen(patch.name, 8)));
        continue;
      }

      int destIndex = (destY * texWidth + destX) * 4;  // RGBA format
      if (destIndex + 3 >= (int)textureData.size()) {
        OkLogger::error("Destination index out of bounds in patch " +
                        std::string(patch.name, strnlen(patch.name, 8)));
        continue;
      }

      // Get color index and validate
      uint8_t colorIndex = patch.pixels[srcIndex];
      if (colorIndex >= palette.size()) {
        continue;
      }

      // Copy color from palette with opacity check
      const WAD::Color &color = palette[colorIndex];
      if (colorIndex > 0) {  // Index 0 is typically transparent
        textureData[destIndex + 0] = color.r;
        textureData[destIndex + 1] = color.g;
        textureData[destIndex + 2] = color.b;
        textureData[destIndex + 3] = 255;  // Full opacity
      }
    }
  }
}

/**
 * @brief Create an OpenGL texture from a flat (floor/ceiling) texture name.
 * @param flatName The name of the flat texture to create.
 * @param flatData The flat data to use.
 * @param palette The color palette to use.
 */
void WADConverter::createFlatTexture(const std::string             &flatName,
                                     const WAD::FlatData           &flatData,
                                     const std::vector<WAD::Color> &palette) {
  // Check if texture already exists in handler
  if (OkTextureHandler::getInstance()->getTexture(flatName)) {
    return;
  }

  // DOOM flats are always 64x64
  const int FLAT_SIZE    = 64;
  const int TOTAL_PIXELS = FLAT_SIZE * FLAT_SIZE;

  // Validate flat data size
  if ((int)flatData.data.size() != TOTAL_PIXELS) {
    OkLogger::error("Invalid flat size for '" + flatName +
                    "': " + std::to_string(flatData.data.size()) +
                    " (expected " + std::to_string(TOTAL_PIXELS) + ")");
    return;
  }

  // Create texture data (RGBA format)
  std::vector<unsigned char> textureData(TOTAL_PIXELS * 4, 0);

  // Convert flat data to RGBA using the palette
  for (int i = 0; i < TOTAL_PIXELS; i++) {
    uint8_t colorIndex = flatData.data[i];
    if (colorIndex >= palette.size()) {
      continue;
    }

    const WAD::Color &color = palette[colorIndex];
    int               idx   = i * 4;
    textureData[idx + 0]    = color.r;
    textureData[idx + 1]    = color.g;
    textureData[idx + 2]    = color.b;
    textureData[idx + 3]    = 255;  // Full opacity
  }

  // Create the texture through the handler
  OkTextureHandler::getInstance()->createTextureFromRawData(
      flatName, textureData.data(), FLAT_SIZE, FLAT_SIZE, 4);

  OkLogger::info("WADConverter :: Created flat texture '" + flatName +
                 "' (64x64)");
}

/**
 * @brief Create an OpenGL texture from a WAD texture definition.
 * @param texDef The texture definition containing patch information.
 * @param patches The vector of patch data.
 */
void WADConverter::createTextureFromDef(
    const WAD::TextureDef &texDef, const std::vector<WAD::PatchData> &patches,
    const std::vector<WAD::Color> &palette) {

  std::string texName = OkStrings::trimFixedString(texDef.name, 8);

  // Check if texture already exists in handler
  if (OkTextureHandler::getInstance()->getTexture(texName)) {
    return;
  }

  // Basic validation
  if (texDef.width <= 0 || texDef.height <= 0 || palette.empty()) {
    OkLogger::error("Invalid texture definition for " + texName);
    return;
  }

  // Create empty texture data with default color (to handle missing patches)
  std::vector<unsigned char> textureData(texDef.width * texDef.height * 4, 128);

  // Count valid patches
  size_t validPatchCount = 0;
  bool   hasValidPatches = false;

  // For each patch in the texture
  for (int i = 0; i < texDef.patches.size(); i++) {
    const WAD::PatchInTexture &patchInfo = texDef.patches[i];

    // Skip invalid patch indices
    if (patchInfo.patch_num >= patches.size()) {
      OkLogger::warning("Skipping invalid patch index " +
                        std::to_string(patchInfo.patch_num) + " in texture " +
                        texName);
      continue;
    }

    // Get patch data
    const WAD::PatchData &patchData = patches[patchInfo.patch_num];

    // Skip invalid patches but don't fail the texture
    if (patchData.pixels.empty() || patchData.width <= 0 ||
        patchData.height <= 0) {
      OkLogger::warning("Skipping invalid patch data in texture " + texName);
      continue;
    }

    hasValidPatches = true;

    try {
      // Composite patch onto texture at (origin_x, origin_y)
      compositePatch(textureData, texDef.width, texDef.height, patchData,
                     patchInfo.origin_x, patchInfo.origin_y, palette);
      validPatchCount++;
    } catch (const std::exception &e) {
      OkLogger::error("Error compositing patch in texture " + texName + ": " +
                      e.what());
      continue;
    }
  }

  // Create texture even if some patches failed, as long as we have valid data
  if (hasValidPatches) {
    OkLogger::info("WADConverter :: Creating texture '" + texName + "' (" +
                   std::to_string(texDef.width) + "x" +
                   std::to_string(texDef.height) +
                   "), Valid patches: " + std::to_string(validPatchCount) +
                   "/" + std::to_string(texDef.patches.size()));

    // Create texture using the dedicated creation method with pre-trimmed name
    OkTextureHandler::getInstance()->createTextureFromRawData(
        texName, textureData.data(), texDef.width, texDef.height, 4);
  } else {
    OkLogger::error("No valid patches found for texture " + texName +
                    " - texture will not be created");
  }
}

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
