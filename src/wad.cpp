#include "wad.hpp"
#include <_string.h>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
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

  // Store the shared resources once. Every level built on demand references
  // these (see buildLevel) instead of re-parsing or duplicating them.
  palette_.swap(palette);
  textureDefs_.swap(allTextures);
  allPatches_.swap(allPatches);
  patchNames_.swap(patchNames);

  // Record the (non-shadowed) level markers. Geometry is NOT parsed here --
  // only getLevel() parses a level, on demand, so viewing one level no longer
  // pays to parse all of them. When an IWAD and a PWAD both define a level, the
  // later (PWAD) copy is the one kept.
  for (size_t i = 0; i < directory_.size(); i++) {
    std::string lumpName = trimString(directory_[i].name, 8);
    if (isLevelMarker(lumpName) && !hasLevelMarkerAfter(lumpName, i)) {
      std::cout << "WAD :: Found level " << lumpName << "\n";
      levelMarkers_.push_back(i);
    }
  }
}

/**
 * @brief Parse a single level on demand
 * @param markerIndex Directory index of the level's marker lump
 * @return The fully parsed Level, carrying the shared resources
 * @note Only the requested level is parsed; the viewer never needs the others.
 */
WAD::Level WAD::buildLevel(size_t markerIndex) const {
  Level       level;
  std::string lumpName = trimString(directory_[markerIndex].name, 8);
  std::strncpy(level.name, lumpName.c_str(), 8);

  // Attach the shared resources (parsed once in processWAD).
  level.texture_defs = textureDefs_;
  level.patches      = allPatches_;
  level.patch_names  = patchNames_;
  level.palette      = palette_;

  // Load level data (VERTEXES, LINEDEFS, ...). These sub-lumps follow the
  // marker in the same source, so a forward first-match search is correct.
  LumpLoc subLoc;
  if (findLump("VERTEXES", subLoc, markerIndex + 1)) {
    level.vertices = readVertices(subLoc);
  }
  if (findLump("LINEDEFS", subLoc, markerIndex + 1)) {
    level.linedefs = readLinedefs(subLoc);
  }
  if (findLump("SIDEDEFS", subLoc, markerIndex + 1)) {
    level.sidedefs = readSidedefs(subLoc);
  }
  if (findLump("SECTORS", subLoc, markerIndex + 1)) {
    level.sectors = readSectors(subLoc);
  }
  if (findLump("THINGS", subLoc, markerIndex + 1)) {
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

  // Collect the unique flat textures referenced by sectors and load each
  // (override-aware: a PWAD flat shadows the IWAD's; a resource-less PWAD
  // resolves flats from the IWAD).
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

  return level;
}

/**
 * @brief Get a level by name
 * @param name Name of the level
 * @return Level object
 * @throws std::runtime_error if the level is not found
 */
WAD::Level WAD::getLevel(const std::string &name) const {
  std::cout << "WAD :: Looking for level: '" << name << "'...";

  // Find the matching (non-shadowed) marker and parse just that level.
  std::string target = trimString(name, 8);
  for (size_t i = 0; i < levelMarkers_.size(); i++) {
    if (trimString(directory_[levelMarkers_[i]].name, 8) == target) {
      std::cout << " found!\n";
      return buildLevel(levelMarkers_[i]);
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
  if (index >= 0 && static_cast<size_t>(index) < levelMarkers_.size()) {
    return trimString(directory_[levelMarkers_[index]].name, 8);
  }

  throw std::out_of_range("Index out of range");
}
