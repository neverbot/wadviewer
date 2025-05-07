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
  // DOOM 1 level names are ExMy (x = episode, y = mission)
  if (name.length() == 4 && name[0] == 'E' && name[2] == 'M' &&
      std::isdigit(name[1]) && std::isdigit(name[3])) {
    return true;
  }

  // DOOM 2 level names are MAPxx (xx = 01-32)
  if (name.length() == 5 && name.substr(0, 3) == "MAP" &&
      std::isdigit(name[3]) && std::isdigit(name[4])) {
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

    if (lumpName == name) {
      offset = directory_[i].filepos;
      size   = directory_[i].size;
      return true;
    }

    // Stop searching when we hit the next level marker
    if (i > startIndex && isLevelMarker(lumpName)) {
      break;
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
 * @brief Process the WAD file and load all data
 * @throws std::runtime_error if any of the lumps cannot be read
 * @note This function reads all the lumps in the WAD file and stores them in
 *       the corresponding vectors. It also prints the number of loaded lumps
 *       to the console.
 */
void WAD::processWAD() {
  for (size_t i = 0; i < directory_.size(); i++) {
    std::string lumpName(directory_[i].name, strnlen(directory_[i].name, 8));

    if (isLevelMarker(lumpName)) {
      Level level;
      level.name = lumpName;

      uint32_t offset, size;

      // Read vertices
      if (findLump("VERTEXES", offset, size, i + 1)) {
        level.vertices = readVertices(offset, size);
        if (verbose_) {
          std::cout << "Level " << lumpName << ": Loaded "
                    << level.vertices.size() << " vertices\n";
        }
      }

      // Read linedefs
      if (findLump("LINEDEFS", offset, size, i + 1)) {
        level.linedefs = readLinedefs(offset, size);
        if (verbose_) {
          std::cout << "Level " << lumpName << ": Loaded "
                    << level.linedefs.size() << " linedefs\n";
        }
      }

      // Read sidedefs
      if (findLump("SIDEDEFS", offset, size, i + 1)) {
        level.sidedefs = readSidedefs(offset, size);
        if (verbose_) {
          std::cout << "Level " << lumpName << ": Loaded "
                    << level.sidedefs.size() << " sidedefs\n";
        }
      }

      // Read sectors
      if (findLump("SECTORS", offset, size, i + 1)) {
        level.sectors = readSectors(offset, size);
        if (verbose_) {
          std::cout << "Level " << lumpName << ": Loaded "
                    << level.sectors.size() << " sectors\n";
        }
      }

      // Read things
      if (findLump("THINGS", offset, size, i + 1)) {
        level.things = readThings(offset, size);
        if (verbose_) {
          std::cout << "Level " << lumpName << ": Loaded "
                    << level.things.size() << " things\n";
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
  for (size_t i = 0; i < levels_.size(); i++) {
    if (levels_[i].name == name) {
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
