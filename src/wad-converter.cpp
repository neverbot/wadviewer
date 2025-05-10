#include "wad-converter.hpp"
#include "../okinawa.cpp/src/handlers/textures.hpp"
#include "../okinawa.cpp/src/utils/logger.hpp"
#include "../okinawa.cpp/src/utils/strings.hpp"
#include <cmath>
#include <limits>

float       WADConverter::centerX = 0.0f;
float       WADConverter::centerY = 0.0f;
const float WADConverter::SCALE =
    1.0f;  // Scale down the geometry to make it more manageable

WADConverter::WADConverter() {
  // Empty constructor
}

WADConverter::~WADConverter() {
  // Nothing to clean up - TextureHandler handles texture cleanup
}

/**
 * @brief Creates all the geometry for a level.
 * @param level The level to create geometry for.
 * @return A vector of OkItem pointers representing the level geometry.
 */
std::vector<OkItem *>
WADConverter::createLevelGeometry(const WAD::Level &level) {
  std::vector<OkItem *> items;

  // Calculate level bounds
  float minX = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float minY = std::numeric_limits<float>::max();
  float maxY = std::numeric_limits<float>::lowest();

  for (size_t i = 0; i < level.vertices.size(); ++i) {
    const WAD::Vertex &vertex = level.vertices[i];
    minX                      = std::min(minX, static_cast<float>(vertex.x));
    maxX                      = std::max(maxX, static_cast<float>(vertex.x));
    minY                      = std::min(minY, static_cast<float>(vertex.y));
    maxY                      = std::max(maxY, static_cast<float>(vertex.y));
  }

  // Calculate center point and dimensions
  centerX = (minX + maxX) / 2.0f;
  centerY = (minY + maxY) / 2.0f;

  // Optional: Calculate scale to normalize size
  float width        = maxX - minX;
  float height       = maxY - minY;
  float maxDimension = std::max(width, height);

  // Log texture and level info in single lines
  OkLogger::info("WADConverter :: Level info - Textures: " +
                 std::to_string(level.texture_defs.size()) +
                 ", Patches: " + std::to_string(level.patches.size()) +
                 ", Colors: " + std::to_string(level.palette.size()));

  if (level.texture_defs.empty()) {
    OkLogger::error("No texture definitions found in level!");
    for (size_t i = 0; i < 5 && i < level.sidedefs.size(); i++) {
      const WAD::Sidedef &sidedef = level.sidedefs[i];
      std::string upper = OkStrings::trimFixedString(sidedef.upper_texture, 8);
      std::string middle =
          OkStrings::trimFixedString(sidedef.middle_texture, 8);
      std::string lower = OkStrings::trimFixedString(sidedef.lower_texture, 8);
      OkLogger::info("WADConverter :: Sidedef " + std::to_string(i) +
                     " textures: " + upper + ", " + middle + ", " + lower);
    }
  }

  // First, load all textures needed for this level
  for (const WAD::Sidedef &sidedef : level.sidedefs) {
    // Get texture names from sidedef (upper, middle, lower)
    std::string upperTex = OkStrings::trimFixedString(sidedef.upper_texture, 8);
    std::string middleTex =
        OkStrings::trimFixedString(sidedef.middle_texture, 8);
    std::string lowerTex = OkStrings::trimFixedString(sidedef.lower_texture, 8);

    // Find corresponding texture definitions
    for (const WAD::TextureDef &texDef : level.texture_defs) {
      std::string texName = OkStrings::trimFixedString(texDef.name, 8);

      if (!texName.empty() && (texName == upperTex || texName == middleTex ||
                               texName == lowerTex)) {
        createTextureFromDef(texDef, level.patches, level.palette);
      }
    }
  }

  // Structure to hold geometry for each texture
  struct GeometryGroup {
    std::vector<float>        vertices;
    std::vector<unsigned int> indices;
    std::string               textureName;
  };
  std::map<std::string, GeometryGroup> geometryGroups;

  // Log level stats before processing
  OkLogger::info("WADConverter :: Level '" + level.name +
                 "' - Vertices: " + std::to_string(level.vertices.size()) +
                 ", Linedefs: " + std::to_string(level.linedefs.size()) +
                 ", Sectors: " + std::to_string(level.sectors.size()));

  // First, create a map to store vertex-to-sector relationships
  std::vector<const WAD::Sector *> vertexSectors(level.vertices.size(),
                                                 nullptr);

  // Find which sector each vertex belongs to through linedefs
  for (size_t i = 0; i < level.linedefs.size(); i++) {
    const WAD::Linedef &linedef = level.linedefs[i];

    if (linedef.right_sidedef != 0xFFFF) {
      const WAD::Sidedef &sidedef = level.sidedefs[linedef.right_sidedef];
      if (sidedef.sector < level.sectors.size()) {
        const WAD::Sector *sector           = &level.sectors[sidedef.sector];
        vertexSectors[linedef.start_vertex] = sector;
        vertexSectors[linedef.end_vertex]   = sector;
      }
    }
  }

  // Now create vertices with their proper sector heights
  for (size_t i = 0; i < level.vertices.size(); i++) {
    const WAD::Vertex &vertex = level.vertices[i];
    const WAD::Sector *sector = vertexSectors[i];

    // Skip vertices that don't belong to any sector
    if (!sector) {
      continue;
    }

    // Create geometry group for floor and ceiling
    GeometryGroup &floorGroup =
        geometryGroups["F_FLAT"];  // Floor texture group
    GeometryGroup &ceilGroup =
        geometryGroups["C_FLAT"];  // Ceiling texture group

    // Subtract center to normalize around origin
    float normalizedX = (static_cast<float>(vertex.x) - centerX) * SCALE;
    float normalizedY = (static_cast<float>(vertex.y) - centerY) * SCALE;

    // Add floor vertex to floor group
    floorGroup.vertices.push_back(normalizedX);  // x
    floorGroup.vertices.push_back(
        static_cast<float>(sector->floor_height));  // y
    floorGroup.vertices.push_back(-normalizedY);  // z (negated for -Z forward)
    floorGroup.vertices.push_back(0.0f);          // u texture coord
    floorGroup.vertices.push_back(0.0f);          // v texture coord

    // Add ceiling vertex to ceiling group
    ceilGroup.vertices.push_back(normalizedX);
    ceilGroup.vertices.push_back(static_cast<float>(sector->ceiling_height));
    ceilGroup.vertices.push_back(-normalizedY);
    ceilGroup.vertices.push_back(0.0f);
    ceilGroup.vertices.push_back(1.0f);

    // Store texture names
    floorGroup.textureName = "F_FLAT";
    ceilGroup.textureName  = "C_FLAT";
  }

  // Create triangles from linedefs, grouped by texture
  for (size_t i = 0; i < level.linedefs.size(); i++) {
    const WAD::Linedef &linedef = level.linedefs[i];

    if (linedef.right_sidedef != 0xFFFF) {
      const WAD::Sidedef &rightSide = level.sidedefs[linedef.right_sidedef];
      const WAD::Sidedef &leftSide  = level.sidedefs[linedef.left_sidedef];
      const WAD::Sector  &sector1   = level.sectors[leftSide.sector];
      const WAD::Sector  &sector2   = level.sectors[rightSide.sector];

      // Get texture name based on wall type
      std::string textureName;
      if (sector1.ceiling_height > sector2.ceiling_height) {
        textureName = OkStrings::trimFixedString(rightSide.upper_texture, 8);
      } else if (sector1.floor_height < sector2.floor_height) {
        textureName = OkStrings::trimFixedString(rightSide.lower_texture, 8);
      } else {
        textureName = OkStrings::trimFixedString(rightSide.middle_texture, 8);
      }

      // Skip invalid/empty texture names
      if (textureName.empty() || textureName == "-") {
        continue;
      }

      // Get or create geometry group for this texture
      GeometryGroup &group = geometryGroups[textureName];
      group.textureName    = textureName;

      // Add geometry to the appropriate group
      const WAD::Vertex &v1 = level.vertices[linedef.start_vertex];
      const WAD::Vertex &v2 = level.vertices[linedef.end_vertex];
      createWallFace(v1, v2, sector1, sector2, rightSide, group.vertices,
                     group.indices);
    }
  }

  // Create items from geometry groups
  for (std::map<std::string, GeometryGroup>::iterator it =
           geometryGroups.begin();
       it != geometryGroups.end(); ++it) {
    const GeometryGroup &group = it->second;

    if (group.vertices.empty() || group.indices.empty()) {
      continue;
    }

    std::string   itemName   = "level_" + group.textureName;
    float        *vertexData = new float[group.vertices.size()];
    unsigned int *indexData  = new unsigned int[group.indices.size()];

    // Copy vertex and index data
    for (size_t i = 0; i < group.vertices.size(); ++i) {
      vertexData[i] = group.vertices[i];
    }
    for (size_t i = 0; i < group.indices.size(); ++i) {
      indexData[i] = group.indices[i];
    }

    OkItem *item = new OkItem(itemName, vertexData, group.vertices.size(),
                              indexData, group.indices.size());

    // Look up texture using the already trimmed name from the geometry group
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

  OkLogger::info("Created " + std::to_string(items.size()) +
                 " geometry groups with textures");

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
    OkLogger::error("Invalid patch data for patch " + patch.name);
    return;
  }

  // Validate texture data size
  if (textureData.size() < (size_t)(texWidth * texHeight * 4)) {
    OkLogger::error("Invalid texture data size for patch " + patch.name);
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
      size_t srcIndex = (y * patch.width + x);
      if (srcIndex >= patch.pixels.size() || srcIndex < 0) {
        OkLogger::error("Source index out of bounds in patch " + patch.name);
        continue;
      }

      size_t destIndex = ((size_t)destY * texWidth + destX) * 4;  // RGBA format
      if (destIndex + 3 >= textureData.size()) {
        OkLogger::error("Destination index out of bounds in patch " +
                        patch.name);
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

      // // Debug first few pixels
      // if (x < 3 && y < 3) {
      //   OkLogger::info("Pixel color at " + std::to_string(x) + "," +
      //                  std::to_string(y) + ": " + std::to_string(color.r) +
      //                  "," + std::to_string(color.g) + "," +
      //                  std::to_string(color.b));
      // }
    }
  }
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
  for (size_t i = 0; i < texDef.patches.size(); i++) {
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
