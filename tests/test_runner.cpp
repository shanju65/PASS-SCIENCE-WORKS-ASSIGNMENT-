#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

#include "core/thermal_solver.hpp"
#include "geometry/stl_parser.hpp"
#include "geometry/csv_parser.hpp"
#include "performance/multicore_engine.hpp"
#include "hardware/hardware_manager.hpp"
#include "neural/backprop_engine.hpp"

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAILED] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            g_tests_failed++; \
        } else { \
            std::cout << "  [PASS] " << msg << "\n"; \
            g_tests_passed++; \
        } \
    } while(0)

// -------------------------------------------------------------
// Test Suite 1: Thermal FEA Solver (Task C)
// -------------------------------------------------------------
void test_thermal_solver() {
    std::cout << "\n=== [TEST SUITE 1] Thermal FEA Solver Tests ===\n";

    uew::core::ThermalParams p;
    p.length_m = 1.0;
    p.area_m2 = 1.0e-4;
    p.conductivity_W_mK = 10.0;
    p.heat_gen_W_m3 = 10000.0;
    p.temp_left_C = 100.0;
    p.temp_right_C = 20.0;
    p.num_elements = 10;

    // 1. Boundary conditions check
    auto res = uew::core::solve_thermal_1d(p);
    TEST_ASSERT(res.success, "Thermal solve succeeds for standard 10-element problem");
    TEST_ASSERT(std::abs(res.node_temp_C.front() - 100.0) < 1e-12, "Left Dirichlet boundary condition T(0) == 100 C exact");
    TEST_ASSERT(std::abs(res.node_temp_C.back() - 20.0) < 1e-12, "Right Dirichlet boundary condition T(L) == 20 C exact");

    // 2. Analytical solution verification at midpoint
    double exact_mid = uew::core::evaluate_analytical_solution(0.5, p);
    // T_exact(0.5) = 100 - 80*0.5 + (10000 / 20) * 0.5 * 0.5 = 100 - 40 + 125 = 185.0 C
    TEST_ASSERT(std::abs(exact_mid - 185.0) < 1e-9, "Analytical solution at x=0.5 m evaluates exactly to 185.0 C");

    // 3. Mesh Convergence Study (N = 10, 20, 40, 80)
    auto conv = uew::core::run_mesh_convergence_study(p);
    TEST_ASSERT(conv.max_errors_C.size() == 4, "Convergence study contains all 4 required mesh sizes");
    TEST_ASSERT(conv.max_errors_C[0] > conv.max_errors_C[1], "Error decreases from N=10 to N=20");
    TEST_ASSERT(conv.max_errors_C[1] > conv.max_errors_C[2], "Error decreases from N=20 to N=40");
    TEST_ASSERT(conv.max_errors_C[2] > conv.max_errors_C[3], "Error decreases from N=40 to N=80");
    
    // Check quadratic convergence rate O(h^2): h halved -> error drops by factor of ~4
    double ratio_1 = conv.max_errors_C[0] / conv.max_errors_C[1];
    TEST_ASSERT(std::abs(ratio_1 - 4.0) < 0.1, "Mesh convergence exhibits theoretical quadratic O(h^2) rate (~4x error reduction)");

    // 4. Invalid input rejection
    uew::core::ThermalParams bad_p = p;
    bad_p.num_elements = 0;
    TEST_ASSERT(!uew::core::solve_thermal_1d(bad_p).success, "Rejects N=0 element count");

    bad_p = p;
    bad_p.conductivity_W_mK = -5.0;
    TEST_ASSERT(!uew::core::solve_thermal_1d(bad_p).success, "Rejects negative thermal conductivity");

    bad_p = p;
    bad_p.length_m = 0.0;
    TEST_ASSERT(!uew::core::solve_thermal_1d(bad_p).success, "Rejects zero length");

    bad_p = p;
    bad_p.heat_gen_W_m3 = std::numeric_limits<double>::quiet_NaN();
    TEST_ASSERT(!uew::core::solve_thermal_1d(bad_p).success, "Rejects NaN parameter input");
}

// -------------------------------------------------------------
// Test Suite 2: CAD STL & ANSYS CSV Parsers (Task B)
// -------------------------------------------------------------
void test_geometry_parsers() {
    std::cout << "\n=== [TEST SUITE 2] CAD STL & ANSYS CSV Parser Tests ===\n";

    // 1. Valid STL Parsing
    auto stl_res = uew::geometry::parse_ascii_stl_file("data/example_geometry.stl");
    TEST_ASSERT(stl_res.success, "Valid ASCII STL parses successfully");
    TEST_ASSERT(stl_res.facet_count == 12, "Example STL contains 12 triangular facets (standard cube/prism)");
    TEST_ASSERT(stl_res.positions.size() == 12 * 9, "Flat position buffer contains 108 floats");
    TEST_ASSERT(stl_res.total_surface_area > 0.0, "Total surface area strictly positive");

    // 2. Empty STL rejection
    auto empty_stl = uew::geometry::parse_ascii_stl_string("");
    TEST_ASSERT(!empty_stl.success, "Rejects empty 0-byte STL with clear diagnostic");

    // 3. Malformed STL rejection
    std::string malformed_stl = "solid test\nfacet normal 0 0 0\nouter loop\nvertex 1 2\nendloop\nendfacet\nendsolid";
    auto malformed_res = uew::geometry::parse_ascii_stl_string(malformed_stl);
    TEST_ASSERT(!malformed_res.success, "Rejects malformed STL with missing vertex coordinates");

    // 4. Valid ANSYS CSV parsing
    auto csv_res = uew::geometry::parse_temperature_csv_file("data/example_temperature.csv");
    TEST_ASSERT(csv_res.success, "Valid synthetic ANSYS CSV parses successfully");
    TEST_ASSERT(csv_res.total_nodes > 0, "Parsed records count is greater than zero");
    TEST_ASSERT(csv_res.min_temperature_C < csv_res.max_temperature_C, "Min temperature < Max temperature");
    TEST_ASSERT(csv_res.point_coords.size() == csv_res.total_nodes * 3, "Point coordinate buffer matches 3x node count");

    // 5. Missing CSV header column rejection
    std::string bad_csv_header = "node_id,x_m,y_m,temperature_C\n1,0,0,100\n";
    auto bad_header_res = uew::geometry::parse_temperature_csv_string(bad_csv_header);
    TEST_ASSERT(!bad_header_res.success, "Rejects CSV with missing column (z_m omitted)");

    // 6. CSV NaN / Non-finite value rejection
    std::string nan_csv = "node_id,x_m,y_m,z_m,temperature_C\n1,0.0,0.0,0.0,NaN\n";
    auto nan_res = uew::geometry::parse_temperature_csv_string(nan_csv);
    TEST_ASSERT(!nan_res.success, "Rejects CSV containing NaN values");
}

// -------------------------------------------------------------
// Test Suite 3: Multicore Batch Simulation (Task D)
// -------------------------------------------------------------
void test_multicore_engine() {
    std::cout << "\n=== [TEST SUITE 3] Multicore Batch Simulation Tests ===\n";

    // 1. Hardware detection
    auto hw_info = uew::performance::get_system_hardware_info();
    TEST_ASSERT(hw_info.logical_cores >= 1, "Detected >= 1 logical CPU core");
    TEST_ASSERT(!hw_info.cpu_model.empty(), "CPU model string detected");

    // 2. Deterministic batch generation
    auto batch1 = uew::performance::generate_thermal_batch(50, 20, 42);
    auto batch2 = uew::performance::generate_thermal_batch(50, 20, 42);
    TEST_ASSERT(batch1.size() == 50, "Batch generator produces configured case count");
    TEST_ASSERT(batch1[10].params.conductivity_W_mK == batch2[10].params.conductivity_W_mK, 
                "Batch generator is strictly deterministic with fixed seed");

    // 3. Serial vs Parallel parity
    auto serial_out = uew::performance::solve_batch_serial(batch1);
    auto parallel_out = uew::performance::solve_batch_parallel(batch1, 2);

    TEST_ASSERT(serial_out.size() == parallel_out.size(), "Serial and parallel outputs match in size");

    double max_diff = 0.0;
    for (size_t i = 0; i < serial_out.size(); ++i) {
        double d = std::abs(serial_out[i].max_temp_C - parallel_out[i].max_temp_C);
        if (d > max_diff) max_diff = d;
    }
    TEST_ASSERT(max_diff <= 1e-8, "Maximum temperature difference between serial and parallel <= 1e-8 C requirement");

    // 4. Benchmark execution
    auto bench = uew::performance::run_multicore_benchmark(100, 20, 2);
    TEST_ASSERT(bench.numerical_agreement_passed, "Benchmark suite verifies serial/parallel agreement pass");
    TEST_ASSERT(!bench.worker_results.empty(), "Benchmark records worker results");
}

// -------------------------------------------------------------
// Test Suite 4: Embedded Hardware Simulators (Task E)
// -------------------------------------------------------------
void test_hardware_simulation() {
    std::cout << "\n=== [TEST SUITE 4] Embedded Hardware Simulation Tests ===\n";

    uew::hardware::HardwareManager mgr;
    auto devices = mgr.list_devices();
    TEST_ASSERT(devices.size() == 3, "Discovered exactly 3 simulated hardware devices (MCU, FPGA, JTAG)");

    auto mcu = mgr.get_device("DEV_MCU_STM32H7");
    auto fpga = mgr.get_device("DEV_FPGA_ARTIX7");
    auto jtag = mgr.get_device("DEV_DEBUG_JTAG");

    TEST_ASSERT(mcu != nullptr, "MCU adapter successfully instantiated");
    TEST_ASSERT(fpga != nullptr, "FPGA adapter successfully instantiated");
    TEST_ASSERT(jtag != nullptr, "JTAG adapter successfully instantiated");

    // Connect & Telemetry
    TEST_ASSERT(mcu->connect(), "MCU connects cleanly");
    auto mcu_tel = mcu->poll_telemetry();
    TEST_ASSERT(!mcu_tel.error_flag, "MCU telemetry streaming nominal");
    TEST_ASSERT(mcu_tel.core_voltage_V > 1.0 && mcu_tel.core_voltage_V < 1.5, "MCU core voltage within 1.25V window");

    // Recoverable failure test (Timeout injection)
    mcu->inject_fault(true);
    auto fault_tel = mcu->poll_telemetry();
    TEST_ASSERT(fault_tel.error_flag, "Fault injection triggers simulated timeout / communication error");
    TEST_ASSERT(mcu->get_status() == uew::hardware::ConnectionStatus::Error_Timeout, "Status transitions to Error_Timeout");

    // Recovery
    TEST_ASSERT(mcu->recover_from_fault(), "Device executes successful error recovery");
    TEST_ASSERT(mcu->get_status() == uew::hardware::ConnectionStatus::Connected, "Device returns to Connected state");
    TEST_ASSERT(mcu->disconnect(), "MCU disconnects cleanly");
}

// -------------------------------------------------------------
// Test Suite 5: Backpropagation & Gradients (Task F)
// -------------------------------------------------------------
void test_backpropagation() {
    std::cout << "\n=== [TEST SUITE 5] Backpropagation Tests ===\n";

    const double x = 1.0;
    const double y = 2.0;
    uew::neural::NetworkParameters p = {0.5, 0.0, 1.0, 0.0};

    // 1. Forward pass numerical check
    auto fwd = uew::neural::forward_pass(x, y, p);
    // z = 0.5, h = tanh(0.5) ~ 0.46211716, y_hat = 0.46211716, diff = -1.53788284, J = 0.5*diff^2 ~ 1.18254181
    TEST_ASSERT(std::abs(fwd.z - 0.5) < 1e-12, "z == 0.5 exact");
    TEST_ASSERT(std::abs(fwd.h - 0.462117157) < 1e-6, "h == tanh(0.5) ~ 0.462117 exact");
    TEST_ASSERT(std::abs(fwd.loss_J - 1.1825418) < 1e-5, "Initial Loss J ~ 1.18254 exact");

    // 2. Analytical gradients check
    auto grads = uew::neural::compute_analytical_gradients(fwd, p);
    TEST_ASSERT(grads.dJ_dc < 0.0, "dJ/dc is negative (pushing prediction towards 2.0)");
    TEST_ASSERT(grads.dJ_dv < 0.0, "dJ/dv is negative");
    TEST_ASSERT(grads.dJ_dw < 0.0, "dJ/dw is negative");
    TEST_ASSERT(grads.dJ_db < 0.0, "dJ/db is negative");

    // 3. Central Finite Difference Validation (epsilon = 1e-6, error <= 1e-5)
    auto checks = uew::neural::verify_gradients_finite_difference(x, y, p, grads, 1.0e-6);
    TEST_ASSERT(checks.size() == 4, "Finite difference executed for all 4 parameters (w, b, v, c)");
    for (const auto& chk : checks) {
        std::ostringstream description;
        description << "Finite diff check for " << chk.parameter_name
                    << " (error: " << std::scientific << std::setprecision(12)
                    << chk.absolute_error << " <= 1e-5)";
        std::string desc = description.str();
        TEST_ASSERT(chk.passed, desc);
    }

    // 4. 100-step training loop loss decay
    auto full_study = uew::neural::run_full_backpropagation_study(0.1, 100);
    TEST_ASSERT(full_study.training_history.size() == 101, "Recorded 100 training steps (steps 0 to 100)");
    TEST_ASSERT(full_study.final_loss < full_study.initial_forward.loss_J, "Loss strictly decreased over training");
    TEST_ASSERT(full_study.final_loss < 0.05, "Loss converged towards zero (J_100 < 0.05)");
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "  UNIFIED ENGINEERING WORKBENCH - VERIFICATION SUITE   \n";
    std::cout << "  PASS SCIENCE WORKS | R&D INTERNSHIP 2026-2027        \n";
    std::cout << "========================================================\n";

    test_thermal_solver();
    test_geometry_parsers();
    test_multicore_engine();
    test_hardware_simulation();
    test_backpropagation();

    std::cout << "\n========================================================\n";
    std::cout << "  TEST SUMMARY: " << g_tests_passed << " PASSED, " << g_tests_failed << " FAILED\n";
    std::cout << "========================================================\n";

    return (g_tests_failed == 0) ? 0 : 1;
}
