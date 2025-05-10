#include "wad.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>

/**
 * @brief WAD constructor
 * @param filepath Path to the WAD file
 * @throws std::runtime_error if the file cannot be opened or is not a valid WAD
 * file
 */
WAD::WAD(const std::string &filepath, bool verbose) {
  filepath_ = filepath;
  verbose_  = verbose;

  std::ifstream file(filepath_, std::ios::binary);

  if (!file) {
    throw std::runtime_error("Unable to open WAD file: " + filepath_);
  }

  // Read header
  file.read(reinterpret_cast<char *>(&header_), sizeof(Header));
  if (!file) {
    throw std::runtime_error("Unable to read WAD header");
  }

  // Verify WAD type
  std::string id(header_.identification, 4);
  if (id != "IWAD" && id != "PWAD") {
    throw std::runtime_error("Not a valid WAD file");
  }

  if (verbose_) {
    std::cout << "WAD type: " << id << std::endl;
    std::cout << "Num lumps: " << header_.numlumps << std::endl;
  }

  // Read directory
  readDirectory();
}

/**
 * @brief Read the WAD directory
 * @throws std::runtime_error if the directory cannot be read
 */
void WAD::readDirectory() {
  // Open the WAD file in binary mode.
  std::ifstream file(filepath_, std::ios::binary);

  // Move the file read position to the start of the directory, using the
  // offset from the header (header_.infotableofs).
  file.seekg(header_.infotableofs);

  // Prepare the directory_ vector to hold all directory entries. The number of
  // entries is specified in the header (header_.numlumps).
  directory_.resize(header_.numlumps);

  // Read the entire directory into memory: each lump has a fixed-size record
  // (16 bytes), and we read all of them at once.
  file.read(reinterpret_cast<char *>(directory_.data()),
            header_.numlumps * sizeof(Directory));
}

bool WAD::isLevelMarker(const std::string &name) const {
  // Clean the name first
  std::string cleanName;
  for (size_t i = 0; i < name.length() && name[i] != '\0'; i++) {
    if (name[i] >= 32 && name[i] <= 126) {  // Printable ASCII only
      cleanName += name[i];
    }
  }

  // Remove trailing spaces
  while (!cleanName.empty() && cleanName.back() == ' ') {
    cleanName.pop_back();
  }

  // DOOM 1 level names are ExMy (x = episode, y = mission)
  if (cleanName.length() == 4 && cleanName[0] == 'E' && cleanName[2] == 'M' &&
      std::isdigit(cleanName[1]) && std::isdigit(cleanName[3])) {
    std::cout << "WAD :: Found DOOM1 level in WAD file: " << cleanName << "\n";
    return true;
  }

  // DOOM 2 level names are MAPxx (xx = 01-32)
  if (cleanName.length() == 5 && cleanName.substr(0, 3) == "MAP" &&
      std::isdigit(cleanName[3]) && std::isdigit(cleanName[4])) {
    std::cout << "WAD :: Found DOOM2 level in WAD file: " << cleanName << "\n";
    return true;
  }

  return false;
}

/**
 * @brief Find a lump by name
 * @param name Lump name
 * @param offset Offset of the lump in the file
 * @param size Size of the lump
 * @param startIndex Index to start searching from
 * @return true if the lump is found, false otherwise
 */
bool WAD::findLump(const std::string &name, uint32_t &offset, uint32_t &size,
                   size_t startIndex = 0) const {
  for (size_t i = startIndex; i < directory_.size(); i++) {
    std::string lumpName(directory_[i].name, strnlen(directory_[i].name, 8));

    // Stop searching for level data at next level marker
    if (name == "VERTEXES" || name == "LINEDEFS" || name == "SIDEDEFS" ||
        name == "SECTORS" || name == "THINGS") {
      // Only stop if we're after a level marker and find another one
      if (i > startIndex && isLevelMarker(lumpName)) {
        break;
      }
    }

    if (lumpName == name) {
      offset = directory_[i].filepos;
      size   = directory_[i].size;
      return true;
    }
  }

  return false;
}

/**
 * @brief Read a lump from the WAD file
 * @param offset Offset of the lump in the file
 * @param size Size of the lump
 * @return Vector containing the lump data
 * @throws std::runtime_error if the lump cannot be read
 */
std::vector<uint8_t> WAD::readLump(std::streamoff offset, std::size_t size) {
  std::vector<uint8_t> data(size);
  std::ifstream        file(filepath_, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Unable to open file: " + filepath_);
  }
  file.seekg(offset);
  file.read(reinterpret_cast<char *>(data.data()), size);
  return data;
}

/**
 * @brief Read vertices from the WAD file
 * @param offset Offset of the vertices in the file
 * @param size Size of the vertices
 * @return Vector containing the vertices
 */
std::vector<WAD::Vertex> WAD::readVertices(std::streamoff offset,
                                           std::size_t    size) {
  auto                data = readLump(offset, size);
  std::vector<Vertex> vertices(size / sizeof(Vertex));
  std::memcpy(vertices.data(), data.data(), size);
  return vertices;
}

/**
 * @brief Read linedefs from the WAD file
 * @param offset Offset of the linedefs in the file
 * @param size Size of the linedefs
 * @return Vector containing the linedefs
 */
std::vector<WAD::Linedef> WAD::readLinedefs(std::streamoff offset,
                                            std::size_t    size) {
  auto                 data = readLump(offset, size);
  std::vector<Linedef> linedefs(size / sizeof(Linedef));
  std::memcpy(linedefs.data(), data.data(), size);
  return linedefs;
}

/**
 * @brief Read sidedefs from the WAD file
 * @param offset Offset of the sidedefs in the file
 * @param size Size of the sidedefs
 * @return Vector containing the sidedefs
 */
std::vector<WAD::Sidedef> WAD::readSidedefs(std::streamoff offset,
                                            std::size_t    size) {
  auto                 data = readLump(offset, size);
  std::vector<Sidedef> sidedefs(size / sizeof(Sidedef));
  std::memcpy(sidedefs.data(), data.data(), size);
  return sidedefs;
}

/**
 * @brief Read sectors from the WAD file
 * @param offset Offset of the sectors in the file
 * @param size Size of the sectors
 * @return Vector containing the sectors
 */
std::vector<WAD::Sector> WAD::readSectors(std::streamoff offset,
                                          std::size_t    size) {
  auto                data = readLump(offset, size);
  std::vector<Sector> sectors(size / sizeof(Sector));
  std::memcpy(sectors.data(), data.data(), size);
  return sectors;
}

/**
 * @brief Read things from the WAD file
 * @param offset Offset of the things in the file
 * @param size Size of the things
 * @return Vector containing the things
 */
std::vector<WAD::Thing> WAD::readThings(std::streamoff offset,
                                        std::size_t    size) {
  auto               data = readLump(offset, size);
  std::vector<Thing> things(size / sizeof(Thing));
  std::memcpy(things.data(), data.data(), size);
  return things;
}

/**
 * @brief Read a patch lump and convert it to RGBA format
 * @param offset Offset of the patch in the file
 * @param size Size of the patch
 * @param name Name of the patch
 * @return PatchData containing the converted patch
 */
WAD::PatchData WAD::readPatch(std::streamoff offset, std::size_t size,
                              const std::string &name) {
  auto      data = readLump(offset, size);
  PatchData patch;
  patch.name = name;

  // Read patch header
  const PatchHeader *header =
      reinterpret_cast<const PatchHeader *>(data.data());
  patch.width  = header->width;
  patch.height = header->height;

  // Initialize pixel data (RGBA format)
  patch.pixels.resize(patch.width * patch.height * 4, 0);

  // Read column offsets
  const uint32_t *columnOffsets = &header->column_offsets[0];

  // Process each column
  for (int x = 0; x < patch.width; x++) {
    uint32_t       columnOffset = columnOffsets[x];
    const uint8_t *column       = data.data() + columnOffset;

    while (true) {
      uint8_t topdelta = *column++;
      if (topdelta == 0xFF)  // End of column
        break;

      uint8_t length = *column++;
      column++;  // Skip padding byte

      // Copy pixels to RGBA format
      for (int y = 0; y < length; y++) {
        uint8_t pixel     = *column++;
        int     destIndex = ((topdelta + y) * patch.width + x) * 4;

        // Convert palette index to RGBA (placeholder values for now)
        patch.pixels[destIndex + 0] = pixel;  // R
        patch.pixels[destIndex + 1] = pixel;  // G
        patch.pixels[destIndex + 2] = pixel;  // B
        patch.pixels[destIndex + 3] = 255;    // A
      }

      column++;  // Skip padding byte
    }
  }

  return patch;
}

/**
 * @brief Read patch names from the WAD file
 * @param offset Offset of the patch names in the file
 * @param size Size of the patch names
 * @return Vector containing the patch names
 */
std::vector<std::string> WAD::readPatchNames(std::streamoff offset,
                                             std::size_t    size) {
  auto                     data = readLump(offset, size);
  std::vector<std::string> names;

  // First 4 bytes is number of patches
  uint32_t num_patches;
  std::memcpy(&num_patches, data.data(), sizeof(uint32_t));

  // Read patch names (8 bytes each, zero-terminated)
  const char *name_data = reinterpret_cast<const char *>(data.data() + 4);
  for (uint32_t i = 0; i < num_patches; i++) {
    names.push_back(
        std::string(name_data + i * 8, strnlen(name_data + i * 8, 8)));
  }

  return names;
}

/**
 * @brief Read texture definitions from the WAD file
 * @param offset Offset of the texture definitions in the file
 * @param size Size of the texture definitions
 * @return Vector containing the texture definitions
 */
std::vector<WAD::TextureDef> WAD::readTextureDefs(std::streamoff offset,
                                                  std::size_t    size) {
  auto                    data = readLump(offset, size);
  std::vector<TextureDef> textures;

  // First 4 bytes is number of textures
  uint32_t num_textures;
  std::memcpy(&num_textures, data.data(), sizeof(uint32_t));

  // Get offsets to each texture
  std::vector<uint32_t> offsets(num_textures);
  std::memcpy(offsets.data(), data.data() + 4, num_textures * sizeof(uint32_t));

  // Read each texture definition
  for (uint32_t i = 0; i < num_textures; i++) {
    TextureDef     tex;
    const uint8_t *tex_data = data.data() + offsets[i];

    // Read texture header
    std::memcpy(tex.name, tex_data, 8);
    std::memcpy(&tex.masked, tex_data + 8, 4);
    std::memcpy(&tex.width, tex_data + 12, 2);
    std::memcpy(&tex.height, tex_data + 14, 2);
    std::memcpy(&tex.column_dir, tex_data + 16, 4);
    std::memcpy(&tex.patch_count, tex_data + 20, 2);

    // Read patches
    const uint8_t *patch_data = tex_data + 22;
    for (uint16_t j = 0; j < tex.patch_count; j++) {
      PatchInTexture patch;
      std::memcpy(&patch.origin_x, patch_data + j * 10, 2);
      std::memcpy(&patch.origin_y, patch_data + j * 10 + 2, 2);
      std::memcpy(&patch.patch_num, patch_data + j * 10 + 4, 2);
      std::memcpy(&patch.stepdir, patch_data + j * 10 + 6, 2);
      std::memcpy(&patch.colormap, patch_data + j * 10 + 8, 2);
      tex.patches.push_back(patch);
    }

    textures.push_back(tex);
  }

  return textures;
}

/**
 * @brief Read the palette from the WAD file
 * @param offset Offset of the palette in the file
 * @param size Size of the palette
 * @return Vector containing the palette colors
 */
std::vector<WAD::Color> WAD::readPalette(std::streamoff offset,
                                         std::size_t    size) {
  std::vector<Color>   palette(256);  // DOOM palette has 256 colors
  std::vector<uint8_t> data = readLump(offset, size);

  // First palette is at offset 0
  for (int i = 0; i < 256; i++) {
    palette[i].r = data[i * 3];      // Red
    palette[i].g = data[i * 3 + 1];  // Green
    palette[i].b = data[i * 3 + 2];  // Blue
  }

  return palette;
}

/**
 * @brief Process the WAD file and load all data
 * @throws std::runtime_error if any of the lumps cannot be read
 * @note This function reads all the lumps in the WAD file and stores them in
 *       the corresponding vectors. It also prints the number of loaded lumps
 *       to the console.
 */
void WAD::processWAD() {
  uint32_t                 offset, size;
  std::vector<TextureDef>  allTextures;
  std::vector<PatchData>   allPatches;
  std::vector<Color>       palette;
  std::vector<std::string> patchNames;

  // First load PLAYPAL (needed for texture conversion)
  if (findLump("PLAYPAL", offset, size, 0)) {
    palette = readPalette(offset, size);
    std::cout << "WAD :: Loaded PLAYPAL (palette data)\n";
  }

  // Then load TEXTURE1/TEXTURE2 to know which patches we actually need
  if (findLump("TEXTURE1", offset, size, 0)) {
    std::vector<TextureDef> tex1 = readTextureDefs(offset, size);
    allTextures.insert(allTextures.end(), tex1.begin(), tex1.end());
  }

  if (findLump("TEXTURE2", offset, size, 0)) {
    std::vector<TextureDef> tex2 = readTextureDefs(offset, size);
    allTextures.insert(allTextures.end(), tex2.begin(), tex2.end());
  }

  // Load PNAMES (needed to map patch numbers to names)
  if (findLump("PNAMES", offset, size, 0)) {
    patchNames = readPatchNames(offset, size);
    std::cout << "WAD :: Found " << patchNames.size()
              << " patch names in PNAMES\n";

    // Create a set of required patch indices from textures
    std::vector<bool> requiredPatches(patchNames.size(), false);
    for (size_t i = 0; i < allTextures.size(); i++) {
      const TextureDef &tex = allTextures[i];
      for (size_t j = 0; j < tex.patches.size(); j++) {
        uint16_t patchNum = tex.patches[j].patch_num;
        if (patchNum < patchNames.size()) {
          requiredPatches[patchNum] = true;
        } else {
          std::cout << "WAD :: Warning: Texture '"
                    << std::string(tex.name, strnlen(tex.name, 8))
                    << "' references invalid patch number " << patchNum << "\n";
        }
      }
    }

    // Count how many patches we actually need
    size_t                   requiredCount = 0;
    std::vector<std::string> missingPatches;
    for (size_t i = 0; i < requiredPatches.size(); i++) {
      if (requiredPatches[i]) {
        requiredCount++;
        // Try to find this patch
        uint32_t pOffset, pSize;
        if (!findLump(patchNames[i], pOffset, pSize, 0)) {
          missingPatches.push_back(patchNames[i]);
        }
      }
    }
    std::cout << "WAD :: Need to load " << requiredCount
              << " patches for textures\n";
    if (!missingPatches.empty()) {
      std::cout << "WAD :: Missing patches: ";
      for (const std::string &name : missingPatches) {
        std::cout << name << " ";
      }
      std::cout << "\n";
    }

    // Struct to track patch marker sections
    struct PatchSection {
      std::string start;
      std::string end;
      bool        found;
      size_t      startIndex;
      size_t      endIndex;
    };

    // Define all possible patch sections
    PatchSection sections[] = {
        {"P1_START", "P1_END", false, 0, 0},  // Shareware patches
        {"P2_START", "P2_END", false, 0, 0},  // Registered patches
        {"P3_START", "P3_END", false, 0, 0}   // DOOM2 patches
    };

    // Find all patch marker sections
    for (size_t s = 0; s < 3; s++) {
      uint32_t startOffset, startSize, endOffset, endSize;
      if (findLump(sections[s].start, startOffset, startSize, 0) &&
          findLump(sections[s].end, endOffset, endSize, 0)) {
        sections[s].found = true;
        // Find section indices
        for (size_t i = 0; i < directory_.size(); i++) {
          std::string name(directory_[i].name, 8);
          while (!name.empty() && name.back() == ' ')
            name.pop_back();

          if (name == sections[s].start)
            sections[s].startIndex = i;
          if (name == sections[s].end) {
            sections[s].endIndex = i;
            break;
          }
        }
      }
    }

    // Load required patches from each section
    std::vector<bool> patchLoaded(patchNames.size(), false);
    size_t            totalLoaded = 0;

    for (size_t s = 0; s < 3; s++) {
      if (!sections[s].found)
        continue;

      size_t sectionLoaded = 0;
      for (size_t i = sections[s].startIndex + 1; i < sections[s].endIndex;
           i++) {
        std::string patchName(directory_[i].name, 8);
        while (!patchName.empty() && patchName.back() == ' ')
          patchName.pop_back();

        // Find this patch's index in PNAMES
        for (size_t p = 0; p < patchNames.size(); p++) {
          if (!patchLoaded[p] && requiredPatches[p] &&
              patchNames[p] == patchName) {
            // Load the patch
            PatchData patch =
                readPatch(directory_[i].filepos, directory_[i].size, patchName);
            allPatches.push_back(patch);
            patchLoaded[p] = true;
            sectionLoaded++;
            totalLoaded++;
            break;
          }
        }
      }

      std::cout << "WAD :: Loaded " << sectionLoaded << " patches from "
                << sections[s].start << " section\n";
    }

    // If we still have missing patches, try loading directly by name
    if (totalLoaded < requiredCount) {
      size_t directLoaded = 0;
      for (size_t p = 0; p < patchNames.size(); p++) {
        if (!patchLoaded[p] && requiredPatches[p]) {
          if (findLump(patchNames[p], offset, size, 0)) {
            PatchData patch = readPatch(offset, size, patchNames[p]);
            allPatches.push_back(patch);
            patchLoaded[p] = true;
            directLoaded++;
            totalLoaded++;
          }
        }
      }
      if (directLoaded > 0) {
        std::cout << "WAD :: Loaded " << directLoaded
                  << " patches directly by name\n";
      }
    }

    std::cout << "WAD :: Successfully loaded " << totalLoaded << " of "
              << requiredCount << " required patches\n";
  }

  // Now process levels (using the loaded textures/patches)
  for (size_t i = 0; i < directory_.size(); i++) {
    std::string lumpName(directory_[i].name, 8);
    while (!lumpName.empty() && lumpName.back() == ' ') {
      lumpName.pop_back();
    }

    if (isLevelMarker(lumpName)) {
      Level level;
      level.name         = lumpName;
      level.texture_defs = allTextures;
      level.patches      = allPatches;
      level.patch_names  = patchNames;
      level.palette      = palette;

      // Load level data (VERTEXES, LINEDEFS, etc.)
      uint32_t vOffset, vSize;
      if (findLump("VERTEXES", vOffset, vSize, i + 1)) {
        level.vertices = readVertices(vOffset, vSize);
      }
      if (findLump("LINEDEFS", vOffset, vSize, i + 1)) {
        level.linedefs = readLinedefs(vOffset, vSize);
      }
      if (findLump("SIDEDEFS", vOffset, vSize, i + 1)) {
        level.sidedefs = readSidedefs(vOffset, vSize);
      }
      if (findLump("SECTORS", vOffset, vSize, i + 1)) {
        level.sectors = readSectors(vOffset, vSize);
      }
      if (findLump("THINGS", vOffset, vSize, i + 1)) {
        level.things = readThings(vOffset, vSize);
      }

      levels_.push_back(level);
    }
  }
}

/**
 * @brief Convert WAD data to JSON verbose format
 * @return JSON string containing the WAD data
 * @note This function uses the nlohmann::json library to create a JSON
 * representation of the WAD data. The output is more verbose than the
 * compact version, with arrays formatted in a more human-readable way.
 */
std::string WAD::toJSONVerbose() const {
  nlohmann::json j;
  j["levels"] = nlohmann::json::array();

  for (size_t levelIndex = 0; levelIndex < levels_.size(); levelIndex++) {
    const Level   &level = levels_[levelIndex];
    nlohmann::json levelJson;
    levelJson["name"] = level.name;

    levelJson["vertices"] = nlohmann::json::array();
    for (size_t vertIndex = 0; vertIndex < level.vertices.size(); vertIndex++) {
      const Vertex &v = level.vertices[vertIndex];
      levelJson["vertices"].push_back({{"x", v.x}, {"y", v.y}});
    }

    levelJson["linedefs"] = nlohmann::json::array();
    for (size_t lineIndex = 0; lineIndex < level.linedefs.size(); lineIndex++) {
      const Linedef &l = level.linedefs[lineIndex];
      levelJson["linedefs"].push_back({{"start", l.start_vertex},
                                       {"end", l.end_vertex},
                                       {"flags", l.flags},
                                       {"type", l.line_type},
                                       {"tag", l.sector_tag},
                                       {"right_sidedef", l.right_sidedef},
                                       {"left_sidedef", l.left_sidedef}});
    }

    levelJson["sidedefs"] = nlohmann::json::array();
    for (size_t sideIndex = 0; sideIndex < level.sidedefs.size(); sideIndex++) {
      const Sidedef &s = level.sidedefs[sideIndex];
      levelJson["sidedefs"].push_back(
          {{"x_offset", s.x_offset},
           {"y_offset", s.y_offset},
           {"upper_texture",
            std::string(s.upper_texture, strnlen(s.upper_texture, 8))},
           {"lower_texture",
            std::string(s.lower_texture, strnlen(s.lower_texture, 8))},
           {"middle_texture",
            std::string(s.middle_texture, strnlen(s.middle_texture, 8))},
           {"sector", s.sector}});
    }

    levelJson["sectors"] = nlohmann::json::array();
    for (size_t sectIndex = 0; sectIndex < level.sectors.size(); sectIndex++) {
      const Sector &s = level.sectors[sectIndex];
      levelJson["sectors"].push_back(
          {{"floor_height", s.floor_height},
           {"ceiling_height", s.ceiling_height},
           {"floor_texture",
            std::string(s.floor_texture, strnlen(s.floor_texture, 8))},
           {"ceiling_texture",
            std::string(s.ceiling_texture, strnlen(s.ceiling_texture, 8))},
           {"light_level", s.light_level},
           {"type", s.type},
           {"tag", s.tag}});
    }

    levelJson["things"] = nlohmann::json::array();
    for (size_t thingIndex = 0; thingIndex < level.things.size();
         thingIndex++) {
      const Thing &t = level.things[thingIndex];
      levelJson["things"].push_back({{"x", t.x},
                                     {"y", t.y},
                                     {"angle", t.angle},
                                     {"type", t.type},
                                     {"flags", t.flags}});
    }

    j["levels"].push_back(levelJson);
  }

  return j.dump(1);
}

/**
 * @brief Create arrays with compact formatting
 * @param array JSON array to format
 * @return Formatted JSON string
 * @note This function formats the JSON array without line breaks and
 *       indentation, making it more compact.
 */
std::string formatArray(const nlohmann::json &array) {
  std::string result = "[";
  for (size_t i = 0; i < array.size(); ++i) {
    result += array[i].dump();  // dump each object without any formatting
    if (i < array.size() - 1) {
      result += ",";
    }
  }
  result += "]";
  return result;
}

/**
 * @brief Convert WAD data to custom DSL format
 * @return DSL string containing the WAD data
 */
std::string WAD::toDSL() const {
  std::ostringstream out;

  for (size_t levelIndex = 0; levelIndex < levels_.size(); levelIndex++) {
    const Level &level = levels_[levelIndex];

    out << "LEVEL " << level.name << " START\n\n";

    // VERTICES
    out << "VERTICES:\n";
    for (size_t vertIndex = 0; vertIndex < level.vertices.size(); vertIndex++) {
      const Vertex &v = level.vertices[vertIndex];
      out << "(" << v.x << ", " << v.y << ")\n";
    }

    // LINEDEFS
    out << "\nLINEDEFS:\n";
    for (size_t lineIndex = 0; lineIndex < level.linedefs.size(); lineIndex++) {
      const Linedef &l = level.linedefs[lineIndex];
      out << l.start_vertex << " -> " << l.end_vertex << " | flags: " << l.flags
          << " | type: " << l.line_type << " | tag: " << l.sector_tag
          << " | right: " << l.right_sidedef << " | left: " << l.left_sidedef
          << "\n";
    }

    // SECTORS
    out << "\nSECTORS:\n";
    for (size_t sectIndex = 0; sectIndex < level.sectors.size(); sectIndex++) {
      const Sector &s = level.sectors[sectIndex];
      out << "floor: " << s.floor_height << " | ceil: " << s.ceiling_height
          << " | light: " << s.light_level << " | floor_tex: "
          << std::string(s.floor_texture, strnlen(s.floor_texture, 8))
          << " | ceil_tex: "
          << std::string(s.ceiling_texture, strnlen(s.ceiling_texture, 8))
          << "\n";
    }

    // THINGS
    out << "\nTHINGS:\n";
    for (size_t thingIndex = 0; thingIndex < level.things.size();
         thingIndex++) {
      const Thing &t       = level.things[thingIndex];
      std::string  typeStr = (t.type == 1) ? "PlayerStart" : "Thing";
      out << typeStr << " at (" << t.x << ", " << t.y << ")"
          << " | angle: " << t.angle << " | type: " << t.type << "\n";
    }

    out << "\nLEVEL " << level.name << " END\n\n";
  }

  return out.str();
}

/**
 * @brief Convert WAD data to JSON brief format
 * @return JSON string containing the WAD data
 * @note This function uses the nlohmann::json library to create a JSON
 * representation of the WAD data. The output is more compact than the
 * verbose version, with arrays formatted in a single line.
 */
std::string WAD::toJSON() const {
  std::ostringstream out;
  out << "{\n";

  // lambda helper to print arrays with one object per line
  auto dumpArray = [&](const std::string &key, const nlohmann::json &array) {
    out << "   \"" << key << "\": [\n";
    for (size_t i = 0; i < array.size(); ++i) {
      out << "    " << array[i].dump(-1);
      if (i < array.size() - 1)
        out << ",";
      out << "\n";
    }
    out << "   ]";
  };

  out << " \"levels\": [\n";
  for (size_t levelIndex = 0; levelIndex < levels_.size(); levelIndex++) {
    const Level &level = levels_[levelIndex];
    // nlohmann::json levelJson;
    // levelJson["name"] = level.name;
    out << "  {\n" << "   \"name\": \"" << level.name << "\",\n";

    // v (vertices)
    nlohmann::json jv = nlohmann::json::array();
    for (size_t vertIndex = 0; vertIndex < level.vertices.size(); vertIndex++) {
      const Vertex &v = level.vertices[vertIndex];
      jv.push_back({{"x", v.x}, {"y", v.y}});
    }
    // levelJson["v"] = jv;
    dumpArray("v", jv);
    out << ",\n";

    // l (linedefs)
    nlohmann::json jl = nlohmann::json::array();
    for (size_t lineIndex = 0; lineIndex < level.linedefs.size(); lineIndex++) {
      const Linedef &l = level.linedefs[lineIndex];
      jl.push_back({{"s", l.start_vertex},
                    {"e", l.end_vertex},
                    {"f", l.flags},
                    {"t", l.line_type},
                    {"g", l.sector_tag},
                    {"r", l.right_sidedef},
                    {"l", l.left_sidedef}});
    }
    // levelJson["l"] = jl;
    dumpArray("l", jl);
    out << ",\n";

    // si (sidedefs)
    nlohmann::json jsi = nlohmann::json::array();
    for (size_t sideIndex = 0; sideIndex < level.sidedefs.size(); sideIndex++) {
      const Sidedef &s = level.sidedefs[sideIndex];
      jsi.push_back(
          {{"x", s.x_offset},
           {"y", s.y_offset},
           {"u", std::string(s.upper_texture, strnlen(s.upper_texture, 8))},
           {"l", std::string(s.lower_texture, strnlen(s.lower_texture, 8))},
           {"m", std::string(s.middle_texture, strnlen(s.middle_texture, 8))},
           {"s", s.sector}});
    }
    // levelJson["si"] = jsi;
    dumpArray("si", jsi);
    out << ",\n";

    // se (sectors)
    nlohmann::json jse = nlohmann::json::array();
    for (size_t sectIndex = 0; sectIndex < level.sectors.size(); sectIndex++) {
      const Sector &s = level.sectors[sectIndex];
      jse.push_back(
          {{"f", s.floor_height},
           {"c", s.ceiling_height},
           {"t", std::string(s.floor_texture, strnlen(s.floor_texture, 8))},
           {"x", std::string(s.ceiling_texture, strnlen(s.ceiling_texture, 8))},
           {"l", s.light_level},
           {"y", s.type},
           {"g", s.tag}});
    }
    // levelJson["se"] = jse;
    dumpArray("se", jse);
    out << ",\n";

    // t (things)
    nlohmann::json jt = nlohmann::json::array();
    for (size_t thingIndex = 0; thingIndex < level.things.size();
         thingIndex++) {
      const Thing &t = level.things[thingIndex];
      jt.push_back({{"x", t.x},
                    {"y", t.y},
                    {"a", t.angle},
                    {"t", t.type},
                    {"f", t.flags}});
    }
    // levelJson["t"] = jt;
    dumpArray("t", jt);
    out << "\n  }";

    // out << "  " << levelJson.dump(2);
    if (levelIndex < levels_.size() - 1) {
      out << ",";
    }
    out << "\n";
  }
  out << " ]\n";
  out << "}\n";

  return out.str();
}

/**
 * @brief Get a level by name
 * @param name Name of the level
 * @return Level object
 * @throws std::runtime_error if the level is not found
 */
WAD::Level WAD::getLevel(std::string name) const {
  std::cout << "WAD :: Looking for level: '" << name << "'...";

  // Trim input name from both ends
  while (!name.empty() && (name.front() == ' ' || name.front() == '\0')) {
    name.erase(0, 1);
  }
  while (!name.empty() && (name.back() == ' ' || name.back() == '\0')) {
    name.pop_back();
  }

  for (size_t i = 0; i < levels_.size(); i++) {
    std::string levelName = levels_[i].name;
    // Trim stored level name from both ends
    while (!levelName.empty() &&
           (levelName.front() == ' ' || levelName.front() == '\0')) {
      levelName.erase(0, 1);
    }
    while (!levelName.empty() &&
           (levelName.back() == ' ' || levelName.back() == '\0')) {
      levelName.pop_back();
    }

    if (levelName == name) {
      std::cout << " found!\n";
      return levels_[i];
    }
  }

  throw std::runtime_error("Level not found");
}

/**
 * @brief Get the name of a level by index
 * @param index Index of the level
 * @return Name of the level
 * @throws std::out_of_range if the index is out of range
 */
std::string WAD::getLevelNameByIndex(size_t index) const {
  if (index < levels_.size()) {
    return levels_[index].name;
  } else {
    throw std::out_of_range("Index out of range");
  }
}
