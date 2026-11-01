#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace uew {

// Dense Gaussian elimination with partial pivoting.
// Intended for the small FEM systems in this workbench (tens to a few hundred unknowns).
inline bool solve_dense(std::vector<double>& a,
                        std::vector<double>& b,
                        int n,
                        std::vector<double>& x,
                        std::string& error) {
    if (n <= 0) {
        error = "Linear system size must be positive.";
        return false;
    }
    if (static_cast<int>(a.size()) != n * n || static_cast<int>(b.size()) != n) {
        error = "Linear system storage size does not match n.";
        return false;
    }
    for (double v : a) {
        if (!std::isfinite(v)) {
            error = "Stiffness matrix contains a non-finite value.";
            return false;
        }
    }
    for (double v : b) {
        if (!std::isfinite(v)) {
            error = "Load vector contains a non-finite value.";
            return false;
        }
    }

    std::vector<int> pivot(n);
    for (int i = 0; i < n; ++i) pivot[i] = i;

    auto A = [&](int r, int c) -> double& { return a[r * n + c]; };

    for (int k = 0; k < n; ++k) {
        int best = k;
        double best_val = std::abs(A(k, k));
        for (int i = k + 1; i < n; ++i) {
            double v = std::abs(A(i, k));
            if (v > best_val) {
                best_val = v;
                best = i;
            }
        }
        if (best_val < 1e-18) {
            error = "Singular or nearly singular linear system.";
            return false;
        }
        if (best != k) {
            for (int c = 0; c < n; ++c) std::swap(A(k, c), A(best, c));
            std::swap(b[k], b[best]);
            std::swap(pivot[k], pivot[best]);
        }
        for (int i = k + 1; i < n; ++i) {
            double factor = A(i, k) / A(k, k);
            A(i, k) = factor;
            for (int j = k + 1; j < n; ++j) {
                A(i, j) -= factor * A(k, j);
            }
            b[i] -= factor * b[k];
        }
    }

    x.assign(n, 0.0);
    for (int i = n - 1; i >= 0; --i) {
        double sum = b[i];
        for (int j = i + 1; j < n; ++j) sum -= A(i, j) * x[j];
        x[i] = sum / A(i, i);
        if (!std::isfinite(x[i])) {
            error = "Non-finite unknown appeared during back substitution.";
            return false;
        }
    }
    error.clear();
    return true;
}

}  // namespace uew
