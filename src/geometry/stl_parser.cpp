#include "stl_parser.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <limits>
#include <algorithm>

namespace uew::geometry {

static inline bool is_finite_vec(const Vector3D& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

static inline double compute_facet_area(const Vector3D& v1, const Vector3D& v2, const Vector3D& v3) {
    const double ax = v2.x - v1.x;
    const double ay = v2.y - v1.y;
    const double az = v2.z - v1.z;

    const double bx = v3.x - v1.x;
    const double by = v3.y - v1.y;
    const double bz = v3.z - v1.z;

    const double cx = ay * bz - az * by;
    const double cy = az * bx - ax * bz;
    const double cz = ax * by - ay * bx;

    return 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
}

STLParseResult parse_ascii_stl_string(const std::string& content) {
    STLParseResult res;
    
    if (content.empty()) {
        res.success = false;
        res.error_message = "STL file is empty (0 bytes). Expected ASCII STL solid definition.";
        return res;
    }

    std::istringstream stream(content);
    std::string token;
    
    // Check initial keyword
    if (!(stream >> token)) {
        res.success = false;
        res.error_message = "STL file contains only whitespace.";
        return res;
    }

    if (token != "solid") {
        res.success = false;
        res.error_message = "Malformed STL: File must begin with 'solid' keyword.";
        return res;
    }

    // Read solid name (rest of first line)
    std::getline(stream, res.solid_name);
    // Trim leading whitespace
    while (!res.solid_name.empty() && (res.solid_name.front() == ' ' || res.solid_name.front() == '\t')) {
        res.solid_name.erase(0, 1);
    }
    if (res.solid_name.empty()) {
        res.solid_name = "unnamed_solid";
    }

    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    double min_z = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double max_y = std::numeric_limits<double>::lowest();
    double max_z = std::numeric_limits<double>::lowest();

    bool closed_properly = false;

    while (stream >> token) {
        if (token == "endsolid") {
            closed_properly = true;
            break;
        }

        if (token != "facet") {
            res.success = false;
            res.error_message = "Malformed STL: Expected 'facet' or 'endsolid', encountered token '" + token + "'.";
            return res;
        }

        std::string normal_token;
        if (!(stream >> normal_token) || normal_token != "normal") {
            res.success = false;
            res.error_message = "Malformed STL: Expected 'normal' after 'facet'.";
            return res;
        }

        Vector3D normal;
        if (!(stream >> normal.x >> normal.y >> normal.z)) {
            res.success = false;
            res.error_message = "Malformed STL: Failed to parse 3D normal vector components.";
            return res;
        }
        if (!is_finite_vec(normal)) {
            res.success = false;
            res.error_message = "Malformed STL: Normal vector contains NaN or Infinity.";
            return res;
        }

        std::string outer_token, loop_token;
        if (!(stream >> outer_token >> loop_token) || outer_token != "outer" || loop_token != "loop") {
            res.success = false;
            res.error_message = "Malformed STL: Expected 'outer loop' block.";
            return res;
        }

        Vector3D vertices[3];
        for (int i = 0; i < 3; ++i) {
            std::string vertex_kw;
            if (!(stream >> vertex_kw) || vertex_kw != "vertex") {
                res.success = false;
                res.error_message = "Malformed STL: Expected 'vertex' keyword for vertex index " + std::to_string(i + 1);
                return res;
            }
            if (!(stream >> vertices[i].x >> vertices[i].y >> vertices[i].z)) {
                res.success = false;
                res.error_message = "Malformed STL: Invalid numeric coordinates for vertex " + std::to_string(i + 1);
                return res;
            }
            if (!is_finite_vec(vertices[i])) {
                res.success = false;
                res.error_message = "Malformed STL: Vertex contains NaN or Infinity.";
                return res;
            }

            // Update bounding box
            min_x = std::min(min_x, vertices[i].x);
            min_y = std::min(min_y, vertices[i].y);
            min_z = std::min(min_z, vertices[i].z);
            max_x = std::max(max_x, vertices[i].x);
            max_y = std::max(max_y, vertices[i].y);
            max_z = std::max(max_z, vertices[i].z);
        }

        std::string endloop_token;
        if (!(stream >> endloop_token) || endloop_token != "endloop") {
            res.success = false;
            res.error_message = "Malformed STL: Expected 'endloop' closing tag.";
            return res;
        }

        std::string endfacet_token;
        if (!(stream >> endfacet_token) || endfacet_token != "endfacet") {
            res.success = false;
            res.error_message = "Malformed STL: Expected 'endfacet' closing tag.";
            return res;
        }

        // Store parsed facet
        TriangleFacet facet;
        facet.normal = normal;
        facet.v1 = vertices[0];
        facet.v2 = vertices[1];
        facet.v3 = vertices[2];
        res.facets.push_back(facet);

        // Surface area
        res.total_surface_area += compute_facet_area(vertices[0], vertices[1], vertices[2]);

        // Flat buffers for frontend
        for (int i = 0; i < 3; ++i) {
            res.positions.push_back(static_cast<float>(vertices[i].x));
            res.positions.push_back(static_cast<float>(vertices[i].y));
            res.positions.push_back(static_cast<float>(vertices[i].z));

            res.normals.push_back(static_cast<float>(normal.x));
            res.normals.push_back(static_cast<float>(normal.y));
            res.normals.push_back(static_cast<float>(normal.z));
        }
    }

    if (!closed_properly && res.facets.empty()) {
        res.success = false;
        res.error_message = "Malformed STL: File truncated before facets or 'endsolid' statement.";
        return res;
    }

    if (res.facets.empty()) {
        res.success = false;
        res.error_message = "STL file defined an empty solid with 0 facets.";
        return res;
    }

    res.facet_count = static_cast<uint32_t>(res.facets.size());
    res.vertex_count = res.facet_count * 3;

    res.bbox.min_pt = {min_x, min_y, min_z};
    res.bbox.max_pt = {max_x, max_y, max_z};
    res.bbox.center = {(min_x + max_x) * 0.5, (min_y + max_y) * 0.5, (min_z + max_z) * 0.5};
    res.bbox.dimensions = {max_x - min_x, max_y - min_y, max_z - min_z};

    res.success = true;
    return res;
}

STLParseResult parse_ascii_stl_file(const std::string& path) {
    STLParseResult res;
    std::ifstream file(path);
    if (!file.is_open()) {
        res.success = false;
        res.error_message = "Failed to open STL file: " + path;
        return res;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse_ascii_stl_string(buffer.str());
}

} // namespace uew::geometry
