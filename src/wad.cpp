#include "wad.hpp"
#include <_string.h>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Local trimFixedString implementation
static std::string trimString(const std::string &str, size_t maxLen) {
  static const std::string whitespace = " \t\n\r\f\v";
  std::string              result;
  if (str.length() > maxLen) {
    result = str.substr(0, maxLen);
  } else {
    result = str;
  }
  size_t last = result.find_last_not_of(whitespace);
  if (last == std::string::npos) {
    return "";
  }
  return result.substr(0, last + 1);
}

/**
 * @brief WAD constructor
 * @param filepath Path to the WAD file
 * @throws std::runtime_error if the file cannot be opened or is not a valid WAD
 * file
 */
WAD::WAD(const std::string &filepath, bool verbose) {
  verbose_ = verbose;
  addSource(filepath);
}

/**
 * @brief WAD constructor for a merged archive (IWAD + PWAD(s))
 * @param filepaths Source files in load order: IWAD (resources) first, then the
 *        PWAD(s) on top. Later files override/append earlier ones by lump name.
 * @param verbose Enable verbose output
 */
WAD::WAD(const std::vector<std::string> &filepaths, bool verbose) {
  verbose_ = verbose;
  for (size_t i = 0; i < filepaths.size(); i++) {
    addSource(filepaths[i]);
  }
}

/**
 * @brief Load one source file and append its lumps to the merged archive
 * @param filepath Path to the WAD file
 * @throws std::runtime_error if the file cannot be opened or is not a valid WAD
 * @note The whole file is read into memory (sourceData_); lump reads then index
 *       into that buffer instead of reopening the file once per lump.
 */
void WAD::addSource(const std::string &filepath) {
  std::ifstream file(filepath, std::ios::binary | std::ios::ate);
  if (!file) {
    throw std::runtime_error("Unable to open WAD file: " + filepath);
  }

  // Read the entire file into memory.
  std::streamoff fileSize = file.tellg();
  file.seekg(0);
  std::vector<uint8_t> bytes(static_cast<size_t>(fileSize));
  file.read(reinterpret_cast<char *>(bytes.data()),
            static_cast<std::streamsize>(fileSize));
  if (!file) {
    throw std::runtime_error("Unable to read WAD file: " + filepath);
  }

  // Parse the header from the in-memory bytes.
  if (bytes.size() < sizeof(Header)) {
    throw std::runtime_error("Truncated WAD header: " + filepath);
  }
  Header header;
  std::memcpy(&header, bytes.data(), sizeof(Header));

  std::string id(header.identification, 4);
  if (id != "IWAD" && id != "PWAD") {
    throw std::runtime_error("Not a valid WAD file: " + filepath);
  }

  if (verbose_) {
    std::cout << "WAD :: Loading " << id << " " << filepath << " ("
              << header.numlumps << " lumps)\n";
  }

  // Register the source and append its directory entries, tagging each with the
  // source index so later reads know which file's bytes to index into.
  size_t sourceIndex = sources_.size();
  sources_.push_back(filepath);
  sourceData_.push_back(std::vector<uint8_t>());
  sourceData_.back().swap(bytes);

  const std::vector<uint8_t> &data = sourceData_[sourceIndex];
  for (uint32_t i = 0; i < header.numlumps; i++) {
    size_t entryOffset = header.infotableofs + i * sizeof(Directory);
    if (entryOffset + sizeof(Directory) > data.size()) {
      break;  // Truncated directory; stop rather than read past the buffer.
    }
    Directory entry;
    std::memcpy(&entry, data.data() + entryOffset, sizeof(Directory));
    directory_.push_back(entry);
    lumpSource_.push_back(sourceIndex);
  }
}

/**
 * @brief Check if a lump name is a level marker
 * @param name Lump name
 * @return true if the name is a level marker, false otherwise
 */
bool WAD::isLevelMarker(const std::string &name) {
  std::string cleanName = trimString(name, 8);

  // DOOM 1 level names are ExMy (x = episode, y = mission). (No logging here:
  // this predicate is called in hot loops -- levels are logged once when
  // enumerated in processWAD.)
  if (cleanName.length() == 4 && cleanName[0] == 'E' && cleanName[2] == 'M' &&
      std::isdigit(cleanName[1]) && std::isdigit(cleanName[3])) {
    return true;
  }

  // DOOM 2 level names are MAPxx (xx = 01-32)
  if (cleanName.length() == 5 && cleanName.substr(0, 3) == "MAP" &&
      std::isdigit(cleanName[3]) && std::isdigit(cleanName[4])) {
    return true;
  }

  return false;
}

/**
 * @brief Find the first lump named `name` at or after `startIndex`
 * @param name Lump name
 * @param loc Filled with the lump location (source + byte range) when found
 * @param startIndex Directory index to start searching from
 * @return true if the lump is found, false otherwise
 * @note Forward first-match: used for a map's sub-lumps, which follow their
 *       level marker in the same source segment.
 */
bool WAD::findLump(const std::string &name, LumpLoc &loc,
                   size_t startIndex) const {
  for (size_t i = startIndex; i < directory_.size(); i++) {
    std::string lumpName = trimString(directory_[i].name, 8);

    // Stop searching for level data at next level marker
    if (name == "VERTEXES" || name == "LINEDEFS" || name == "SIDEDEFS" ||
        name == "SECTORS" || name == "THINGS") {
      // Only stop if we're after a level marker and find another one
      if (i > startIndex && isLevelMarker(lumpName)) {
        break;
      }
    }

    if (lumpName == name) {
      loc.source = lumpSource_[i];
      loc.offset = directory_[i].filepos;
      loc.size   = directory_[i].size;
      return true;
    }
  }

  return false;
}

/**
 * @brief Find the last lump with the given name (override-aware)
 * @param name Lump name
 * @param loc Filled with the lump location when found
 * @return true if found
 * @note Scans the whole merged archive and returns the LAST match, so a lump in
 *       a later source (PWAD) shadows the same-named lump in an earlier one
 *       (IWAD) -- DOOM's "last-wins" rule for shared resources and maps.
 */
bool WAD::findLastLump(const std::string &name, LumpLoc &loc) const {
  bool found = false;
  for (size_t i = 0; i < directory_.size(); i++) {
    if (trimString(directory_[i].name, 8) == name) {
      loc.source = lumpSource_[i];
      loc.offset = directory_[i].filepos;
      loc.size   = directory_[i].size;
      found      = true;
    }
  }
  return found;
}

/**
 * @brief Whether a level marker with this name occurs after the given index
 * @note Used to skip a shadowed level (e.g. the IWAD's MAP01 when a PWAD also
 *       provides MAP01) so the later copy is the one enumerated.
 */
bool WAD::hasLevelMarkerAfter(const std::string &name,
                              size_t             afterIndex) const {
  for (size_t i = afterIndex + 1; i < directory_.size(); i++) {
    if (isLevelMarker(directory_[i].name) &&
        trimString(directory_[i].name, 8) == name) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Read a lump's raw bytes from the source file that owns it
 * @param loc Lump location (source + byte range)
 * @return Vector containing the lump data
 */
std::vector<uint8_t> WAD::readLump(const LumpLoc &loc) const {
  const std::vector<uint8_t> &data = sourceData_[loc.source];
  size_t                      end  = static_cast<size_t>(loc.offset) + loc.size;
  if (end > data.size()) {
    throw std::runtime_error("Lump extends past end of source: " +
                             sources_[loc.source]);
  }
  return std::vector<uint8_t>(data.begin() + loc.offset, data.begin() + end);
}

/**
 * @brief Read vertices from the WAD file
 * @param offset Offset of the vertices in the file
 * @param size Size of the vertices
 * @return Vector containing the vertices
 */
std::vector<WAD::Vertex> WAD::readVertices(const LumpLoc &loc) const {
  std::vector<uint8_t> data = readLump(loc);
  std::vector<Vertex>  vertices(loc.size / sizeof(Vertex));
  std::memcpy(vertices.data(), data.data(), loc.size);
  return vertices;
}

/**
 * @brief Read linedefs from the WAD file
 * @param offset Offset of the linedefs in the file
 * @param size Size of the linedefs
 * @return Vector containing the linedefs
 */
std::vector<WAD::Linedef> WAD::readLinedefs(const LumpLoc &loc) const {
  std::vector<uint8_t> data = readLump(loc);
  std::vector<Linedef> linedefs(loc.size / sizeof(Linedef));
  std::memcpy(linedefs.data(), data.data(), loc.size);
  return linedefs;
}

/**
 * @brief Read sidedefs from the WAD file
 * @param offset Offset of the sidedefs in the file
 * @param size Size of the sidedefs
 * @return Vector containing the sidedefs
 */
std::vector<WAD::Sidedef> WAD::readSidedefs(const LumpLoc &loc) const {
  std::vector<uint8_t> data = readLump(loc);
  std::vector<Sidedef> sidedefs(loc.size / sizeof(Sidedef));
  std::memcpy(sidedefs.data(), data.data(), loc.size);
  return sidedefs;
}

/**
 * @brief Read sectors from the WAD file
 * @param offset Offset of the sectors in the file
 * @param size Size of the sectors
 * @return Vector containing the sectors
 */
std::vector<WAD::Sector> WAD::readSectors(const LumpLoc &loc) const {
  std::vector<uint8_t> data = readLump(loc);
  std::vector<Sector>  sectors(loc.size / sizeof(Sector));
  std::memcpy(sectors.data(), data.data(), loc.size);
  return sectors;
}

/**
 * @brief Read things from the WAD file
 * @param offset Offset of the things in the file
 * @param size Size of the things
 * @return Vector containing the things
 */
std::vector<WAD::Thing> WAD::readThings(const LumpLoc &loc) const {
  std::vector<uint8_t> data = readLump(loc);
  std::vector<Thing>   things(loc.size / sizeof(Thing));
  std::memcpy(things.data(), data.data(), loc.size);
  return things;
}

/**
 * @brief Read a patch lump and convert it to RGBA format
 * @param offset Offset of the patch in the file
 * @param size Size of the patch
 * @param name Name of the patch
 * @return PatchData containing the converted patch
 */
WAD::PatchData WAD::readPatch(const LumpLoc     &loc,
                              const std::string &name) const {
  std::vector<uint8_t> data = readLump(loc);
  PatchData            patch;
  std::strncpy(patch.name, name.c_str(), 8);  // Copy name to char array

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

        // Store raw palette index in the pixels array
        patch.pixels[destIndex + 0] = pixel;  // Store palette index
        patch.pixels[destIndex + 1] = 0;      // Not used
        patch.pixels[destIndex + 2] = 0;      // Not used
        patch.pixels[destIndex + 3] = 255;    // Fully opaque
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
std::vector<std::string> WAD::readPatchNames(const LumpLoc &loc) const {
  std::vector<uint8_t>     data = readLump(loc);
  std::vector<std::string> names;

  // First 4 bytes is number of patches
  uint32_t num_patches;
  std::memcpy(&num_patches, data.data(), sizeof(uint32_t));

  // Pre-allocate vector capacity
  names.reserve(num_patches);

  // Read patch names (8 bytes each, zero-terminated)
  const char *name_data = reinterpret_cast<const char *>(data.data() + 4);
  for (uint32_t i = 0; i < num_patches; i++) {
    std::string name = trimString(name_data + i * 8, 8);
    // PNAMES entries are sometimes stored lower-case, but the actual patch
    // lumps are upper-case and DOOM matches them case-insensitively. Normalise
    // to upper-case so lookups resolve (e.g. TEKWALL4's "w94_1" -> "W94_1").
    for (size_t c = 0; c < name.size(); c++) {
      if (name[c] >= 'a' && name[c] <= 'z') {
        name[c] = static_cast<char>(name[c] - ('a' - 'A'));
      }
    }
    names.push_back(name);
  }

  return names;
}

/**
 * @brief Read texture definitions from the WAD file
 * @param offset Offset of the texture definitions in the file
 * @param size Size of the texture definitions
 * @return Vector containing the texture definitions
 */
std::vector<WAD::TextureDef> WAD::readTextureDefs(const LumpLoc &loc) const {
  std::vector<uint8_t>    data = readLump(loc);
  std::vector<TextureDef> textures;

  // First 4 bytes is number of textures
  uint32_t num_textures;
  std::memcpy(&num_textures, data.data(), sizeof(uint32_t));

  // Pre-allocate vectors
  textures.reserve(num_textures);

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

    // Pre-allocate patches vector
    tex.patches.reserve(tex.patch_count);

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
std::vector<WAD::Color> WAD::readPalette(const LumpLoc &loc) const {
  std::vector<Color>   palette(256);  // DOOM palette has 256 colors
  std::vector<uint8_t> data = readLump(loc);

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
  LumpLoc                  loc;
  std::vector<TextureDef>  allTextures;
  std::vector<PatchData>   allPatches;
  std::vector<Color>       palette;
  std::vector<std::string> patchNames;

  // Shared resources (palette, textures, patch names, patches) use override-
  // aware lookups (findLastLump): when an IWAD and a PWAD both define a lump,
  // the PWAD's (loaded later) wins, matching DOOM. A resource-less PWAD falls
  // through to the IWAD's copy, which is what lets DOOM II map packs render.

  // First load PLAYPAL (needed for texture conversion)
  if (findLastLump("PLAYPAL", loc)) {
    palette = readPalette(loc);
    std::cout << "WAD :: Loaded PLAYPAL (palette data)\n";
  }

  // Then load TEXTURE1/TEXTURE2 to know which patches we actually need
  if (findLastLump("TEXTURE1", loc)) {
    std::vector<TextureDef> tex1 = readTextureDefs(loc);
    allTextures.insert(allTextures.end(), tex1.begin(), tex1.end());
  }

  if (findLastLump("TEXTURE2", loc)) {
    std::vector<TextureDef> tex2 = readTextureDefs(loc);
    allTextures.insert(allTextures.end(), tex2.begin(), tex2.end());
  }

  // Load PNAMES (needed to map patch numbers to names)
  if (findLastLump("PNAMES", loc)) {
    patchNames = readPatchNames(loc);
    std::cout << "WAD :: Found " << patchNames.size()
              << " patch names in PNAMES\n";

    // Index the patch store by PNAMES index (not a compacted list): texture
    // definitions reference patches by their PNAMES index (patch_num), so a
    // compacted array only works by luck when the required indices happen to be
    // contiguous. Pre-size and fill by index so any WAD resolves correctly;
    // unloaded slots stay empty and are skipped at composite time.
    allPatches.assign(patchNames.size(), PatchData());

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
    missingPatches.reserve(patchNames.size());  // Pre-allocate worst case

    for (size_t i = 0; i < requiredPatches.size(); i++) {
      if (requiredPatches[i]) {
        requiredCount++;
        // Try to find this patch
        LumpLoc patchLoc;
        if (!findLastLump(patchNames[i], patchLoc)) {
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
      LumpLoc startLoc, endLoc;
      if (findLastLump(sections[s].start, startLoc) &&
          findLastLump(sections[s].end, endLoc)) {
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
            // Load the patch (store at its PNAMES index)
            LumpLoc patchLoc;
            patchLoc.source = lumpSource_[i];
            patchLoc.offset = directory_[i].filepos;
            patchLoc.size   = directory_[i].size;
            PatchData patch = readPatch(patchLoc, patchName);
            allPatches[p]   = patch;
            patchLoaded[p]  = true;
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
          LumpLoc patchLoc;
          if (findLastLump(patchNames[p], patchLoc)) {
            PatchData patch = readPatch(patchLoc, patchNames[p]);
            allPatches[p]   = patch;
            patchLoaded[p]  = true;
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
    std::string lumpName = trimString(directory_[i].name, 8);

    if (isLevelMarker(lumpName)) {
      // Skip a level marker that a later source overrides: when an IWAD and a
      // PWAD both define MAP01, only the PWAD's (last) copy is enumerated.
      if (hasLevelMarkerAfter(lumpName, i)) {
        continue;
      }

      std::cout << "WAD :: Found level " << lumpName << "\n";
      Level level;
      std::strncpy(level.name, lumpName.c_str(), 8);
      level.texture_defs = allTextures;
      level.patches      = allPatches;
      level.patch_names  = patchNames;
      level.palette      = palette;

      // Load level data (VERTEXES, LINEDEFS, etc.). These sub-lumps follow the
      // marker in the same source, so a forward first-match search is correct.
      LumpLoc subLoc;
      if (findLump("VERTEXES", subLoc, i + 1)) {
        level.vertices = readVertices(subLoc);
      }
      if (findLump("LINEDEFS", subLoc, i + 1)) {
        level.linedefs = readLinedefs(subLoc);
      }
      if (findLump("SIDEDEFS", subLoc, i + 1)) {
        level.sidedefs = readSidedefs(subLoc);
      }
      if (findLump("SECTORS", subLoc, i + 1)) {
        level.sectors = readSectors(subLoc);
      }
      if (findLump("THINGS", subLoc, i + 1)) {
        level.things = readThings(subLoc);
      }

      // Load player start position (Thing type 1)
      for (size_t j = 0; j < level.things.size(); j++) {
        if (level.things[j].type == 1) {
          level.has_player_start = true;
          level.player_start     = level.things[j];
          break;
        }
      }

      // Load all unique flat textures referenced by sectors
      std::set<std::string> uniqueFlats;
      for (size_t j = 0; j < level.sectors.size(); j++) {
        std::string floorTex = trimString(level.sectors[j].floor_texture, 8);
        std::string ceilTex  = trimString(level.sectors[j].ceiling_texture, 8);

        if (!floorTex.empty() && floorTex != "-") {
          uniqueFlats.insert(floorTex);
        }
        if (!ceilTex.empty() && ceilTex != "-") {
          uniqueFlats.insert(ceilTex);
        }
      }

      // Load each unique flat texture (override-aware: a PWAD flat shadows the
      // IWAD's; a resource-less PWAD resolves flats from the IWAD).
      for (std::set<std::string>::iterator it = uniqueFlats.begin();
           it != uniqueFlats.end(); ++it) {
        LumpLoc flatLoc;
        if (findLastLump(*it, flatLoc)) {
          std::vector<uint8_t> flatData = readLump(flatLoc);
          if (flatData.size() == 64 * 64) {  // DOOM flats are always 64x64
            FlatData flat;
            std::strncpy(flat.name, it->c_str(), 8);
            flat.data = flatData;
            level.flats.push_back(flat);
          }
        }
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
      jsi.push_back({{"x", s.x_offset},
                     {"y", s.y_offset},
                     {"u", trimString(s.upper_texture, 8)},
                     {"l", trimString(s.lower_texture, 8)},
                     {"m", trimString(s.middle_texture, 8)},
                     {"s", s.sector}});
    }
    // levelJson["si"] = jsi;
    dumpArray("si", jsi);
    out << ",\n";

    // se (sectors)
    nlohmann::json jse = nlohmann::json::array();
    for (size_t sectIndex = 0; sectIndex < level.sectors.size(); sectIndex++) {
      const Sector &s = level.sectors[sectIndex];
      jse.push_back({{"f", s.floor_height},
                     {"c", s.ceiling_height},
                     {"t", trimString(s.floor_texture, 8)},
                     {"x", trimString(s.ceiling_texture, 8)},
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
WAD::Level WAD::getLevel(const std::string &name) const {
  std::cout << "WAD :: Looking for level: '" << name << "'...";

  // Compare the first 8 characters of the name
  for (size_t i = 0; i < levels_.size(); i++) {
    if (strncmp(levels_[i].name, name.c_str(), 8) == 0) {
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
std::string WAD::getLevelNameByIndex(int index) const {
  if (index < levels_.size()) {
    return std::string(levels_[index].name, strnlen(levels_[index].name, 8));
  }

  throw std::out_of_range("Index out of range");
}
