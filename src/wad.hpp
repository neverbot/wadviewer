#pragma once
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

  // Process and load all WAD data
  void processWAD();

  // Convert WAD data to JSON format
  std::string toJSON() const;
  std::string toJSONVerbose() const;
  // Convert WAD data to custom DSL format
  std::string toDSL() const;

private:
  bool                   verbose_;
  std::string            filepath_;
  Header                 header_;
  std::vector<Directory> directory_;

  // Storage for WAD data
  std::vector<Vertex>  vertices_;
  std::vector<Linedef> linedefs_;
  std::vector<Sidedef> sidedefs_;
  std::vector<Sector>  sectors_;
  std::vector<Thing>   things_;

  // Method to read the WAD directory
  void readDirectory();

  // Method to find a lump by name
  bool findLump(const std::string &name, uint32_t &offset,
                uint32_t &size) const;
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
};
