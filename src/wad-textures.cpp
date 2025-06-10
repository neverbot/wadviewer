#include "wad-textures.hpp"
#include "okinawa/handlers/textures.hpp"
#include "okinawa/utils/logger.hpp"
#include "okinawa/utils/strings.hpp"

/**
 * @brief Process all textures in a WAD level.
 * @param level The WAD level containing texture data.
 */
void WADTextures::processTextures(const WAD::Level &level) {
  // Process all flat textures
  for (size_t i = 0; i < level.flats.size(); i++) {
    const WAD::FlatData &flat = level.flats[i];
    createFlatTexture(flat.name, flat, level.palette);
  }

  // Process all texture definitions
  for (size_t i = 0; i < level.texture_defs.size(); i++) {
    const WAD::TextureDef &texDef = level.texture_defs[i];
    createTextureFromDef(texDef, level.patches, level.palette);
  }
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
void WADTextures::compositePatch(std::vector<unsigned char> &textureData,
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
  if (textureData.size() <
      static_cast<size_t>(texWidth) * static_cast<size_t>(texHeight) * 4) {
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

      // Calculate source and destination indices with proper widening
      size_t srcIndex =
          (static_cast<size_t>(y) * static_cast<size_t>(patch.width) +
           static_cast<size_t>(x)) *
          4;  // RGBA format in source
      if (srcIndex >= patch.pixels.size()) {
        OkLogger::error("Source index out of bounds in patch " +
                        std::string(patch.name, strnlen(patch.name, 8)));
        continue;
      }

      size_t destIndex =
          (static_cast<size_t>(destY) * static_cast<size_t>(texWidth) +
           static_cast<size_t>(destX)) *
          4;  // RGBA format
      if (destIndex + 3 >= textureData.size()) {
        OkLogger::error("Destination index out of bounds in patch " +
                        std::string(patch.name, strnlen(patch.name, 8)));
        continue;
      }

      // Get palette index from red channel where we stored it
      uint8_t colorIndex = patch.pixels[srcIndex];
      if (colorIndex >= palette.size()) {
        continue;
      }

      // Skip fully transparent pixels (index 0 in DOOM palette)
      if (colorIndex == 0) {
        continue;
      }

      // Copy color from palette
      const WAD::Color &color    = palette[colorIndex];
      textureData[destIndex + 0] = color.r;
      textureData[destIndex + 1] = color.g;
      textureData[destIndex + 2] = color.b;
      textureData[destIndex + 3] = 255;  // Full opacity
    }
  }
}

/**
 * @brief Create an OpenGL texture from a flat (floor/ceiling) texture name.
 * @param flatName The name of the flat texture to create.
 * @param flatData The flat data to use.
 * @param palette The color palette to use.
 */
void WADTextures::createFlatTexture(const std::string             &flatName,
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
void WADTextures::createTextureFromDef(
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
