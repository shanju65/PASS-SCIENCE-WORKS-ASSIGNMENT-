#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace uew {

struct TemperatureNode {
    int node_id = 0;
    double x_m = 0, y_m = 0, z_m = 0;
    double temperature_C = 0;
};

struct TemperatureCloud {
    bool ok = false;
    std::string error;
    bool labelled_synthetic = false;
    std::vector<TemperatureNode> nodes;
    double t_min = 0, t_max = 0;
};

inline std::string csv_trim(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
    return s;
}

inline std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> cols;
    std::string cur;
    for (char c : line) {
        if (c == ',') {
            cols.push_back(csv_trim(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    cols.push_back(csv_trim(cur));
    return cols;
}

inline bool parse_finite_double(const std::string& s, double& v, std::string& error, const std::string& name) {
    if (s.empty()) {
        error = "Missing value for " + name + ".";
        return false;
    }
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "nan" || lower == "+nan" || lower == "-nan" || lower == "inf" || lower == "+inf" ||
        lower == "-inf" || lower == "infinity" || lower == "+infinity" || lower == "-infinity") {
        error = name + " is NaN or infinity, which is rejected.";
        return false;
    }
    try {
        std::size_t idx = 0;
        v = std::stod(s, &idx);
        if (idx != s.size()) {
            error = name + " is not a plain number ('" + s + "').";
            return false;
        }
    } catch (...) {
        error = name + " is not a valid number ('" + s + "').";
        return false;
    }
    if (!std::isfinite(v)) {
        error = name + " is NaN or infinity, which is rejected.";
        return false;
    }
    return true;
}

inline TemperatureCloud parse_temperature_csv(const std::string& text) {
    TemperatureCloud cloud;
    if (text.empty()) {
        cloud.error = "CSV file is empty. Expected a header line: node_id,x_m,y_m,z_m,temperature_C";
        return cloud;
    }
    if (text.size() > 20ull * 1024ull * 1024ull) {
        cloud.error = "CSV exceeds the 20 MiB parsed-text limit used in this workbench.";
        return cloud;
    }

    std::istringstream in(text);
    std::string line;
    int line_no = 0;
    std::vector<std::string> header;
    const int kMaxNodes = 100000;

    while (std::getline(in, line)) {
        ++line_no;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string t = csv_trim(line);
        if (t.empty()) continue;
        if (t[0] == '#') {
            std::string lower = t;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower.find("synthetic") != std::string::npos) cloud.labelled_synthetic = true;
            continue;
        }
        header = split_csv_line(t);
        break;
    }
    if (header.empty()) {
        cloud.error = "CSV has no header row.";
        return cloud;
    }

    auto find_col = [&](const std::string& name) -> int {
        for (int i = 0; i < static_cast<int>(header.size()); ++i) {
            if (header[i] == name) return i;
        }
        return -1;
    };
    const int c_id = find_col("node_id");
    const int c_x = find_col("x_m");
    const int c_y = find_col("y_m");
    const int c_z = find_col("z_m");
    const int c_t = find_col("temperature_C");
    if (c_id < 0 || c_x < 0 || c_y < 0 || c_z < 0 || c_t < 0) {
        cloud.error =
            "CSV is missing required columns. Expected exactly these names: "
            "node_id,x_m,y_m,z_m,temperature_C";
        return cloud;
    }

    while (std::getline(in, line)) {
        ++line_no;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string t = csv_trim(line);
        if (t.empty() || t[0] == '#') continue;
        auto cols = split_csv_line(t);
        int need = std::max({c_id, c_x, c_y, c_z, c_t});
        if (static_cast<int>(cols.size()) <= need) {
            cloud.error = "CSV line " + std::to_string(line_no) + " is missing fields.";
            return cloud;
        }
        TemperatureNode n;
        std::string err;
        double id_d = 0;
        if (!parse_finite_double(cols[c_id], id_d, err, "node_id")) {
            cloud.error = "CSV line " + std::to_string(line_no) + ": " + err;
            return cloud;
        }
        if (std::floor(id_d) != id_d) {
            cloud.error = "CSV line " + std::to_string(line_no) + ": node_id must be an integer.";
            return cloud;
        }
        n.node_id = static_cast<int>(id_d);
        if (!parse_finite_double(cols[c_x], n.x_m, err, "x_m") ||
            !parse_finite_double(cols[c_y], n.y_m, err, "y_m") ||
            !parse_finite_double(cols[c_z], n.z_m, err, "z_m") ||
            !parse_finite_double(cols[c_t], n.temperature_C, err, "temperature_C")) {
            cloud.error = "CSV line " + std::to_string(line_no) + ": " + err;
            return cloud;
        }
        if (static_cast<int>(cloud.nodes.size()) >= kMaxNodes) {
            cloud.error = "CSV exceeds the tested node limit of 100,000 rows.";
            return cloud;
        }
        cloud.nodes.push_back(n);
    }

    if (cloud.nodes.empty()) {
        cloud.error = "CSV header was read, but there are no data rows. Empty result tables are rejected.";
        return cloud;
    }
    cloud.t_min = cloud.t_max = cloud.nodes.front().temperature_C;
    for (const auto& n : cloud.nodes) {
        cloud.t_min = std::min(cloud.t_min, n.temperature_C);
        cloud.t_max = std::max(cloud.t_max, n.temperature_C);
    }
    cloud.ok = true;
    return cloud;
}

}  // namespace uew
