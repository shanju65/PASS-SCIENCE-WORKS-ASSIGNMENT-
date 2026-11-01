#include "csv_parser.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <limits>
#include <algorithm>
#include <iomanip>

namespace uew::geometry {

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

CSVTemperatureParseResult parse_temperature_csv_string(const std::string& csv_content) {
    CSVTemperatureParseResult res;
    res.provenance = "Imported ANSYS Nodal Temperature CSV Dataset";

    if (csv_content.empty()) {
        res.success = false;
        res.error_message = "CSV file is empty (0 bytes). Expected header 'node_id,x_m,y_m,z_m,temperature_C'.";
        return res;
    }

    std::istringstream stream(csv_content);
    std::string line;

    // Check first line (header)
    if (!std::getline(stream, line)) {
        res.success = false;
        res.error_message = "CSV file contains no readable lines.";
        return res;
    }

    line = trim(line);
    // Ignore optional comment/provenance banner lines starting with #
    while (line.rfind("#", 0) == 0) {
        if (!std::getline(stream, line)) {
            res.success = false;
            res.error_message = "CSV file contains only comment lines, missing column header.";
            return res;
        }
        line = trim(line);
    }

    // Split and check columns
    std::vector<std::string> headers;
    std::stringstream header_stream(line);
    std::string col;
    while (std::getline(header_stream, col, ',')) {
        headers.push_back(trim(col));
    }

    if (headers.size() < 5) {
        res.success = false;
        res.error_message = "Malformed CSV Header: expected at least 5 columns, found " + std::to_string(headers.size());
        return res;
    }

    // Expected columns: node_id,x_m,y_m,z_m,temperature_C
    if (headers[0] != "node_id" || headers[1] != "x_m" || headers[2] != "y_m" || 
        headers[3] != "z_m" || headers[4] != "temperature_C") {
        res.success = false;
        res.error_message = "Invalid CSV Header schema: Expected 'node_id,x_m,y_m,z_m,temperature_C', but got '" + line + "'.";
        return res;
    }

    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    double min_z = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double max_y = std::numeric_limits<double>::lowest();
    double max_z = std::numeric_limits<double>::lowest();

    double min_temp = std::numeric_limits<double>::max();
    double max_temp = std::numeric_limits<double>::lowest();
    double sum_temp = 0.0;

    int line_number = 1;
    while (std::getline(stream, line)) {
        line_number++;
        line = trim(line);
        if (line.empty() || line.rfind("#", 0) == 0) {
            continue; // Skip blank lines and internal comments
        }

        std::stringstream row_stream(line);
        std::string val;
        std::vector<std::string> values;
        while (std::getline(row_stream, val, ',')) {
            values.push_back(trim(val));
        }

        if (values.size() != 5) {
            res.success = false;
            res.error_message = "Row parsing error at line " + std::to_string(line_number) + 
                                ": expected 5 columns, got " + std::to_string(values.size());
            return res;
        }

        try {
            NodalTemperatureRecord rec;
            rec.node_id = std::stoll(values[0]);
            rec.x_m = std::stod(values[1]);
            rec.y_m = std::stod(values[2]);
            rec.z_m = std::stod(values[3]);
            rec.temperature_C = std::stod(values[4]);

            // Rejection of NaN and Infinity
            if (!std::isfinite(rec.x_m) || !std::isfinite(rec.y_m) || 
                !std::isfinite(rec.z_m) || !std::isfinite(rec.temperature_C)) {
                res.success = false;
                res.error_message = "Non-finite value (NaN or Inf) encountered at line " + std::to_string(line_number);
                return res;
            }

            // Spatial bounds
            min_x = std::min(min_x, rec.x_m);
            min_y = std::min(min_y, rec.y_m);
            min_z = std::min(min_z, rec.z_m);
            max_x = std::max(max_x, rec.x_m);
            max_y = std::max(max_y, rec.y_m);
            max_z = std::max(max_z, rec.z_m);

            // Thermal stats
            min_temp = std::min(min_temp, rec.temperature_C);
            max_temp = std::max(max_temp, rec.temperature_C);
            sum_temp += rec.temperature_C;

            res.records.push_back(rec);

            // Populate flat visual arrays
            res.point_coords.push_back(static_cast<float>(rec.x_m));
            res.point_coords.push_back(static_cast<float>(rec.y_m));
            res.point_coords.push_back(static_cast<float>(rec.z_m));
            res.temperatures.push_back(static_cast<float>(rec.temperature_C));

        } catch (const std::exception& e) {
            res.success = false;
            res.error_message = "Numeric conversion failure at line " + std::to_string(line_number) + ": " + e.what();
            return res;
        }
    }

    if (res.records.empty()) {
        res.success = false;
        res.error_message = "CSV file contains a valid header, but zero data rows were found.";
        return res;
    }

    res.total_nodes = static_cast<uint32_t>(res.records.size());
    res.min_temperature_C = min_temp;
    res.max_temperature_C = max_temp;
    res.mean_temperature_C = sum_temp / static_cast<double>(res.total_nodes);

    res.spatial_bounds.min_pt = {min_x, min_y, min_z};
    res.spatial_bounds.max_pt = {max_x, max_y, max_z};
    res.spatial_bounds.center = {(min_x + max_x) * 0.5, (min_y + max_y) * 0.5, (min_z + max_z) * 0.5};
    res.spatial_bounds.dimensions = {max_x - min_x, max_y - min_y, max_z - min_z};

    res.success = true;
    return res;
}

CSVTemperatureParseResult parse_temperature_csv_file(const std::string& path) {
    CSVTemperatureParseResult res;
    std::ifstream file(path);
    if (!file.is_open()) {
        res.success = false;
        res.error_message = "Failed to open CSV file: " + path;
        return res;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse_temperature_csv_string(buffer.str());
}

std::string generate_synthetic_ansys_csv() {
    std::ostringstream oss;
    oss << "# PROVENANCE: SYNTHETIC ANSYS MECHANICAL NODAL TEMPERATURE EXPORT\n";
    oss << "# GENERATOR: Unified Engineering Workbench Synthetic FEA Mesh Generator v1.0\n";
    oss << "# UNITS: Spatial coordinates in meters (m), Temperature in degrees Celsius (C)\n";
    oss << "# MODEL: 3D Cantilever Beam undergoing internal thermal conduction & convection\n";
    oss << "# NOTE: SYNTHETIC DATA CREATED FOR TECHNICAL ASSESSMENT SPECIFICATION (TASK B)\n";
    oss << "node_id,x_m,y_m,z_m,temperature_C\n";
    oss << std::fixed << std::setprecision(5);

    int id = 1;
    // Generate a 3D structural grid: 11 x 5 x 5 = 275 nodes
    const int nx = 15;
    const int ny = 6;
    const int nz = 6;
    const double lx = 1.0;
    const double ly = 0.2;
    const double lz = 0.2;

    for (int ix = 0; ix < nx; ++ix) {
        const double x = (static_cast<double>(ix) / (nx - 1)) * lx;
        for (int iy = 0; iy < ny; ++iy) {
            const double y = (static_cast<double>(iy) / (ny - 1)) * ly - (ly * 0.5);
            for (int iz = 0; iz < nz; ++iz) {
                const double z = (static_cast<double>(iz) / (nz - 1)) * lz - (lz * 0.5);
                
                // Realistic thermal distribution: heated base at x=0 (120 C), cooling towards tip with radial gradient
                const double r2 = y * y + z * z;
                const double temp = 120.0 - 70.0 * (x / lx) + 15.0 * std::sin(3.14159 * x / lx) - 250.0 * r2;
                
                oss << id++ << "," << x << "," << y << "," << z << "," << temp << "\n";
            }
        }
    }
    return oss.str();
}

} // namespace uew::geometry
