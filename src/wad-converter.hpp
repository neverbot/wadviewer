#ifndef WAD_VIEWER_WAD_CONVERTER_HPP
#define WAD_VIEWER_WAD_CONVERTER_HPP

#include "./wad.hpp"
#include "okinawa/item/group.hpp"
#include "okinawa/math/point.hpp"

class WADConverter {
public:
  WADConverter()  = default;
  ~WADConverter() = default;

  static std::vector<OkItemGroup *> convertLevel(const WAD::Level &level);
  static void                       processTextures(const WAD &wad);
  static OkPoint *getPlayerStartPosition(const WAD::Level &level);

private:
  static void calculateLevelBounds(const WAD::Level &level);

  /**
   * @brief Check if a point is inside a sector boundary line.
   * Uses a cross product to determine which side of the line the point is on.
   * @param px X coordinate of the point to test
   * @param py Y coordinate of the point to test
   * @param x1 X coordinate of line start
   * @param y1 Y coordinate of line start
   * @param x2 X coordinate of line end
   * @param y2 Y coordinate of line end
   * @return true if point is on the right side of the line
   */
  static bool pointInSector(int16_t px, int16_t py, int16_t x1, int16_t y1,
                            int16_t x2, int16_t y2) {
    // Cross product: (x2-x1)(py-y1) - (y2-y1)(px-x1)
    // Positive result means point is on right side of line
    int32_t crossProduct = ((x2 - x1) * (py - y1)) - ((y2 - y1) * (px - x1));
    return crossProduct > 0;
  }
};

#endif  // WAD_VIEWER_WAD_CONVERTER_HPP
