#include "routes.hpp"
#include "../core/thermal_solver.hpp"
#include "../geometry/stl_parser.hpp"
#include "../geometry/csv_parser.hpp"
#include "../performance/multicore_engine.hpp"
#include "../neural/backprop_engine.hpp"

#include <sstream>
#include <iomanip>
#include <iostream>

namespace uew::api {

// JSON helper: escape string
static std::string escape_json(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    o << c;
                }
        }
    }
    return o.str();
}

// Simple query/json field extractor
static std::string extract_json_string(const std::string& json, const std::string& key, const std::string& default_val = "") {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return default_val;
    size_t colon = json.find(':', pos);
    if (colon == std::string::npos) return default_val;
    size_t quote_start = json.find('"', colon);
    if (quote_start == std::string::npos) return default_val;
    size_t quote_end = json.find('"', quote_start + 1);
    if (quote_end == std::string::npos) return default_val;
    return json.substr(quote_start + 1, quote_end - quote_start - 1);
}

static double extract_json_double(const std::string& json, const std::string& key, double default_val) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return default_val;
    size_t colon = json.find(':', pos);
    if (colon == std::string::npos) return default_val;
    size_t val_start = json.find_first_of("0123456789-+.", colon);
    if (val_start == std::string::npos) return default_val;
    size_t val_end = json.find_first_not_of("0123456789-+.", val_start);
    try {
        return std::stod(json.substr(val_start, (val_end == std::string::npos ? json.size() : val_end) - val_start));
    } catch (...) {
        return default_val;
    }
}

static int extract_json_int(const std::string& json, const std::string& key, int default_val) {
    return static_cast<int>(extract_json_double(json, key, default_val));
}

void register_all_routes(HttpServer& server, std::shared_ptr<hardware::HardwareManager> hw_mgr) {
    // -------------------------------------------------------------
    // 1. Health & System Check
    // -------------------------------------------------------------
    server.add_get_route("/api/health", [](const HttpRequest&) {
        auto info = performance::get_system_hardware_info();
        std::ostringstream oss;
        oss << "{"
            << "\"success\":true,"
            << "\"status\":\"OPERATIONAL\","
            << "\"service\":\"Unified Engineering Workbench C++ Backend\","
            << "\"version\":\"1.0.0\","
            << "\"cpu\":\"" << escape_json(info.cpu_model) << "\","
            << "\"logical_cores\":" << info.logical_cores << ","
            << "\"physical_cores\":" << info.physical_cores << ","
            << "\"ram_mb\":" << info.total_ram_mb << ","
            << "\"compiler\":\"" << escape_json(info.compiler_info) << "\""
            << "}";
        return HttpResponse::json(oss.str());
    });

    // -------------------------------------------------------------
    // 2. Thermal FEA Solver (Task C)
    // -------------------------------------------------------------
    server.add_post_route("/api/thermal/solve", [](const HttpRequest& req) {
        core::ThermalParams params;
        if (!req.body.empty()) {
            params.length_m = extract_json_double(req.body, "length_m", 1.0);
            params.area_m2 = extract_json_double(req.body, "area_m2", 1.0e-4);
            params.conductivity_W_mK = extract_json_double(req.body, "conductivity_W_mK", 10.0);
            params.heat_gen_W_m3 = extract_json_double(req.body, "heat_gen_W_m3", 10000.0);
            params.temp_left_C = extract_json_double(req.body, "temp_left_C", 100.0);
            params.temp_right_C = extract_json_double(req.body, "temp_right_C", 20.0);
            params.num_elements = extract_json_int(req.body, "num_elements", 10);
        }

        core::ThermalResult res = core::solve_thermal_1d(params);
        if (!res.success) {
            return HttpResponse::error(400, res.error_message);
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6);
        oss << "{"
            << "\"success\":true,"
            << "\"num_elements\":" << params.num_elements << ","
            << "\"max_midpoint_abs_error_C\":" << res.max_midpoint_abs_error_C << ","
            << "\"l2_error_norm\":" << res.l2_error_norm << ","
            << "\"nodes\":[";

        for (size_t i = 0; i < res.node_x.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "{\"x\":" << res.node_x[i] << ",\"temp_fem\":" << res.node_temp_C[i] 
                << ",\"temp_exact\":" << res.exact_temp_C[i] << "}";
        }
        oss << "],\"midpoints\":[";
        for (size_t i = 0; i < res.midpoints.size(); ++i) {
            if (i > 0) oss << ",";
            const auto& mp = res.midpoints[i];
            oss << "{\"elem\":" << mp.element_index << ",\"x_mid\":" << mp.x_mid_m
                << ",\"temp_fem\":" << mp.temp_fem_C << ",\"temp_exact\":" << mp.temp_exact_C
                << ",\"abs_error\":" << mp.abs_error_C << "}";
        }
        oss << "]}";
        return HttpResponse::json(oss.str());
    });

    server.add_get_route("/api/thermal/convergence", [](const HttpRequest&) {
        core::ThermalParams p;
        auto rep = core::run_mesh_convergence_study(p);

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6);
        oss << "{\"success\":true,\"convergence\":[";
        for (size_t i = 0; i < rep.mesh_sizes.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "{\"elements\":" << rep.mesh_sizes[i] 
                << ",\"element_length_m\":" << rep.element_lengths_m[i]
                << ",\"max_midpoint_error_C\":" << rep.max_errors_C[i] << "}";
        }
        oss << "]}";
        return HttpResponse::json(oss.str());
    });

    // -------------------------------------------------------------
    // 3. Geometry & ANSYS CSV Import (Task B)
    // -------------------------------------------------------------
    server.add_post_route("/api/geometry/parse_stl", [](const HttpRequest& req) {
        geometry::STLParseResult res;
        if (req.body.empty()) {
            // Load default example
            res = geometry::parse_ascii_stl_file("data/example_geometry.stl");
        } else {
            res = geometry::parse_ascii_stl_string(req.body);
        }

        if (!res.success) {
            return HttpResponse::error(400, res.error_message);
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4);
        oss << "{"
            << "\"success\":true,"
            << "\"solid_name\":\"" << escape_json(res.solid_name) << "\","
            << "\"facet_count\":" << res.facet_count << ","
            << "\"vertex_count\":" << res.vertex_count << ","
            << "\"surface_area\":" << res.total_surface_area << ","
            << "\"bbox\":{"
            << "\"min\":[" << res.bbox.min_pt.x << "," << res.bbox.min_pt.y << "," << res.bbox.min_pt.z << "],"
            << "\"max\":[" << res.bbox.max_pt.x << "," << res.bbox.max_pt.y << "," << res.bbox.max_pt.z << "],"
            << "\"center\":[" << res.bbox.center.x << "," << res.bbox.center.y << "," << res.bbox.center.z << "],"
            << "\"dimensions\":[" << res.bbox.dimensions.x << "," << res.bbox.dimensions.y << "," << res.bbox.dimensions.z << "]"
            << "},"
            << "\"positions\":[";
        for (size_t i = 0; i < res.positions.size(); ++i) {
            if (i > 0) oss << ",";
            oss << res.positions[i];
        }
        oss << "],\"normals\":[";
        for (size_t i = 0; i < res.normals.size(); ++i) {
            if (i > 0) oss << ",";
            oss << res.normals[i];
        }
        oss << "]}";
        return HttpResponse::json(oss.str());
    });

    server.add_post_route("/api/geometry/parse_csv", [](const HttpRequest& req) {
        geometry::CSVTemperatureParseResult res;
        if (req.body.empty()) {
            res = geometry::parse_temperature_csv_file("data/example_temperature.csv");
        } else {
            res = geometry::parse_temperature_csv_string(req.body);
        }

        if (!res.success) {
            return HttpResponse::error(400, res.error_message);
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4);
        oss << "{"
            << "\"success\":true,"
            << "\"provenance\":\"" << escape_json(res.provenance) << "\","
            << "\"total_nodes\":" << res.total_nodes << ","
            << "\"min_temp_C\":" << res.min_temperature_C << ","
            << "\"max_temp_C\":" << res.max_temperature_C << ","
            << "\"mean_temp_C\":" << res.mean_temperature_C << ","
            << "\"bbox\":{"
            << "\"min\":[" << res.spatial_bounds.min_pt.x << "," << res.spatial_bounds.min_pt.y << "," << res.spatial_bounds.min_pt.z << "],"
            << "\"max\":[" << res.spatial_bounds.max_pt.x << "," << res.spatial_bounds.max_pt.y << "," << res.spatial_bounds.max_pt.z << "],"
            << "\"center\":[" << res.spatial_bounds.center.x << "," << res.spatial_bounds.center.y << "," << res.spatial_bounds.center.z << "],"
            << "\"dimensions\":[" << res.spatial_bounds.dimensions.x << "," << res.spatial_bounds.dimensions.y << "," << res.spatial_bounds.dimensions.z << "]"
            << "},"
            << "\"point_coords\":[";
        for (size_t i = 0; i < res.point_coords.size(); ++i) {
            if (i > 0) oss << ",";
            oss << res.point_coords[i];
        }
        oss << "],\"temperatures\":[";
        for (size_t i = 0; i < res.temperatures.size(); ++i) {
            if (i > 0) oss << ",";
            oss << res.temperatures[i];
        }
        oss << "]}";
        return HttpResponse::json(oss.str());
    });

    // -------------------------------------------------------------
    // 4. Multicore Benchmark (Task D)
    // -------------------------------------------------------------
    server.add_post_route("/api/performance/benchmark", [](const HttpRequest& req) {
        int case_count = 1000;
        int mesh_size = 40;
        int max_workers = 4;
        if (!req.body.empty()) {
            case_count = extract_json_int(req.body, "case_count", 1000);
            mesh_size = extract_json_int(req.body, "mesh_size", 40);
            max_workers = extract_json_int(req.body, "max_workers", 4);
        }

        auto report = performance::run_multicore_benchmark(case_count, mesh_size, max_workers);

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4);
        oss << "{"
            << "\"success\":true,"
            << "\"cpu_model\":\"" << escape_json(report.system_info.cpu_model) << "\","
            << "\"logical_cores\":" << report.system_info.logical_cores << ","
            << "\"physical_cores\":" << report.system_info.physical_cores << ","
            << "\"ram_mb\":" << report.system_info.total_ram_mb << ","
            << "\"compiler\":\"" << escape_json(report.system_info.compiler_info) << "\","
            << "\"optimization_flags\":\"" << escape_json(report.system_info.optimization_flags) << "\","
            << "\"total_cases\":" << report.total_cases << ","
            << "\"mesh_size\":" << report.mesh_size << ","
            << "\"warmup_time_ms\":" << report.warmup_time_ms << ","
            << "\"serial_median_time_ms\":" << report.serial_median_time_ms << ","
            << "\"max_temperature_discrepancy_C\":" << report.max_temperature_discrepancy_C << ","
            << "\"numerical_agreement_passed\":" << (report.numerical_agreement_passed ? "true" : "false") << ","
            << "\"analysis_notes\":\"" << escape_json(report.analysis_notes) << "\","
            << "\"workers\":[";

        for (size_t i = 0; i < report.worker_results.size(); ++i) {
            if (i > 0) oss << ",";
            const auto& w = report.worker_results[i];
            oss << "{\"workers\":" << w.worker_count 
                << ",\"median_ms\":" << w.median_time_ms
                << ",\"speedup\":" << w.speedup
                << ",\"efficiency\":" << w.efficiency
                << ",\"repetitions\":[";
            for (size_t j = 0; j < w.repetition_times_ms.size(); ++j) {
                if (j > 0) oss << ",";
                oss << w.repetition_times_ms[j];
            }
            oss << "]}";
        }
        oss << "]}";
        return HttpResponse::json(oss.str());
    });

    // -------------------------------------------------------------
    // 5. Embedded Hardware System (Task E)
    // -------------------------------------------------------------
    server.add_get_route("/api/hardware/list", [hw_mgr](const HttpRequest&) {
        auto list = hw_mgr->list_devices();
        std::ostringstream oss;
        oss << "{\"success\":true,\"devices\":[";
        for (size_t i = 0; i < list.size(); ++i) {
            if (i > 0) oss << ",";
            const auto& d = list[i];
            auto dev = hw_mgr->get_device(d.device_id);
            int status_code = dev ? static_cast<int>(dev->get_status()) : 0;
            
            oss << "{\"id\":\"" << d.device_id << "\""
                << ",\"name\":\"" << escape_json(d.display_name) << "\""
                << ",\"vendor\":\"" << escape_json(d.vendor) << "\""
                << ",\"serial\":\"" << escape_json(d.serial_number) << "\""
                << ",\"firmware\":\"" << escape_json(d.firmware_version) << "\""
                << ",\"host_interface\":\"" << escape_json(d.host_interface) << "\""
                << ",\"power\":\"" << escape_json(d.power_requirements) << "\""
                << ",\"logic\":\"" << escape_json(d.logic_levels) << "\""
                << ",\"connector\":\"" << escape_json(d.connector_type) << "\""
                << ",\"is_simulated\":" << (d.is_simulated ? "true" : "false")
                << ",\"status_code\":" << status_code
                << ",\"pinouts\":[";
            for (size_t p = 0; p < d.pinout_table.size(); ++p) {
                if (p > 0) oss << ",";
                const auto& pin = d.pinout_table[p];
                oss << "{\"pin\":\"" << escape_json(pin.pin_name) << "\""
                    << ",\"role\":\"" << escape_json(pin.signal_role) << "\""
                    << ",\"voltage\":\"" << escape_json(pin.voltage_domain) << "\""
                    << ",\"direction\":\"" << escape_json(pin.direction) << "\""
                    << ",\"notes\":\"" << escape_json(pin.notes) << "\"}";
            }
            oss << "]}";
        }
        oss << "]}";
        return HttpResponse::json(oss.str());
    });

    server.add_post_route("/api/hardware/action", [hw_mgr](const HttpRequest& req) {
        std::string dev_id = extract_json_string(req.body, "device_id");
        std::string action = extract_json_string(req.body, "action");

        auto dev = hw_mgr->get_device(dev_id);
        if (!dev) {
            return HttpResponse::error(404, "Device not found: " + dev_id);
        }

        bool ok = false;
        if (action == "connect") {
            ok = dev->connect();
        } else if (action == "disconnect") {
            ok = dev->disconnect();
        } else if (action == "fault") {
            dev->inject_fault(true);
            ok = true;
        } else if (action == "recover") {
            ok = dev->recover_from_fault();
        } else {
            return HttpResponse::error(400, "Unknown hardware action: " + action);
        }

        std::ostringstream oss;
        oss << "{\"success\":" << (ok ? "true" : "false") 
            << ",\"status_code\":" << static_cast<int>(dev->get_status()) << "}";
        return HttpResponse::json(oss.str());
    });

    server.add_get_route("/api/hardware/telemetry", [hw_mgr](const HttpRequest& req) {
        std::string dev_id = req.query_string;
        size_t id_pos = dev_id.find("id=");
        if (id_pos != std::string::npos) {
            dev_id = dev_id.substr(id_pos + 3);
            size_t ampersand = dev_id.find('&');
            if (ampersand != std::string::npos) dev_id = dev_id.substr(0, ampersand);
        }

        auto dev = hw_mgr->get_device(dev_id);
        if (!dev) {
            return HttpResponse::error(404, "Device not found: " + dev_id);
        }

        auto tel = dev->poll_telemetry();
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);
        oss << "{"
            << "\"success\":true,"
            << "\"timestamp\":\"" << escape_json(tel.timestamp_iso) << "\","
            << "\"packet_counter\":" << tel.packet_counter << ","
            << "\"core_voltage_V\":" << tel.core_voltage_V << ","
            << "\"io_voltage_V\":" << tel.io_voltage_V << ","
            << "\"current_mA\":" << tel.current_mA << ","
            << "\"junction_temp_C\":" << tel.junction_temp_C << ","
            << "\"status\":\"" << escape_json(tel.status_payload) << "\","
            << "\"error_flag\":" << (tel.error_flag ? "true" : "false")
            << "}";
        return HttpResponse::json(oss.str());
    });

    // -------------------------------------------------------------
    // 6. Backpropagation & Neural Engine (Task F)
    // -------------------------------------------------------------
    server.add_get_route("/api/neural/train", [](const HttpRequest&) {
        auto rep = neural::run_full_backpropagation_study(0.1, 100);

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(17);
        oss << "{"
            << "\"success\":true,"
            << "\"initial_params\":{\"w\":" << rep.initial_params.w << ",\"b\":" << rep.initial_params.b
            << ",\"v\":" << rep.initial_params.v << ",\"c\":" << rep.initial_params.c << "},"
            << "\"initial_forward\":{"
            << "\"x\":" << rep.initial_forward.x << ",\"y\":" << rep.initial_forward.y
            << ",\"z\":" << rep.initial_forward.z << ",\"h\":" << rep.initial_forward.h
            << ",\"y_hat\":" << rep.initial_forward.y_hat << ",\"loss_J\":" << rep.initial_forward.loss_J
            << "},"
            << "\"initial_gradients\":{"
            << "\"dJ_dw\":" << rep.initial_analytical_gradients.dJ_dw
            << ",\"dJ_db\":" << rep.initial_analytical_gradients.dJ_db
            << ",\"dJ_dv\":" << rep.initial_analytical_gradients.dJ_dv
            << ",\"dJ_dc\":" << rep.initial_analytical_gradients.dJ_dc
            << "},"
            << "\"finite_difference_checks\":[";

        for (size_t i = 0; i < rep.finite_diff_checks.size(); ++i) {
            if (i > 0) oss << ",";
            const auto& chk = rep.finite_diff_checks[i];
            oss << "{\"param\":\"" << chk.parameter_name << "\""
                << ",\"analytical\":" << chk.analytical_gradient
                << ",\"numerical\":" << chk.numerical_gradient
                << ",\"error\":" << chk.absolute_error
                << ",\"passed\":" << (chk.passed ? "true" : "false") << "}";
        }
        oss << "],\"all_checks_passed\":" << (rep.all_finite_diff_checks_passed ? "true" : "false")
            << ",\"step1_update\":{"
            << "\"w\":" << rep.step1_params.w << ",\"b\":" << rep.step1_params.b
            << ",\"v\":" << rep.step1_params.v << ",\"c\":" << rep.step1_params.c
            << ",\"y_hat\":" << rep.step1_forward.y_hat << ",\"loss_J\":" << rep.step1_forward.loss_J
            << "},"
            << "\"final_loss\":" << rep.final_loss
            << ",\"explanation\":\"" << escape_json(rep.mathematical_explanation) << "\""
            << ",\"history\":[";

        for (size_t i = 0; i < rep.training_history.size(); ++i) {
            if (i > 0) oss << ",";
            const auto& r = rep.training_history[i];
            oss << "{\"step\":" << r.step << ",\"loss\":" << r.loss_J 
                << ",\"y_hat\":" << r.y_hat << ",\"w\":" << r.w << ",\"b\":" << r.b 
                << ",\"v\":" << r.v << ",\"c\":" << r.c << "}";
        }
        oss << "]}";
        return HttpResponse::json(oss.str());
    });

    // -------------------------------------------------------------
    // 7. CSV Exports (Reports & Results)
    // -------------------------------------------------------------
    server.add_get_route("/api/export/thermal", [](const HttpRequest&) {
        core::ThermalParams p;
        auto res = core::solve_thermal_1d(p);
        HttpResponse resp;
        resp.status_code = 200;
        resp.content_type = "text/csv";
        resp.extra_headers["Content-Disposition"] = "attachment; filename=\"thermal_fea_results.csv\"";
        resp.body = core::export_thermal_csv(res);
        return resp;
    });

    server.add_get_route("/api/export/benchmark", [](const HttpRequest&) {
        auto rep = performance::run_multicore_benchmark(1000, 40, 4);
        HttpResponse resp;
        resp.status_code = 200;
        resp.content_type = "text/csv";
        resp.extra_headers["Content-Disposition"] = "attachment; filename=\"multicore_benchmark.csv\"";
        resp.body = performance::export_benchmark_csv(rep);
        return resp;
    });

    server.add_get_route("/api/export/neural", [](const HttpRequest&) {
        auto rep = neural::run_full_backpropagation_study(0.1, 100);
        HttpResponse resp;
        resp.status_code = 200;
        resp.content_type = "text/csv";
        resp.extra_headers["Content-Disposition"] = "attachment; filename=\"neural_backprop_training.csv\"";
        resp.body = neural::export_training_history_csv(rep);
        return resp;
    });
}

} // namespace uew::api
