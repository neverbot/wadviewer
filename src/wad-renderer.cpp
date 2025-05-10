#include "wad-renderer.hpp"
#include "../okinawa.cpp/src/utils/logger.hpp"
#include <cmath>
#include <limits>

float       WADRenderer::centerX = 0.0f;
float       WADRenderer::centerY = 0.0f;
const float WADRenderer::SCALE   = 1.0f;

WADRenderer::WADRenderer() {
  // Constructor
}

WADRenderer::~WADRenderer() {
  // Clean up textures
  for (std::map<std::string, OkTexture *>::iterator it = textureCache.begin();
       it != textureCache.end(); ++it) {
    delete it->second;
  }
  textureCache.clear();
}

/**
 * @brief Creates all the geometry for a level.
 * @param level The level to create geometry for.
 * @return A vector of OkItem pointers representing the level geometry.
 */
std::vector<OkItem *>
WADRenderer::createLevelGeometry(const WAD::Level &level) {
  // Calculate level bounds
  float minX = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float minY = std::numeric_limits<float>::max();
  float maxY = std::numeric_limits<float>::lowest();

  for (const auto &vertex : level.vertices) {
    minX = std::min(minX, static_cast<float>(vertex.x));
    maxX = std::max(maxX, static_cast<float>(vertex.x));
    minY = std::min(minY, static_cast<float>(vertex.y));
    maxY = std::max(maxY, static_cast<float>(vertex.y));
  }

  // Calculate center point and dimensions
  centerX = (minX + maxX) / 2.0f;
  centerY = (minY + maxY) / 2.0f;

  // Optional: Calculate scale to normalize size
  float width        = maxX - minX;
  float height       = maxY - minY;
  float maxDimension = std::max(width, height);

  // Debug texture definitions
  OkLogger::info("WAD texture info:");
  OkLogger::info("- Texture definitions: " +
                 std::to_string(level.texture_defs.size()));
  OkLogger::info("- Patches available: " +
                 std::to_string(level.patches.size()));
  OkLogger::info("- Palette colors: " + std::to_string(level.palette.size()));

  if (level.texture_defs.empty()) {
    OkLogger::error("No texture definitions found in level!");
    // Print first few sidedefs textures for debugging
    for (size_t i = 0; i < 5 && i < level.sidedefs.size(); i++) {
      const WAD::Sidedef &sidedef = level.sidedefs[i];
      OkLogger::info("Sidedef " + std::to_string(i) + " textures:");
      OkLogger::info("- Upper: " + std::string(sidedef.upper_texture, 8));
      OkLogger::info("- Middle: " + std::string(sidedef.middle_texture, 8));
      OkLogger::info("- Lower: " + std::string(sidedef.lower_texture, 8));
    }
  }

  // Pre-load all textures needed for this level
  for (const WAD::Sidedef &sidedef : level.sidedefs) {
    // Get texture names from sidedef (upper, middle, lower)
    std::string upperTex(sidedef.upper_texture, 8);
    std::string middleTex(sidedef.middle_texture, 8);
    std::string lowerTex(sidedef.lower_texture, 8);

    // Trim trailing spaces
    while (!upperTex.empty() && upperTex.back() == ' ')
      upperTex.pop_back();
    while (!middleTex.empty() && middleTex.back() == ' ')
      middleTex.pop_back();
    while (!lowerTex.empty() && lowerTex.back() == ' ')
      lowerTex.pop_back();

    // OkLogger::info("WADRenderer :: Processing textures - Upper: '" + upperTex
    // +
    //                "' Middle: '" + middleTex + "' Lower: '" + lowerTex +
    //                "'");

    // Find corresponding texture definitions
    for (const WAD::TextureDef &texDef : level.texture_defs) {
      std::string texName(texDef.name, 8);
      while (!texName.empty() && texName.back() == ' ')
        texName.pop_back();

      if (!texName.empty() && (texName == upperTex || texName == middleTex ||
                               texName == lowerTex)) {
        // Create texture if not already in cache
        if (textureCache.find(texName) == textureCache.end()) {
          OkLogger::info("Found matching texture: '" + texName + "'");
          createTextureFromDef(texDef, level.patches, level.palette);
        }
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
  OkLogger::info("WADRenderer :: Creating geometry for level: " + level.name);
  OkLogger::info("WADRenderer :: Vertices: " +
                 std::to_string(level.vertices.size()));
  OkLogger::info("WADRenderer :: Linedefs: " +
                 std::to_string(level.linedefs.size()));
  OkLogger::info("WADRenderer :: Sectors: " +
                 std::to_string(level.sectors.size()));

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
        textureName = std::string(rightSide.upper_texture, 8);
      } else if (sector1.floor_height < sector2.floor_height) {
        textureName = std::string(rightSide.lower_texture, 8);
      } else {
        textureName = std::string(rightSide.middle_texture, 8);
      }

      // Trim texture name
      while (!textureName.empty() && textureName.back() == ' ') {
        textureName.pop_back();
      }

      if (!textureName.empty()) {
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
  }

  // Create items from geometry groups
  std::vector<OkItem *> items;

  for (std::map<std::string, GeometryGroup>::iterator it =
           geometryGroups.begin();
       it != geometryGroups.end(); ++it) {
    const GeometryGroup &group = it->second;

    if (group.vertices.empty() || group.indices.empty()) {
      continue;
    }

    // Create item for this geometry group
    std::string itemName = "level_" + group.textureName;
    // Create non-const copies of the vertex and index data
    float        *vertexData = new float[group.vertices.size()];
    unsigned int *indexData  = new unsigned int[group.indices.size()];

    // Copy the data
    for (size_t i = 0; i < group.vertices.size(); ++i) {
      vertexData[i] = group.vertices[i];
    }
    for (size_t i = 0; i < group.indices.size(); ++i) {
      indexData[i] = group.indices[i];
    }

    // Create item with the copied data
    OkItem *item = new OkItem(itemName,
                              vertexData,             // float* vertexData
                              group.vertices.size(),  // long vertexCount
                              indexData,              // unsigned int* indexData
                              group.indices.size());  // long indexCount

    // Assign texture from cache
    std::map<std::string, OkTexture *>::iterator texIt =
        textureCache.find(group.textureName);
    if (texIt != textureCache.end()) {
      item->setTexture(texIt->second);
      OkLogger::info("Assigned texture '" + group.textureName + "' to item '" +
                     itemName + "'");
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
void WADRenderer::createWallFace(const WAD::Vertex         &vertex1,
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

  // Get ceiling and floor heights for both sectors
  float floor1 = static_cast<float>(sector1.floor_height);
  float ceil1  = static_cast<float>(sector1.ceiling_height);
  float floor2 = static_cast<float>(sector2.floor_height);
  float ceil2  = static_cast<float>(sector2.ceiling_height);

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
  float v2 =
      v1 + (wallHeight / TEXTURE_HEIGHT);  // Texture repeats along height

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
void WADRenderer::compositePatch(std::vector<unsigned char> &textureData,
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

      // Debug first few pixels
      if (x < 3 && y < 3) {
        OkLogger::info("Pixel color at " + std::to_string(x) + "," +
                       std::to_string(y) + ": " + std::to_string(color.r) +
                       "," + std::to_string(color.g) + "," +
                       std::to_string(color.b));
      }
    }
  }
}

/**
 * @brief Create an OpenGL texture from a WAD texture definition.
 * @param texDef The texture definition containing patch information.
 * @param patches The vector of patch data.
 */
void WADRenderer::createTextureFromDef(
    const WAD::TextureDef &texDef, const std::vector<WAD::PatchData> &patches,
    const std::vector<WAD::Color> &palette) {

  // Get texture name and trim it
  std::string texName(texDef.name);
  while (!texName.empty() && texName[texName.size() - 1] == ' ') {
    texName.resize(texName.size() - 1);
  }

  OkLogger::info("Creating texture: " + texName);
  OkLogger::info("Texture size: " + std::to_string(texDef.width) + "x" +
                 std::to_string(texDef.height));
  OkLogger::info("Patch count: " + std::to_string(texDef.patches.size()));
  OkLogger::info("Palette size: " + std::to_string(palette.size()));

  // Basic validation
  if (texDef.width <= 0 || texDef.height <= 0 || texDef.patches.empty() ||
      palette.empty()) {
    OkLogger::error("Invalid texture definition for " + texName);
    return;
  }

  // Create empty texture of required size
  std::vector<unsigned char> textureData(texDef.width * texDef.height * 4, 0);

  // For each patch in the texture
  for (size_t i = 0; i < texDef.patches.size(); i++) {
    const WAD::PatchInTexture &patchInfo = texDef.patches[i];

    // Validate patch index
    if (patchInfo.patch_num >= patches.size()) {
      OkLogger::error("Invalid patch index " +
                      std::to_string(patchInfo.patch_num) + " for texture " +
                      texName);
      continue;
    }

    // Get patch data
    const WAD::PatchData &patchData = patches[patchInfo.patch_num];

    // Skip invalid patches
    if (patchData.pixels.empty() || patchData.width <= 0 ||
        patchData.height <= 0) {
      OkLogger::error("Invalid patch data in texture " + texName);
      continue;
    }

    try {
      // Composite patch onto texture at (origin_x, origin_y)
      compositePatch(textureData, texDef.width, texDef.height, patchData,
                     patchInfo.origin_x, patchInfo.origin_y, palette);
    } catch (const std::exception &e) {
      OkLogger::error("Error compositing patch in texture " + texName + ": " +
                      e.what());
      continue;
    }
  }

  // Create OpenGL texture even if some patches failed
  OkTexture *texture = new OkTexture(texName);

  if (!texture->createFromRawData(textureData.data(), texDef.width,
                                  texDef.height, GL_RGBA)) {
    OkLogger::error("Failed to create OpenGL texture: " + texName);
    delete texture;
    return;
  }

  // Add to cache even if it's partially broken
  textureCache[texName] = texture;
  OkLogger::info("Created texture: " + texName + " (" +
                 std::to_string(texDef.width) + "x" +
                 std::to_string(texDef.height) + ")");
}
