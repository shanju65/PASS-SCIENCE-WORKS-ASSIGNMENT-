#ifndef UEW_GEOMETRY_CSV_PARSER_HPP
#define UEW_GEOMETRY_CSV_PARSER_HPP

#include <vector>
#include <string>
#include <cstdint>
#include "stl_parser.hpp"

namespace uew::geometry {

struct NodalTemperatureRecord {
    int64_t node_id = 0;
    double x_m = 0.0;
    double y_m = 0.0;
    double z_m = 0.0;
    double temperature_C = 0.0;
};

struct CSVTemperatureParseResult {
    bool success = false;
    std::string error_message;
    std::string provenance;
    uint32_t total_nodes = 0;
    
    // Statistical metrics
    double min_temperature_C = 0.0;
    double max_temperature_C = 0.0;
    double mean_temperature_C = 0.0;
    BoundingBox spatial_bounds;
    
    std::vector<NodalTemperatureRecord> records;

    // Flat arrays for WebGL visualization (x,y,z, temp)
    std::vector<float> point_coords; // 3 floats per node (x, y, z)
    std::vector<float> temperatures; // 1 float per node
};

/**
 * @brief Parses and validates an ANSYS Nodal Temperature CSV string or file.
 * Required header format:
 * node_id,x_m,y_m,z_m,temperature_C
 */
CSVTemperatureParseResult parse_temperature_csv_string(const std::string& csv_content);
CSVTemperatureParseResult parse_temperature_csv_file(const std::string& file_path);

/**
 * @brief Generates a synthetic ANSYS nodal temperature CSV file with documented provenance.
 */
std::string generate_synthetic_ansys_csv();

} // namespace uew::geometry

#endif // UEW_GEOMETRY_CSV_PARSER_HPP
