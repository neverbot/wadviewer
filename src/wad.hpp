#ifndef WAD_VIEWER_WAD_HPP
#define WAD_VIEWER_WAD_HPP

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

class WAD {
public:
  // Constructor takes WAD file path
  explicit WAD(const std::string &filepath, bool verbose = false);

  // WAD header structure
  struct Header {
    char     identification[4];  // IWAD or PWAD
    uint32_t numlumps;           // Number of lumps
    uint32_t infotableofs;       // Offset to directory
  };

  // Directory entry structure
  struct Directory {
    uint32_t filepos;  // Offset to start of lump
    uint32_t size;     // Size of lump
    char     name[8];  // Lump name (zero-terminated)
  };

  // Structure definitions
  struct Vertex {
    int16_t x;
    int16_t y;
  };

  struct Linedef {
    uint16_t start_vertex;
    uint16_t end_vertex;
    uint16_t flags;
    uint16_t line_type;
    uint16_t sector_tag;
    uint16_t right_sidedef;
    uint16_t left_sidedef;
  };

  struct Sidedef {
    int16_t  x_offset;
    int16_t  y_offset;
    char     upper_texture[8];
    char     lower_texture[8];
    char     middle_texture[8];
    uint16_t sector;
  };

  struct Sector {
    int16_t  floor_height;
    int16_t  ceiling_height;
    char     floor_texture[8];
    char     ceiling_texture[8];
    uint16_t light_level;
    uint16_t type;
    uint16_t tag;
  };

  struct Thing {
    int16_t  x;
    int16_t  y;
    uint16_t angle;
    uint16_t type;
    uint16_t flags;
  };

  struct PatchHeader {
    int16_t  width;             // Width of patch
    int16_t  height;            // Height of patch
    int16_t  left_offset;       // Left offset
    int16_t  top_offset;        // Top offset
    uint32_t column_offsets[];  // Offset table, size = width
  };

  struct PatchColumn {
    uint8_t top_delta;  // 0xFF is the end of column marker
    uint8_t length;     // Length of the column data
    uint8_t padding;    // Unused byte
    uint8_t data[];     // Pixel data
  };

  struct PatchData {
    char                 name[8];  // name from PNAMES
    uint16_t             width;    // Width of the patch
    uint16_t             height;   // Height of the patch
    std::vector<uint8_t> pixels;   // Pixel data (width * height)
  };

  // Patch definition in a texture
  struct PatchInTexture {
    int16_t  origin_x;   // X offset from top-left of texture
    int16_t  origin_y;   // Y offset from top-left of texture
    uint16_t patch_num;  // Index into PNAMES
    uint16_t stepdir;    // Unused
    uint16_t colormap;   // Unused
  };

  // Texture definition
  struct TextureDef {
    char                        name[8];      // Texture name
    uint32_t                    masked;       // Composite texture
    uint16_t                    width;        // Width of texture
    uint16_t                    height;       // Height of texture
    uint32_t                    column_dir;   // Unused
    uint16_t                    patch_count;  // Number of patches
    std::vector<PatchInTexture> patches;
  };

  struct Color {
    uint8_t r, g, b;
  };

  struct FlatData {
    char                 name[8];
    std::vector<uint8_t> data;  // Raw flat data (64x64 pixels)
  };

  struct Level {
    char name[8];
    // Initial player position and angle
    Thing player_start;  // Player 1 start position (Thing type 1)
    bool  has_player_start;
    // Level geometry
    std::vector<Vertex>  vertices;
    std::vector<Linedef> linedefs;
    std::vector<Sidedef> sidedefs;
    std::vector<Sector>  sectors;
    std::vector<Thing>   things;
    // Textures and visuals
    std::vector<PatchData>   patches;
    std::vector<std::string> patch_names;   // PNAMES
    std::vector<TextureDef>  texture_defs;  // TEXTURE1/TEXTURE2
    std::vector<Color>       palette;       // PLAYPAL lump (256 colors)
    std::vector<FlatData>    flats;         // Floor/ceiling textures
  };

  // Process and load all WAD data
  void processWAD();

  // Convert WAD data to JSON format
  std::string toJSON() const;
  std::string toJSONVerbose() const;
  // Convert WAD data to custom DSL format
  std::string toDSL() const;

  Level       getLevel(std::string name) const;
  std::string getLevelNameByIndex(size_t index) const;

private:
  bool                   verbose_;
  std::string            filepath_;
  Header                 header_;
  std::vector<Directory> directory_;
  std::vector<PatchData> patches_;

  // List of levels in the WAD file
  std::vector<Level> levels_;

  // Method to read the WAD directory
  void readDirectory();
  bool isLevelMarker(const std::string &name) const;

  // Method to find a lump by name
  bool findLump(const std::string &name, uint32_t &offset, uint32_t &size,
                size_t startIndex) const;
  // Method to read a lump from the WAD file
  std::vector<uint8_t> readLump(std::streamoff offset, std::size_t size);

  // Methods to read lumps by type
  // These methods will read the lump data and return a vector of the
  // appropriate type
  std::vector<Vertex>  readVertices(std::streamoff offset, std::size_t size);
  std::vector<Linedef> readLinedefs(std::streamoff offset, std::size_t size);
  std::vector<Sidedef> readSidedefs(std::streamoff offset, std::size_t size);
  std::vector<Sector>  readSectors(std::streamoff offset, std::size_t size);
  std::vector<Thing>   readThings(std::streamoff offset, std::size_t size);
  std::vector<std::string> readPatchNames(std::streamoff offset,
                                          std::size_t    size);
  std::vector<TextureDef>  readTextureDefs(std::streamoff offset,
                                           std::size_t    size);
  PatchData                readPatch(std::streamoff offset, std::size_t size,
                                     const std::string &name);
  std::vector<Color>       readPalette(std::streamoff offset, std::size_t size);
};

#endif  // WAD_VIEWER_WAD_HPP
