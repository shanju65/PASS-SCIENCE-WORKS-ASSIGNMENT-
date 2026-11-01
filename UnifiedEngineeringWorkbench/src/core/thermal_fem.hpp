#pragma once

#include "linear_solver.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace uew {

struct ThermalParams {
    double L = 1.0;           // m
    double A = 1.0e-4;        // m^2
    double k = 10.0;          // W/(m K)
    double q = 10000.0;       // W/m^3
    double T_left = 100.0;    // deg C at x=0
    double T_right = 20.0;    // deg C at x=L
    int n_elements = 10;
};

struct ThermalResult {
    bool ok = false;
    std::string error;
    ThermalParams params;
    std::vector<double> x_nodes_m;
    std::vector<double> T_nodes_C;
    std::vector<double> T_exact_nodes_C;
    std::vector<double> x_mid_m;
    std::vector<double> T_mid_C;
    std::vector<double> T_exact_mid_C;
    std::vector<double> abs_err_mid_C;
    double max_abs_err_mid_C = 0;
    double max_abs_err_nodes_C = 0;
};

inline bool finite_positive(double v) { return std::isfinite(v) && v > 0.0; }

inline std::string validate_params(const ThermalParams& p) {
    if (!finite_positive(p.L)) return "Length L must be a finite positive value in metres.";
    if (!finite_positive(p.A)) return "Area A must be a finite positive value in m^2.";
    if (!finite_positive(p.k)) return "Conductivity k must be a finite positive value in W/(m K).";
    if (!std::isfinite(p.q) || p.q < 0.0) return "Heat generation q must be finite and >= 0 W/m^3.";
    if (!std::isfinite(p.T_left) || !std::isfinite(p.T_right)) return "Boundary temperatures must be finite.";
    if (p.n_elements < 1 || p.n_elements > 5000) return "Element count N must be between 1 and 5000.";
    return {};
}

// Closed-form solution of -k T''(x) = q with T(0)=T_left, T(L)=T_right.
inline double analytical_temperature(double x, const ThermalParams& p) {
    const double slope = (p.T_right - p.T_left) / p.L;
    return p.T_left + slope * x + (p.q / (2.0 * p.k)) * x * (p.L - x);
}

inline ThermalResult solve_thermal(const ThermalParams& params) {
    ThermalResult result;
    result.params = params;
    result.error = validate_params(params);
    if (!result.error.empty()) return result;

    const int n_el = params.n_elements;
    const int n_nodes = n_el + 1;
    const double le = params.L / static_cast<double>(n_el);
    const double ke_coeff = (params.k * params.A) / le;
    const double fe_coeff = (params.q * params.A * le) / 2.0;

    std::vector<double> K(n_nodes * n_nodes, 0.0);
    std::vector<double> F(n_nodes, 0.0);
    result.x_nodes_m.resize(n_nodes);
    for (int i = 0; i < n_nodes; ++i) {
        result.x_nodes_m[i] = le * static_cast<double>(i);
    }

    // Assemble 2-node linear elements.
    for (int e = 0; e < n_el; ++e) {
        const int n1 = e;
        const int n2 = e + 1;
        K[n1 * n_nodes + n1] += ke_coeff;
        K[n1 * n_nodes + n2] -= ke_coeff;
        K[n2 * n_nodes + n1] -= ke_coeff;
        K[n2 * n_nodes + n2] += ke_coeff;
        F[n1] += fe_coeff;
        F[n2] += fe_coeff;
    }

    auto apply_dirichlet = [&](int node, double value) {
        for (int j = 0; j < n_nodes; ++j) {
            K[node * n_nodes + j] = 0.0;
        }
        K[node * n_nodes + node] = 1.0;
        F[node] = value;
    };
    apply_dirichlet(0, params.T_left);
    apply_dirichlet(n_nodes - 1, params.T_right);

    std::string solve_error;
    if (!solve_dense(K, F, n_nodes, result.T_nodes_C, solve_error)) {
        result.error = solve_error;
        return result;
    }

    result.T_exact_nodes_C.resize(n_nodes);
    result.max_abs_err_nodes_C = 0;
    for (int i = 0; i < n_nodes; ++i) {
        result.T_exact_nodes_C[i] = analytical_temperature(result.x_nodes_m[i], params);
        result.max_abs_err_nodes_C = std::max(
            result.max_abs_err_nodes_C,
            std::abs(result.T_nodes_C[i] - result.T_exact_nodes_C[i]));
    }

    result.x_mid_m.resize(n_el);
    result.T_mid_C.resize(n_el);
    result.T_exact_mid_C.resize(n_el);
    result.abs_err_mid_C.resize(n_el);
    result.max_abs_err_mid_C = 0;
    for (int e = 0; e < n_el; ++e) {
        const double xmid = 0.5 * (result.x_nodes_m[e] + result.x_nodes_m[e + 1]);
        // Linear FE interpolation at the element midpoint.
        const double Tmid = 0.5 * (result.T_nodes_C[e] + result.T_nodes_C[e + 1]);
        const double Tex = analytical_temperature(xmid, params);
        result.x_mid_m[e] = xmid;
        result.T_mid_C[e] = Tmid;
        result.T_exact_mid_C[e] = Tex;
        result.abs_err_mid_C[e] = std::abs(Tmid - Tex);
        result.max_abs_err_mid_C = std::max(result.max_abs_err_mid_C, result.abs_err_mid_C[e]);
    }

    result.ok = true;
    return result;
}

inline std::string thermal_result_csv(const ThermalResult& r) {
    std::ostringstream os;
    os.setf(std::ios::fmtflags(0), std::ios::floatfield);
    os.precision(12);
    os << "# SYNTHETIC_OR_COMPUTED thermal FEM export\n";
    os << "# units: x_m [m], temperature_C [deg C]\n";
    os << "kind,element_or_node,x_m,temperature_C,analytical_C,abs_error_C\n";
    if (!r.ok) return os.str();
    for (std::size_t i = 0; i < r.x_nodes_m.size(); ++i) {
        os << "node," << i << ',' << r.x_nodes_m[i] << ',' << r.T_nodes_C[i] << ','
           << r.T_exact_nodes_C[i] << ',' << std::abs(r.T_nodes_C[i] - r.T_exact_nodes_C[i]) << '\n';
    }
    for (std::size_t e = 0; e < r.x_mid_m.size(); ++e) {
        os << "midpoint," << e << ',' << r.x_mid_m[e] << ',' << r.T_mid_C[e] << ','
           << r.T_exact_mid_C[e] << ',' << r.abs_err_mid_C[e] << '\n';
    }
    return os.str();
}

inline std::string analytical_check_report() {
    ThermalParams p;
    // Independent check: ODE -k T'' = q.
    // At x=L/2, T = 100 - 40 + (q/(2k))*(L^2/4) = 60 + 10000/(20)*0.25 = 60 + 125 = 185.
    const double mid = analytical_temperature(0.5, p);
    std::ostringstream os;
    os.precision(12);
    os << "T_exact(L/2)=" << mid << " C (expected 185 C for the assignment constants)";
    return os.str();
}

}  // namespace uew
