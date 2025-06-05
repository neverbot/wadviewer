#ifndef WAD_VIEWER_WAD_TEXTURES_HPP
#define WAD_VIEWER_WAD_TEXTURES_HPP

#include "wad.hpp"

class WADTextures {
public:
  static void processTextures(const WAD::Level &level);

  static void createTextureFromDef(const WAD::TextureDef             &texDef,
                                   const std::vector<WAD::PatchData> &patches,
                                   const std::vector<WAD::Color>     &palette);

  static void createFlatTexture(const std::string             &flatName,
                                const WAD::FlatData           &flatData,
                                const std::vector<WAD::Color> &palette);

private:
  static void compositePatch(std::vector<unsigned char> &textureData,
                             int texWidth, int texHeight,
                             const WAD::PatchData &patch, int originX,
                             int                            originY,
                             const std::vector<WAD::Color> &palette);
};

#endif  // WAD_VIEWER_WAD_TEXTURES_HPP
