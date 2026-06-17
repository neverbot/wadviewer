#include "wad-generate.hpp"
#include "okinawa/config/config.hpp"
#include "okinawa/handlers/textures.hpp"
#include "okinawa/item/item.hpp"
#include "okinawa/item/texture.hpp"
#include "okinawa/utils/logger.hpp"
#include "okinawa/utils/strings.hpp"
#include "wad.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <mapbox/earcut.hpp>
#include <string>
#include <vector>

// Initialize constants
const float WADGenerate::SCALE        = 1.0f;
const float WADGenerate::TEXTURE_SIZE = 64.0f;  // DOOM uses 64x64 flat textures

std::vector<OkItem *> WADGenerate::generateFloors(
    const WAD::Level                    &level,
    const std::vector<std::vector<int>> &sectorVertices) {

  std::vector<OkItem *> floorItems;

  for (size_t i = 0; i < level.sectors.size(); i++) {
    if (i >= sectorVertices.size()) {
      continue;
    }

    const WAD::Sector &sector = level.sectors[i];
    OkItem *floorItem = generateSectorFloor(level, sector, sectorVertices[i],
                                            static_cast<int>(i));

    if (floorItem) {
      floorItems.push_back(floorItem);
    }
  }

  return floorItems;
}

std::vector<OkItem *> WADGenerate::generateCeilings(
    const WAD::Level                    &level,
    const std::vector<std::vector<int>> &sectorVertices) {

  std::vector<OkItem *> ceilingItems;

  for (size_t i = 0; i < level.sectors.size(); i++) {
    if (i >= sectorVertices.size()) {
      continue;
    }

    const WAD::Sector &sector      = level.sectors[i];
    OkItem            *ceilingItem = generateSectorCeiling(
        level, sector, sectorVertices[i], static_cast<int>(i));

    if (ceilingItem) {
      ceilingItems.push_back(ceilingItem);
    }
  }

  return ceilingItems;
}

OkItem *WADGenerate::generateSectorFloor(const WAD::Level       &level,
                                         const WAD::Sector      &sector,
                                         const std::vector<int> &sectorVertices,
                                         int                     sectorIndex) {

  // Check if sector has valid floor texture
  std::string floorTexName =
      OkStrings::trimFixedString(sector.floor_texture, 8);
  if (floorTexName.empty() || floorTexName == "-") {
    return nullptr;
  }

  // Check if we have enough vertices
  if (sectorVertices.size() < 3) {
    return nullptr;
  }

  std::vector<float>        vertices;
  std::vector<unsigned int> indices;

  // Create geometry for this floor
  createSectorGeometry(level, sector, sectorIndex, true, vertices, indices);

  if (vertices.empty() || indices.empty()) {
    return nullptr;
  }

  // Calculate the geometric center for positioning
  float centerX, centerY, centerZ;
  float height = static_cast<float>(sector.floor_height) * SCALE;
  calculateSectorCenter(level, sectorVertices, height, centerX, centerY,
                        centerZ);

  // Convert to local coordinates (relative to center)
  for (size_t i = 0; i < vertices.size(); i += 5) {
    vertices[i] -= centerX;      // x - centerX
    vertices[i + 1] -= centerY;  // y - centerY
    vertices[i + 2] -= centerZ;  // z - centerZ
    // vertices[i + 3] and vertices[i + 4] are texture coords, leave unchanged
  }

  // Create vertex and index arrays for OkItem
  float        *vertexData = new float[vertices.size()];
  unsigned int *indexData  = new unsigned int[indices.size()];

  for (size_t i = 0; i < vertices.size(); i++) {
    vertexData[i] = vertices[i];
  }
  for (size_t i = 0; i < indices.size(); i++) {
    indexData[i] = indices[i];
  }

  // Create the item
  std::string itemName =
      "floor_sector_" + std::to_string(sectorIndex) + "_" + floorTexName;
  OkItem *item =
      new OkItem(itemName, vertexData, static_cast<long>(vertices.size()),
                 indexData, static_cast<long>(indices.size()));

  // Set the item's position to the calculated center
  item->setPosition(centerX, centerY, centerZ);

  // Try to assign texture
  OkTexture *texture =
      OkTextureHandler::getInstance()->getTexture(floorTexName);
  if (texture) {
    item->setTexture(floorTexName, texture);
    OkLogger::info("WADGenerator", "Generated floor item '" + itemName +
                                       "' with texture '" + floorTexName + "'");
  } else {
    OkLogger::warning("WADGenerator", "Could not find texture '" +
                                          floorTexName + "' for floor item '" +
                                          itemName + "'");
  }

  return item;
}

OkItem *WADGenerate::generateSectorCeiling(
    const WAD::Level &level, const WAD::Sector &sector,
    const std::vector<int> &sectorVertices, int sectorIndex) {

  // Check if sector has valid ceiling texture
  std::string ceilingTexName =
      OkStrings::trimFixedString(sector.ceiling_texture, 8);
  if (ceilingTexName.empty() || ceilingTexName == "-") {
    return nullptr;
  }

  // F_SKY1 is DOOM's sky marker: such a sector is open to the sky and DOOM
  // draws the sky behind it, never a solid ceiling flat. Omit the ceiling so
  // open-air rooms stay open (a real sky-dome could replace this later).
  if (ceilingTexName == "F_SKY1") {
    return nullptr;
  }

  // Check if we have enough vertices
  if (sectorVertices.size() < 3) {
    return nullptr;
  }

  std::vector<float>        vertices;
  std::vector<unsigned int> indices;

  // Create geometry for this ceiling
  createSectorGeometry(level, sector, sectorIndex, false, vertices, indices);

  if (vertices.empty() || indices.empty()) {
    return nullptr;
  }

  // Calculate the geometric center for positioning
  float centerX, centerY, centerZ;
  float height = static_cast<float>(sector.ceiling_height) * SCALE;
  calculateSectorCenter(level, sectorVertices, height, centerX, centerY,
                        centerZ);

  // Convert to local coordinates (relative to center)
  for (size_t i = 0; i < vertices.size(); i += 5) {
    vertices[i] -= centerX;      // x - centerX
    vertices[i + 1] -= centerY;  // y - centerY
    vertices[i + 2] -= centerZ;  // z - centerZ
    // vertices[i + 3] and vertices[i + 4] are texture coords, leave unchanged
  }

  // Create vertex and index arrays for OkItem
  float        *vertexData = new float[vertices.size()];
  unsigned int *indexData  = new unsigned int[indices.size()];

  for (size_t i = 0; i < vertices.size(); i++) {
    vertexData[i] = vertices[i];
  }
  for (size_t i = 0; i < indices.size(); i++) {
    indexData[i] = indices[i];
  }

  // Create the item
  std::string itemName =
      "ceiling_sector_" + std::to_string(sectorIndex) + "_" + ceilingTexName;
  OkItem *item =
      new OkItem(itemName, vertexData, static_cast<long>(vertices.size()),
                 indexData, static_cast<long>(indices.size()));

  // Set the item's position to the calculated center
  item->setPosition(centerX, centerY, centerZ);

  // Try to assign texture
  OkTexture *texture =
      OkTextureHandler::getInstance()->getTexture(ceilingTexName);
  if (texture) {
    item->setTexture(ceilingTexName, texture);
    OkLogger::info("WADGenerator", "Generated ceiling item '" + itemName +
                                       "' with texture '" + ceilingTexName +
                                       "'");
  } else {
    OkLogger::warning("WADGenerator",
                      "Could not find texture '" + ceilingTexName +
                          "' for ceiling item '" + itemName + "'");
  }

  return item;
}

namespace {

  // 2x the signed area of a loop given as WAD vertex indices (DOOM x,y plane).
  // Positive when the loop winds counter-clockwise.
  double loopSignedArea2(const WAD::Level       &level,
                         const std::vector<int> &loop) {
    double sum = 0.0;
    size_t n   = loop.size();
    for (size_t i = 0; i < n; i++) {
      const WAD::Vertex &p = level.vertices[loop[i]];
      const WAD::Vertex &q = level.vertices[loop[(i + 1) % n]];
      sum += static_cast<double>(p.x) * static_cast<double>(q.y) -
             static_cast<double>(q.x) * static_cast<double>(p.y);
    }
    return sum;
  }

  // Ray-casting point-in-polygon test (polygon given as WAD vertex indices).
  bool pointInLoop(const WAD::Level &level, double px, double py,
                   const std::vector<int> &loop) {
    bool   inside = false;
    size_t n      = loop.size();
    size_t j      = n - 1;
    for (size_t i = 0; i < n; i++) {
      double ax = static_cast<double>(level.vertices[loop[i]].x);
      double ay = static_cast<double>(level.vertices[loop[i]].y);
      double bx = static_cast<double>(level.vertices[loop[j]].x);
      double by = static_cast<double>(level.vertices[loop[j]].y);
      if (((ay > py) != (by > py)) &&
          (px < (bx - ax) * (py - ay) / (by - ay) + ax)) {
        inside = !inside;
      }
      j = i;
    }
    return inside;
  }

  // Split one chained boundary loop into simple sub-loops at any vertex it
  // revisits. Thin/pinched sectors (e.g. a step ledge whose arms meet at a
  // single vertex) chain into a weakly-simple loop that passes through the
  // pinch vertices twice; ear-clipping needs each piece to be a simple polygon.
  // When a vertex reappears, the path since its first occurrence is a closed
  // simple sub-loop, which we cut out (the standard mesh-DOOM sector
  // decomposition).
  std::vector<std::vector<int>> splitSimpleLoops(const std::vector<int> &loop) {
    std::vector<std::vector<int>> result;
    std::vector<int>              path;
    std::map<int, int>            pos;  // vertex index -> position in path
    for (size_t i = 0; i < loop.size(); i++) {
      int                          v  = loop[i];
      std::map<int, int>::iterator it = pos.find(v);
      if (it != pos.end()) {
        int              start = it->second;
        std::vector<int> sub(path.begin() + start, path.end());
        if (sub.size() >= 3) {
          result.push_back(sub);
        }
        for (size_t k = start; k < path.size(); k++) {
          pos.erase(path[k]);
        }
        path.resize(start);
        pos[v] = static_cast<int>(path.size());
        path.push_back(v);
      } else {
        pos[v] = static_cast<int>(path.size());
        path.push_back(v);
      }
    }
    if (path.size() >= 3) {
      result.push_back(path);
    }
    return result;
  }

  // Build the ordered boundary loops of a sector by chaining its linedef edges.
  // A linedef whose front (right) sidedef is the sector contributes the edge
  // start->end; whose back (left) sidedef is the sector contributes end->start.
  // This keeps the sector interior consistently on one side, so the chained
  // edges close into loops: the outer boundary plus any inner holes (pillars).
  std::vector<std::vector<int>> buildSectorLoops(const WAD::Level &level,
                                                 int sectorIndex) {
    std::vector<std::vector<int>> loops;

    // Collect the sector's directed boundary edges (a -> b as vertex indices).
    std::vector<int> edgeA;
    std::vector<int> edgeB;
    for (size_t i = 0; i < level.linedefs.size(); i++) {
      const WAD::Linedef &linedef = level.linedefs[i];
      if (linedef.start_vertex >= level.vertices.size() ||
          linedef.end_vertex >= level.vertices.size()) {
        continue;
      }
      if (linedef.right_sidedef != 0xFFFF &&
          linedef.right_sidedef < level.sidedefs.size() &&
          level.sidedefs[linedef.right_sidedef].sector == sectorIndex) {
        edgeA.push_back(static_cast<int>(linedef.start_vertex));
        edgeB.push_back(static_cast<int>(linedef.end_vertex));
      }
      if (linedef.left_sidedef != 0xFFFF &&
          linedef.left_sidedef < level.sidedefs.size() &&
          level.sidedefs[linedef.left_sidedef].sector == sectorIndex) {
        edgeA.push_back(static_cast<int>(linedef.end_vertex));
        edgeB.push_back(static_cast<int>(linedef.start_vertex));
      }
    }

    // Cancel exact reverse-pairs before chaining. A self-referencing linedef
    // (both sidedefs point at THIS sector -- the Boom deep-water / fake-bridge
    // control trick) contributes both a->b and b->a, so the two directed edges
    // are an exact reverse-pair. They carry no real boundary: a pure control
    // sector is made entirely of such pairs and must yield zero edges (skipped
    // harmlessly, no degenerate sliver loop), while a sector that mixes a few
    // self-ref linedefs with real walls keeps its genuine boundary intact.
    // (Blanket-excluding self-ref linedefs is wrong -- it also drops the real
    // edges of mixed sectors and leaves more loops unclosed.)
    std::vector<bool> cancelled(edgeA.size(), false);
    for (size_t i = 0; i < edgeA.size(); i++) {
      if (cancelled[i]) {
        continue;
      }
      for (size_t k = i + 1; k < edgeA.size(); k++) {
        if (!cancelled[k] && edgeA[k] == edgeB[i] && edgeB[k] == edgeA[i]) {
          cancelled[i] = true;
          cancelled[k] = true;
          break;
        }
      }
    }

    // Chain edges end-to-start into closed loops.
    std::vector<bool> used(edgeA.size(), false);
    for (size_t i = 0; i < edgeA.size(); i++) {
      used[i] = cancelled[i];
    }
    for (size_t i = 0; i < edgeA.size(); i++) {
      if (used[i]) {
        continue;
      }
      std::vector<int> loop;
      int              loopStart = edgeA[i];
      int              cur       = static_cast<int>(i);
      bool             closed    = false;
      while (cur != -1 && !used[cur]) {
        used[cur] = true;
        loop.push_back(edgeA[cur]);
        int endVertex = edgeB[cur];
        if (endVertex == loopStart) {
          closed = true;
          break;
        }
        // At a junction (more than one of the sector's edges leaves this
        // vertex) the greedy "first unused" pick can jump onto a different face
        // and merge two distinct loops into one self-intersecting ring. Trace
        // the face properly: among the unused outgoing edges, take the one that
        // turns the most CLOCKWISE relative to the incoming direction (the
        // right-hand boundary-following rule for our edge winding). This keeps
        // touching inner/outer loops separate.
        int    prevVertex = edgeA[cur];
        double dinX       = static_cast<double>(level.vertices[endVertex].x) -
                            static_cast<double>(level.vertices[prevVertex].x);
        double dinY       = static_cast<double>(level.vertices[endVertex].y) -
                            static_cast<double>(level.vertices[prevVertex].y);
        int    next       = -1;
        double bestTurn   = 0.0;
        for (size_t k = 0; k < edgeA.size(); k++) {
          if (used[k] || edgeA[k] != endVertex) {
            continue;
          }
          double doutX = static_cast<double>(level.vertices[edgeB[k]].x) -
                         static_cast<double>(level.vertices[endVertex].x);
          double doutY = static_cast<double>(level.vertices[edgeB[k]].y) -
                         static_cast<double>(level.vertices[endVertex].y);
          double cross = dinX * doutY - dinY * doutX;
          double dot   = dinX * doutX + dinY * doutY;
          double turn  = std::atan2(cross, dot);  // (-pi, pi]; < 0 = clockwise
          if (next == -1 || turn < bestTurn) {
            bestTurn = turn;
            next     = static_cast<int>(k);
          }
        }
        cur = next;
      }
      if (closed && loop.size() >= 3) {
        std::vector<std::vector<int>> simples = splitSimpleLoops(loop);
        for (size_t s = 0; s < simples.size(); s++) {
          loops.push_back(simples[s]);
        }
      }
    }
    return loops;
  }

}  // namespace

void WADGenerate::createSectorGeometry(const WAD::Level  &level,
                                       const WAD::Sector &sector,
                                       int sectorIndex, bool isFloor,
                                       std::vector<float>        &vertices,
                                       std::vector<unsigned int> &indices) {
  float levelCenterX = OkConfig::getFloat("level.center.x");
  float levelCenterY = OkConfig::getFloat("level.center.y");
  float height = isFloor ? static_cast<float>(sector.floor_height) * SCALE
                         : static_cast<float>(sector.ceiling_height) * SCALE;

  // Build the sector's ordered boundary loops from its linedefs.
  std::vector<std::vector<int>> loops = buildSectorLoops(level, sectorIndex);
  if (loops.empty()) {
    return;
  }

  // Classify loops into outer boundaries and holes: a loop is a hole when one
  // of its vertices lies inside another, larger loop (its container).
  std::vector<bool> isHole(loops.size(), false);
  std::vector<int>  container(loops.size(), -1);
  for (size_t a = 0; a < loops.size(); a++) {
    double areaA = std::fabs(loopSignedArea2(level, loops[a]));
    // Use the loop centroid (not a boundary vertex) as the containment probe:
    // a boundary vertex is frequently shared with the containing loop, where
    // point-in-polygon is ambiguous. The centroid sits well inside the loop's
    // region, which is itself inside the container, so the test is reliable.
    double px = 0.0;
    double py = 0.0;
    for (size_t k = 0; k < loops[a].size(); k++) {
      px += static_cast<double>(level.vertices[loops[a][k]].x);
      py += static_cast<double>(level.vertices[loops[a][k]].y);
    }
    px /= static_cast<double>(loops[a].size());
    py /= static_cast<double>(loops[a].size());
    double smallest = std::numeric_limits<double>::max();
    for (size_t b = 0; b < loops.size(); b++) {
      if (a == b) {
        continue;
      }
      double areaB = std::fabs(loopSignedArea2(level, loops[b]));
      if (areaB <= areaA) {
        continue;
      }
      if (pointInLoop(level, px, py, loops[b]) && areaB < smallest) {
        smallest     = areaB;
        container[a] = static_cast<int>(b);
      }
    }
    if (container[a] != -1) {
      isHole[a] = true;
    }
  }

  // Triangulate each outer loop together with the holes it contains, using a
  // robust polygon-with-holes triangulator (mapbox/earcut). buildSectorLoops
  // already splits weakly-simple loops, so each ring is simple; earcut does the
  // hole bridging internally and copes with holes that touch the outer or each
  // other (the pillar / pinwheel cases the hand-written ear-clipper left with
  // floor gaps).
  for (size_t a = 0; a < loops.size(); a++) {
    if (isHole[a]) {
      continue;
    }

    // Build the earcut polygon: ring 0 is the outer boundary, the rest are its
    // holes. flatVerts maps each flattened ring point back to its WAD vertex
    // index (earcut returns triangle indices into that flattened order).
    std::vector<std::vector<std::array<double, 2>>> polygon;
    std::vector<int>                                flatVerts;

    std::vector<std::array<double, 2>> outerRing;
    for (size_t k = 0; k < loops[a].size(); k++) {
      const WAD::Vertex    &v = level.vertices[loops[a][k]];
      std::array<double, 2> p;
      p[0] = static_cast<double>(v.x);
      p[1] = static_cast<double>(v.y);
      outerRing.push_back(p);
      flatVerts.push_back(loops[a][k]);
    }
    polygon.push_back(outerRing);

    for (size_t b = 0; b < loops.size(); b++) {
      if (!isHole[b] || container[b] != static_cast<int>(a)) {
        continue;
      }
      std::vector<std::array<double, 2>> holeRing;
      for (size_t k = 0; k < loops[b].size(); k++) {
        const WAD::Vertex    &v = level.vertices[loops[b][k]];
        std::array<double, 2> p;
        p[0] = static_cast<double>(v.x);
        p[1] = static_cast<double>(v.y);
        holeRing.push_back(p);
        flatVerts.push_back(loops[b][k]);
      }
      polygon.push_back(holeRing);
    }

    std::vector<uint32_t> tris = mapbox::earcut<uint32_t>(polygon);

    // Emit GL vertices for the flattened ring points; the triangle indices from
    // earcut are positions into that flattened order (offset by base here).
    unsigned int base = static_cast<unsigned int>(vertices.size() / 5);
    for (size_t i = 0; i < flatVerts.size(); i++) {
      const WAD::Vertex &vertex = level.vertices[flatVerts[i]];
      float x = (static_cast<float>(vertex.x) - levelCenterX) * SCALE;
      float z = (static_cast<float>(vertex.y) - levelCenterY) * SCALE;
      // Flats tile on the absolute world grid (origin 0,0) so adjacent sectors
      // align, matching vanilla DOOM. GL_REPEAT tiles, so emit raw UVs.
      float u = static_cast<float>(vertex.x) / TEXTURE_SIZE;
      float v = static_cast<float>(vertex.y) / TEXTURE_SIZE;
      vertices.push_back(x);
      vertices.push_back(height);
      vertices.push_back(-z);  // Negate Z for the engine coordinate system
      vertices.push_back(u);
      vertices.push_back(v);
    }

    // Append triangles (ceiling uses reversed winding).
    for (size_t t = 0; t + 3 <= tris.size(); t += 3) {
      unsigned int i0 = base + tris[t];
      unsigned int i1 = base + tris[t + 1];
      unsigned int i2 = base + tris[t + 2];
      if (isFloor) {
        indices.push_back(i0);
        indices.push_back(i1);
        indices.push_back(i2);
      } else {
        indices.push_back(i0);
        indices.push_back(i2);
        indices.push_back(i1);
      }
    }
  }
}

void WADGenerate::calculateSectorCenter(const WAD::Level       &level,
                                        const std::vector<int> &sectorVertices,
                                        float height, float &centerX,
                                        float &centerY, float &centerZ) {

  if (sectorVertices.empty()) {
    centerX = centerY = centerZ = 0.0f;
    return;
  }

  // Get level center coordinates from global config
  float levelCenterX = OkConfig::getFloat("level.center.x");
  float levelCenterY = OkConfig::getFloat("level.center.y");

  float minX = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float minZ = std::numeric_limits<float>::max();
  float maxZ = std::numeric_limits<float>::lowest();

  // Calculate bounds using same coordinate transformation as walls
  for (size_t i = 0; i < sectorVertices.size(); i++) {
    if (sectorVertices[i] >= static_cast<int>(level.vertices.size())) {
      continue;  // Skip invalid vertex indices
    }

    const WAD::Vertex &vertex = level.vertices[sectorVertices[i]];
    // Apply the same coordinate transformation as walls
    float x = (static_cast<float>(vertex.x) - levelCenterX) * SCALE;
    float z = (static_cast<float>(vertex.y) - levelCenterY) * SCALE;

    minX = std::min(minX, x);
    maxX = std::max(maxX, x);
    minZ = std::min(minZ, z);
    maxZ = std::max(maxZ, z);
  }

  // Calculate center
  centerX = (minX + maxX) * 0.5f;
  centerY = height;                 // Y is the height
  centerZ = -(minZ + maxZ) * 0.5f;  // Negate Z for proper coordinate system
}

std::vector<int> WADGenerate::generateSectorVertices(const WAD::Level &level,
                                                     int sectorIndex) {
  std::vector<int> sectorVertices;

  // Iterate through all linedefs to find ones that reference this sector
  for (size_t i = 0; i < level.linedefs.size(); i++) {
    const WAD::Linedef &linedef = level.linedefs[i];

    // Skip invalid vertex indices
    if (linedef.start_vertex >= level.vertices.size() ||
        linedef.end_vertex >= level.vertices.size()) {
      continue;
    }

    // Check right side
    if (linedef.right_sidedef != 0xFFFF &&
        linedef.right_sidedef < level.sidedefs.size()) {
      const WAD::Sidedef &rightSide = level.sidedefs[linedef.right_sidedef];

      if (rightSide.sector == sectorIndex) {
        sectorVertices.push_back(linedef.start_vertex);
        sectorVertices.push_back(linedef.end_vertex);
      }
    }

    // Check left side
    if (linedef.left_sidedef != 0xFFFF &&
        linedef.left_sidedef < level.sidedefs.size()) {
      const WAD::Sidedef &leftSide = level.sidedefs[linedef.left_sidedef];

      if (leftSide.sector == sectorIndex) {
        sectorVertices.push_back(linedef.start_vertex);
        sectorVertices.push_back(linedef.end_vertex);
      }
    }
  }

  // Remove duplicates and sort
  std::sort(sectorVertices.begin(), sectorVertices.end());
  sectorVertices.erase(
      std::unique(sectorVertices.begin(), sectorVertices.end()),
      sectorVertices.end());

  return sectorVertices;
}
