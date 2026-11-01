#include "thermal_solver.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace uew::core {

double evaluate_analytical_solution(double x, const ThermalParams& p) {
    if (p.length_m <= 0.0) return 0.0;
    // T_exact(x) = T_0 - (T_0 - T_L)*(x/L) + [q / (2*k)] * x * (L - x)
    const double delta_T = p.temp_left_C - p.temp_right_C;
    const double linear_term = p.temp_left_C - delta_T * (x / p.length_m);
    const double source_term = (p.heat_gen_W_m3 / (2.0 * p.conductivity_W_mK)) * x * (p.length_m - x);
    return linear_term + source_term;
}

ThermalResult solve_thermal_1d(const ThermalParams& p) {
    ThermalResult res;
    
    // 1. Rigorous parameter validation
    if (p.num_elements < 1) {
        res.success = false;
        res.error_message = "Invalid element count: must have at least 1 element.";
        return res;
    }
    if (p.length_m <= 0.0 || !std::isfinite(p.length_m)) {
        res.success = false;
        res.error_message = "Invalid length: L must be positive and finite.";
        return res;
    }
    if (p.area_m2 <= 0.0 || !std::isfinite(p.area_m2)) {
        res.success = false;
        res.error_message = "Invalid area: A must be positive and finite.";
        return res;
    }
    if (p.conductivity_W_mK <= 0.0 || !std::isfinite(p.conductivity_W_mK)) {
        res.success = false;
        res.error_message = "Invalid thermal conductivity: k must be positive and finite.";
        return res;
    }
    if (!std::isfinite(p.heat_gen_W_m3) || !std::isfinite(p.temp_left_C) || !std::isfinite(p.temp_right_C)) {
        res.success = false;
        res.error_message = "Thermal parameters must be finite numbers (no NaN/Inf).";
        return res;
    }

    const int N = p.num_elements;
    const int num_nodes = N + 1;
    const double le = p.length_m / static_cast<double>(N);

    // 2. Uniform mesh node coordinates
    res.node_x.resize(num_nodes);
    res.node_temp_C.resize(num_nodes, 0.0);
    res.exact_temp_C.resize(num_nodes);

    for (int i = 0; i < num_nodes; ++i) {
        res.node_x[i] = i * le;
        res.exact_temp_C[i] = evaluate_analytical_solution(res.node_x[i], p);
    }

    // 3. Impose known Dirichlet boundary conditions at endpoints
    res.node_temp_C[0] = p.temp_left_C;
    res.node_temp_C[N] = p.temp_right_C;

    // If N == 1, only boundary nodes exist, solved immediately
    if (N > 1) {
        // Reduced interior system size: M = N - 1 (nodes 1 through N-1)
        const int M = N - 1;
        
        // Element coefficients
        // Ke = (kA / le) * [ 1, -1; -1, 1 ]
        // Fe = (q * A * le / 2) * [ 1, 1 ]^T
        const double k_elem = (p.conductivity_W_mK * p.area_m2) / le;
        const double f_elem = (p.heat_gen_W_m3 * p.area_m2 * le) / 2.0;

        // Tridiagonal system:
        // a: subdiagonal (size M-1)
        // b: main diagonal (size M)
        // c: superdiagonal (size M-1)
        // d: right-hand side load vector (size M)
        std::vector<double> a(M - 1, -k_elem);
        std::vector<double> b(M, 2.0 * k_elem);
        std::vector<double> c(M - 1, -k_elem);
        std::vector<double> d(M, 2.0 * f_elem); // Each interior node receives f_elem from left + f_elem from right

        // Modify right-hand side for Dirichlet boundary conditions at node 0 and node N
        d[0] += k_elem * p.temp_left_C;      // Subtracted from LHS: -(-k_elem * T_0) -> + k_elem * T_0
        d[M - 1] += k_elem * p.temp_right_C; // Subtracted from LHS: -(-k_elem * T_N) -> + k_elem * T_N

        // Thomas algorithm (Tridiagonal Matrix Algorithm) for O(M) solution
        // Forward elimination
        std::vector<double> c_prime(M - 1, 0.0);
        std::vector<double> d_prime(M, 0.0);

        c_prime[0] = c[0] / b[0];
        d_prime[0] = d[0] / b[0];

        for (int i = 1; i < M; ++i) {
            const double denom = b[i] - a[i - 1] * c_prime[i - 1];
            if (std::abs(denom) < 1e-15) {
                res.success = false;
                res.error_message = "Numerical singularity in tridiagonal solver.";
                return res;
            }
            if (i < M - 1) {
                c_prime[i] = c[i] / denom;
            }
            d_prime[i] = (d[i] - a[i - 1] * d_prime[i - 1]) / denom;
        }

        // Back substitution
        std::vector<double> T_interior(M, 0.0);
        T_interior[M - 1] = d_prime[M - 1];
        for (int i = M - 2; i >= 0; --i) {
            T_interior[i] = d_prime[i] - c_prime[i] * T_interior[i + 1];
        }

        // Copy interior solutions to global node temperature vector
        for (int i = 0; i < M; ++i) {
            res.node_temp_C[i + 1] = T_interior[i];
        }
    }

    // 4. Element Midpoint Interpolation & Error Assessment
    // Task C requirement: "Report maximum absolute error at element midpoints using FE interpolation, not only at nodes"
    res.midpoints.resize(N);
    double max_err = 0.0;
    double sum_sq_err = 0.0;

    for (int e = 0; e < N; ++e) {
        const double x_mid = (res.node_x[e] + res.node_x[e + 1]) * 0.5;
        // Linear shape function interpolation at midpoint xi = 0: N1 = 0.5, N2 = 0.5
        const double temp_fem_mid = 0.5 * (res.node_temp_C[e] + res.node_temp_C[e + 1]);
        const double temp_exact_mid = evaluate_analytical_solution(x_mid, p);
        const double abs_err = std::abs(temp_fem_mid - temp_exact_mid);

        res.midpoints[e].element_index = e;
        res.midpoints[e].x_mid_m = x_mid;
        res.midpoints[e].temp_fem_C = temp_fem_mid;
        res.midpoints[e].temp_exact_C = temp_exact_mid;
        res.midpoints[e].abs_error_C = abs_err;

        if (abs_err > max_err) {
            max_err = abs_err;
        }
        sum_sq_err += abs_err * abs_err;
    }

    res.max_midpoint_abs_error_C = max_err;
    res.l2_error_norm = std::sqrt(sum_sq_err / static_cast<double>(N));
    res.success = true;
    return res;
}

ConvergenceReport run_mesh_convergence_study(ThermalParams base_params) {
    ConvergenceReport report;
    report.mesh_sizes = {10, 20, 40, 80};
    report.max_errors_C.clear();
    report.element_lengths_m.clear();
    report.results.clear();

    for (int n : report.mesh_sizes) {
        base_params.num_elements = n;
        ThermalResult r = solve_thermal_1d(base_params);
        report.results.push_back(r);
        report.max_errors_C.push_back(r.max_midpoint_abs_error_C);
        report.element_lengths_m.push_back(base_params.length_m / static_cast<double>(n));
    }
    return report;
}

std::string export_thermal_csv(const ThermalResult& res) {
    std::ostringstream oss;
    oss << "node_index,x_m,temp_fem_C,temp_exact_C,abs_error_C\n";
    oss << std::fixed << std::setprecision(8);
    for (size_t i = 0; i < res.node_x.size(); ++i) {
        const double abs_err = std::abs(res.node_temp_C[i] - res.exact_temp_C[i]);
        oss << i << "," << res.node_x[i] << "," << res.node_temp_C[i] << "," 
            << res.exact_temp_C[i] << "," << abs_err << "\n";
    }

    oss << "\n# ELEMENT MIDPOINT EVALUATION (FE LINEAR INTERPOLATION)\n";
    oss << "element_index,x_mid_m,temp_fem_mid_C,temp_exact_mid_C,midpoint_abs_error_C\n";
    for (const auto& mp : res.midpoints) {
        oss << mp.element_index << "," << mp.x_mid_m << "," << mp.temp_fem_C << ","
            << mp.temp_exact_C << "," << mp.abs_error_C << "\n";
    }
    return oss.str();
}

} // namespace uew::core
