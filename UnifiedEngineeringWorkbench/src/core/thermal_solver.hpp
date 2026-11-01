#ifndef UEW_CORE_THERMAL_SOLVER_HPP
#define UEW_CORE_THERMAL_SOLVER_HPP

#include <vector>
#include <string>

namespace uew::core {

/**
 * @brief Input parameters for 1D Steady-State Thermal Finite Element Analysis.
 * Governing differential equation: -d/dx( k * A * dT/dx ) = q * A
 * Boundary conditions: T(0) = T_left, T(L) = T_right
 */
struct ThermalParams {
    double length_m = 1.0;            // L: Rod length in meters (default 1.0 m)
    double area_m2 = 1.0e-4;          // A: Cross-sectional area in m^2 (default 1e-4 m^2)
    double conductivity_W_mK = 10.0;  // k: Thermal conductivity in W/(m*K) (default 10 W/(m*K))
    double heat_gen_W_m3 = 10000.0;   // q: Volumetric heat generation in W/m^3 (default 10000 W/m^3)
    double temp_left_C = 100.0;       // T(0): Prescribed Dirichlet temperature at x=0 (default 100 C)
    double temp_right_C = 20.0;       // T(L): Prescribed Dirichlet temperature at x=L (default 20 C)
    int num_elements = 10;            // N: Number of uniform 2-node linear elements
};

/**
 * @brief Results for a single element midpoint evaluation.
 */
struct MidpointError {
    int element_index = 0;
    double x_mid_m = 0.0;
    double temp_fem_C = 0.0;
    double temp_exact_C = 0.0;
    double abs_error_C = 0.0;
};

/**
 * @brief Complete results of a 1D Thermal FEA solve.
 */
struct ThermalResult {
    bool success = false;
    std::string error_message;
    
    // Mesh definition
    std::vector<double> node_x;         // Node coordinate positions in meters
    std::vector<double> node_temp_C;    // FE nodal temperature results in Celsius
    std::vector<double> exact_temp_C;   // Analytical exact temperature at nodes in Celsius
    
    // Midpoint evaluations (evaluated using element shape functions)
    std::vector<MidpointError> midpoints;
    double max_midpoint_abs_error_C = 0.0;
    double l2_error_norm = 0.0;
};

/**
 * @brief Evaluates the exact closed-form analytical solution:
 * T_exact(x) = T_0 - (T_0 - T_L)*(x/L) + [q / (2*k)] * x * (L - x)
 */
double evaluate_analytical_solution(double x, const ThermalParams& params);

/**
 * @brief Solves the 1D steady-state heat equation using uniform 2-node linear finite elements.
 */
ThermalResult solve_thermal_1d(const ThermalParams& params);

/**
 * @brief Performs convergence study across N = 10, 20, 40, 80 elements.
 */
struct ConvergenceReport {
    std::vector<int> mesh_sizes = {10, 20, 40, 80};
    std::vector<double> max_errors_C;
    std::vector<double> element_lengths_m;
    std::vector<ThermalResult> results;
};

ConvergenceReport run_mesh_convergence_study(ThermalParams base_params);

/**
 * @brief Generates CSV string containing the thermal simulation results.
 */
std::string export_thermal_csv(const ThermalResult& result);

} // namespace uew::core

#endif // UEW_CORE_THERMAL_SOLVER_HPP
