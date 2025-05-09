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
 * @brief Convert a WAD level to an OkItem.
 * @param level The WAD level to convert.
 * @return A pointer to the created OkItem.
 */
OkItem *WADRenderer::createLevelGeometry(const WAD::Level &level) {
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

  // Pre-load all textures needed for this level
  for (const WAD::Sidedef &sidedef : level.sidedefs) {
    // Get texture names from sidedef (upper, middle, lower)
    std::string upperTex(sidedef.upper_texture, 8);
    std::string middleTex(sidedef.middle_texture, 8);
    std::string lowerTex(sidedef.lower_texture, 8);

    // Find corresponding texture definitions
    for (const WAD::TextureDef &texDef : level.texture_defs) {
      std::string texName(texDef.name, 8);
      if (texName == upperTex || texName == middleTex || texName == lowerTex) {
        // Create texture if not already in cache
        if (textureCache.find(texName) == textureCache.end()) {
          // Create OpenGL texture from WAD texture definition
          createTextureFromDef(texDef, level.patches);
        }
      }
    }
  }

  // Convert WAD vertices to OpenGL format, centered around origin
  std::vector<float>        levelVertices;
  std::vector<unsigned int> levelIndices;

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

    // Subtract center to normalize around origin
    float normalizedX = (static_cast<float>(vertex.x) - centerX) * SCALE;
    float normalizedY = (static_cast<float>(vertex.y) - centerY) * SCALE;

    // Add floor vertex
    levelVertices.push_back(normalizedX);  // x
    levelVertices.push_back(
        static_cast<float>(sector->floor_height));  // y (flat map)
    levelVertices.push_back(-normalizedY);  // z (negated for -Z forward)
    levelVertices.push_back(0.0f);          // u texture coord
    levelVertices.push_back(0.0f);          // v texture coord

    // Add ceiling vertex
    levelVertices.push_back(normalizedX);
    levelVertices.push_back(static_cast<float>(sector->ceiling_height));
    levelVertices.push_back(-normalizedY);
    levelVertices.push_back(0.0f);
    levelVertices.push_back(1.0f);
  }

  // Second pass: Create triangles from linedefs
  for (size_t i = 0; i < level.linedefs.size(); i++) {
    const WAD::Linedef &linedef = level.linedefs[i];

    // Only process linedefs that have a right (front) sidedef
    if (linedef.right_sidedef != 0xFFFF) {
      const WAD::Sidedef &leftSide  = level.sidedefs[linedef.left_sidedef];
      const WAD::Sidedef &rightSide = level.sidedefs[linedef.right_sidedef];

      const WAD::Sector &sector1 = level.sectors[leftSide.sector];
      const WAD::Sector &sector2 = level.sectors[rightSide.sector];

      // Create wall if sectors have different heights
      if (sector1.floor_height != sector2.floor_height ||
          sector1.ceiling_height != sector2.ceiling_height) {
        const WAD::Vertex &v1 = level.vertices[linedef.start_vertex];
        const WAD::Vertex &v2 = level.vertices[linedef.end_vertex];
        // Create wall geometry
        createWallFace(v1, v2, sector1, sector2, rightSide, levelVertices,
                       levelIndices);
      }
      // Get the sector this sidedef belongs to
      if (rightSide.sector < level.sectors.size()) {
        const WAD::Sector &sector = level.sectors[rightSide.sector];

        // Create two triangles for each linedef segment
        unsigned int v1 = linedef.start_vertex;
        unsigned int v2 = linedef.end_vertex;

        // Find next linedef that shares v2 as start vertex
        for (size_t j = 0; j < level.linedefs.size(); j++) {
          if (level.linedefs[j].start_vertex == v2) {
            unsigned int v3 = level.linedefs[j].end_vertex;

            // Add triangle indices (CCW winding)
            levelIndices.push_back(v1);
            levelIndices.push_back(v2);
            levelIndices.push_back(v3);
            break;
          }
        }
      }
    }
  }

  OkLogger::info("WADRenderer :: Created geometry with " +
                 std::to_string(levelVertices.size() / 5) + " vertices and " +
                 std::to_string(levelIndices.size() / 3) + " triangles");

  // Create the item with the geometry
  OkItem *levelItem =
      new OkItem("level_geometry", levelVertices.data(), levelVertices.size(),
                 levelIndices.data(), levelIndices.size());

  // Assign first available texture for testing
  if (!textureCache.empty()) {
    levelItem->setTexture(textureCache.begin()->second);
    OkLogger::info("WADRenderer :: Assigned texture: " +
                   textureCache.begin()->first);
  }

  // Log the normalization info
  OkLogger::info("Level bounds: (" + std::to_string(minX) + "," +
                 std::to_string(minY) + ") to (" + std::to_string(maxX) + "," +
                 std::to_string(maxY) + ")");
  OkLogger::info("Level center: (" + std::to_string(centerX) + "," +
                 std::to_string(centerY) + ")");
  OkLogger::info("Level dimensions: " + std::to_string(width) + " x " +
                 std::to_string(height));

  return levelItem;
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

  // Get ceiling and floor heights
  float floor1 = static_cast<float>(sector1.floor_height);
  float ceil1  = static_cast<float>(sector1.ceiling_height);
  float floor2 = static_cast<float>(sector2.floor_height);
  float ceil2  = static_cast<float>(sector2.ceiling_height);

  // Calculate wall height and length for texture mapping
  float wallHeight = ceil1 - floor1;
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

  // Get the appropriate texture based on wall type
  std::string textureName;
  if (sector1.ceiling_height > sector2.ceiling_height) {
    textureName = std::string(sidedef.upper_texture, 8);
  } else if (sector1.floor_height < sector2.floor_height) {
    textureName = std::string(sidedef.lower_texture, 8);
  } else {
    textureName = std::string(sidedef.middle_texture, 8);
  }

  // Find texture in cache and bind it
  auto texIt = textureCache.find(textureName);
  if (texIt != textureCache.end()) {
    texIt->second->bind();
  }

  // Add vertices for the wall quad with proper texture coordinates
  // Bottom left
  vertices.push_back(x1);
  vertices.push_back(floor1);
  vertices.push_back(-z1);
  vertices.push_back(u1);
  vertices.push_back(v1);

  // Top left
  vertices.push_back(x1);
  vertices.push_back(ceil1);
  vertices.push_back(-z1);
  vertices.push_back(u1);
  vertices.push_back(v2);

  // Bottom right
  vertices.push_back(x2);
  vertices.push_back(floor2);
  vertices.push_back(-z2);
  vertices.push_back(u2);
  vertices.push_back(v1);

  // Top right
  vertices.push_back(x2);
  vertices.push_back(ceil2);
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

void WADRenderer::compositePatch(std::vector<unsigned char> &textureData,
                                 int texWidth, int texHeight,
                                 const WAD::PatchData &patch, int originX,
                                 int originY) {
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

      // Get color index from patch data
      size_t srcIndex = y * patch.width + x;
      if (srcIndex < patch.pixels.size()) {
        uint8_t colorIndex = patch.pixels[srcIndex];

        // Skip transparent pixels (index 255 in DOOM)
        if (colorIndex == 255) {
          continue;
        }

        // Calculate destination pixel position
        size_t destIndex = (destY * texWidth + destX) * 4;

        // For now, use grayscale until we implement palette
        textureData[destIndex + 0] = colorIndex;  // R
        textureData[destIndex + 1] = colorIndex;  // G
        textureData[destIndex + 2] = colorIndex;  // B
        textureData[destIndex + 3] = 255;         // A
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
    const WAD::TextureDef &texDef, const std::vector<WAD::PatchData> &patches) {
  // Create empty texture of required size
  std::vector<unsigned char> textureData(texDef.width * texDef.height * 4,
                                         0);  // RGBA format

  // For each patch in the texture
  for (const WAD::PatchInTexture &patch : texDef.patches) {
    // Get patch data
    const WAD::PatchData &patchData = patches[patch.patch_num];

    // Composite patch onto texture at (origin_x, origin_y)
    compositePatch(textureData, texDef.width, texDef.height, patchData,
                   patch.origin_x, patch.origin_y);
  }

  // Trim texture name to remove trailing spaces
  std::string texName(texDef.name);
  while (!texName.empty() && texName[texName.size() - 1] == ' ') {
    texName.resize(texName.size() - 1);
  }

  // Create OpenGL texture
  OkTexture *texture = new OkTexture(texName);

  if (!texture->createFromRawData(textureData.data(), texDef.width,
                                  texDef.height, GL_RGBA)) {
    delete texture;
    OkLogger::error("WADRenderer :: Failed to create texture: " + texName);
    return;
  }

  // Add to cache
  textureCache[texName] = texture;

  OkLogger::info("WADRenderer :: Created texture: " + texName + " (" +
                 std::to_string(texDef.width) + "x" +
                 std::to_string(texDef.height) + ")");
}
