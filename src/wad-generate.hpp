#ifndef WAD_GENERATE_HPP
#define WAD_GENERATE_HPP

#include "../okinawa.cpp/src/item/item.hpp"
#include "wad.hpp"
#include <vector>

/**
 * @brief Class responsible for generating individual floor and ceiling OkItems
 *        from WAD level data.
 */
class WADGenerate {
public:
  /**
   * @brief Generate individual floor items for each sector in the level.
   * @param level The WAD level data
   * @param sectorVertices Vector of vertex indices for each sector
   * @return Vector of OkItem pointers representing floor geometry
   */
  static std::vector<OkItem *>
  generateFloors(const WAD::Level                    &level,
                 const std::vector<std::vector<int>> &sectorVertices);

  /**
   * @brief Generate individual ceiling items for each sector in the level.
   * @param level The WAD level data
   * @param sectorVertices Vector of vertex indices for each sector
   * @return Vector of OkItem pointers representing ceiling geometry
   */
  static std::vector<OkItem *>
  generateCeilings(const WAD::Level                    &level,
                   const std::vector<std::vector<int>> &sectorVertices);

  /**
   * @brief Generate a single floor item for a specific sector.
   * @param level The WAD level data
   * @param sector The sector to generate floor for
   * @param sectorVertices Vector of vertex indices for this sector
   * @param sectorIndex The index of the sector in the level
   * @return OkItem pointer representing the floor geometry, or nullptr if
   * failed
   */
  static OkItem *generateSectorFloor(const WAD::Level       &level,
                                     const WAD::Sector      &sector,
                                     const std::vector<int> &sectorVertices,
                                     int                     sectorIndex);

  /**
   * @brief Generate a single ceiling item for a specific sector.
   * @param level The WAD level data
   * @param sector The sector to generate ceiling for
   * @param sectorVertices Vector of vertex indices for this sector
   * @param sectorIndex The index of the sector in the level
   * @return OkItem pointer representing the ceiling geometry, or nullptr if
   * failed
   */
  static OkItem *generateSectorCeiling(const WAD::Level       &level,
                                       const WAD::Sector      &sector,
                                       const std::vector<int> &sectorVertices,
                                       int                     sectorIndex);

private:
  /**
   * @brief Create geometry data for a sector (floor or ceiling).
   * @param level The WAD level data
   * @param sector The sector to generate geometry for
   * @param sectorVertices Vector of vertex indices for this sector
   * @param isFloor True for floor, false for ceiling
   * @param vertices Output vertex data
   * @param indices Output index data
   */
  static void createSectorGeometry(const WAD::Level       &level,
                                   const WAD::Sector      &sector,
                                   const std::vector<int> &sectorVertices,
                                   bool isFloor, std::vector<float> &vertices,
                                   std::vector<unsigned int> &indices);

  /**
   * @brief Calculate the geometric center of a sector.
   * @param level The WAD level data
   * @param sectorVertices Vector of vertex indices for this sector
   * @param centerX Output center X coordinate
   * @param centerY Output center Y coordinate
   * @param centerZ Output center Z coordinate
   */
  static void calculateSectorCenter(const WAD::Level       &level,
                                    const std::vector<int> &sectorVertices,
                                    float height, float &centerX,
                                    float &centerY, float &centerZ);

  // Constants
  static const float SCALE;
  static const float TEXTURE_SIZE;
};

#endif  // WAD_GENERATE_HPP
