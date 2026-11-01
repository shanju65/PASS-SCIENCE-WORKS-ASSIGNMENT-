#ifndef UEW_GEOMETRY_STL_PARSER_HPP
#define UEW_GEOMETRY_STL_PARSER_HPP

#include <vector>
#include <string>
#include <cstdint>

namespace uew::geometry {

struct Vector3D {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct TriangleFacet {
    Vector3D normal;
    Vector3D v1;
    Vector3D v2;
    Vector3D v3;
};

struct BoundingBox {
    Vector3D min_pt;
    Vector3D max_pt;
    Vector3D center;
    Vector3D dimensions;
};

struct STLParseResult {
    bool success = false;
    std::string error_message;
    std::string solid_name;
    uint32_t facet_count = 0;
    uint32_t vertex_count = 0;
    BoundingBox bbox;
    double total_surface_area = 0.0;
    std::vector<TriangleFacet> facets;
    
    // Flat arrays suitable for direct WebGL / Three.js serialization
    std::vector<float> positions; // 9 floats per triangle (x1,y1,z1, x2,y2,z2, x3,y3,z3)
    std::vector<float> normals;   // 9 floats per triangle
};

/**
 * @brief Parses an ASCII STL file from an input stream or string buffer.
 * Performs rigorous validation:
 * - Checks for valid "solid" header and "endsolid"
 * - Rejects non-finite values (NaN, Inf)
 * - Validates 3 vertices per facet within "outer loop" / "endloop"
 * - Computes exact geometric bounding box and surface area
 */
STLParseResult parse_ascii_stl_string(const std::string& stl_content);
STLParseResult parse_ascii_stl_file(const std::string& file_path);

} // namespace uew::geometry

#endif // UEW_GEOMETRY_STL_PARSER_HPP
