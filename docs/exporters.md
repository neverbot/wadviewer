# WAD exporters (archived)

These `WAD` methods exported a parsed WAD to JSON / a custom DSL. They belonged
to an older version of the app and are **no longer used** (the viewer only reads
WADs; the `-json` / `-dsl` format selectors were never wired). They were removed
when `WAD` switched to **lazy per-level loading** (parse only the requested
level instead of building every level eagerly), since these were the only
consumers of the eager all-levels `levels_` vector.

Archived here verbatim in case the export format is ever needed again. To
restore: re-add the declarations and the `WADFormat` enum to `wad.hpp`, the
method bodies to `wad.cpp`, and a `levels_` vector populated for all levels
(or adapt them to iterate lazily-built levels).

## Header: `WADFormat` enum (wad.hpp)

```cpp
/**
 * enum with the possible formats for the file to be loaded or written.
 * - WAD: Standard WAD format
 * - JSON: JSON format
 * - JSON_VERBOSE: JSON format with verbose output
 * - DSL: Custom DSL format
 * - DSL_VERBOSE: Custom DSL format with verbose output
 * The format is used to determine how to read or write the file.
 * The default format is WAD.
 */
enum class WADFormat : std::uint8_t {
  WAD,
  JSON,
  JSON_VERBOSE,
  DSL,
  DSL_VERBOSE
};
```

## Header: public declarations (wad.hpp)

```cpp
  // Convert WAD data to JSON format
  std::string toJSON() const;
  std::string toJSONVerbose() const;
  // Convert WAD data to custom DSL format
  std::string toDSL() const;
```

## Implementation (wad.cpp)

```cpp
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
```
