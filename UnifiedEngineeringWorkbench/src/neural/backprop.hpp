#pragma once

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace uew {

struct NetworkParams {
    double w = 0.5;
    double b = 0.0;
    double v = 1.0;
    double c = 0.0;
};

struct BackpropConfig {
    double x = 1.0;
    double y = 2.0;
    double eta = 0.1;
    NetworkParams theta;
    int steps = 100;
    double fd_epsilon = 1e-6;
};

struct Gradients {
    double dw = 0, db = 0, dv = 0, dc = 0;
};

struct ForwardPass {
    double z = 0;      // w*x + b
    double h = 0;      // tanh(z)
    double y_hat = 0;  // v*h + c
    double J = 0;      // 0.5 (y_hat - y)^2
};

inline ForwardPass forward(const BackpropConfig& cfg, const NetworkParams& theta) {
    ForwardPass f;
    f.z = theta.w * cfg.x + theta.b;
    f.h = std::tanh(f.z);
    f.y_hat = theta.v * f.h + theta.c;
    const double e = f.y_hat - cfg.y;
    f.J = 0.5 * e * e;
    return f;
}

// Manual chain-rule gradients at the supplied parameters. Not automatic differentiation.
inline Gradients analytical_gradients(const BackpropConfig& cfg, const NetworkParams& theta) {
    const ForwardPass f = forward(cfg, theta);
    const double dJ_dyhat = (f.y_hat - cfg.y);          // d/d y_hat of 0.5 e^2
    const double sech2 = 1.0 - f.h * f.h;               // d tanh(z) / dz
    Gradients g;
    g.dc = dJ_dyhat * 1.0;
    g.dv = dJ_dyhat * f.h;
    g.db = dJ_dyhat * theta.v * sech2 * 1.0;
    g.dw = dJ_dyhat * theta.v * sech2 * cfg.x;
    return g;
}

inline double loss_with_perturbed(const BackpropConfig& cfg, NetworkParams theta, double NetworkParams::* member, double delta) {
    theta.*member += delta;
    return forward(cfg, theta).J;
}

inline Gradients finite_difference_gradients(const BackpropConfig& cfg, const NetworkParams& theta, double eps) {
    Gradients g;
    auto fd = [&](double NetworkParams::* member) {
        const double jp = loss_with_perturbed(cfg, theta, member, eps);
        const double jm = loss_with_perturbed(cfg, theta, member, -eps);
        return (jp - jm) / (2.0 * eps);
    };
    g.dw = fd(&NetworkParams::w);
    g.db = fd(&NetworkParams::b);
    g.dv = fd(&NetworkParams::v);
    g.dc = fd(&NetworkParams::c);
    return g;
}

struct TrainPoint {
    int step = 0;
    double J = 0;
    double y_hat = 0;
    NetworkParams theta;
};

struct BackpropResult {
    bool ok = false;
    std::string error;
    ForwardPass initial_forward;
    Gradients analytic;
    Gradients finite_diff;
    Gradients abs_grad_error;
    bool fd_pass = false;
    NetworkParams updated;
    std::vector<TrainPoint> history;
    double final_loss = 0;
};

inline NetworkParams gradient_descent_step(const NetworkParams& theta, const Gradients& g, double eta) {
    NetworkParams n = theta;
    n.w -= eta * g.dw;
    n.b -= eta * g.db;
    n.v -= eta * g.dv;
    n.c -= eta * g.dc;
    return n;
}

inline BackpropResult run_backprop(BackpropConfig cfg) {
    BackpropResult r;
    if (!std::isfinite(cfg.x) || !std::isfinite(cfg.y) || !std::isfinite(cfg.eta) || cfg.eta <= 0) {
        r.error = "x, y must be finite and learning rate eta must be positive.";
        return r;
    }
    if (cfg.steps < 1 || cfg.steps > 10000) {
        r.error = "Training step count must be between 1 and 10000.";
        return r;
    }
    if (!(cfg.fd_epsilon > 0 && cfg.fd_epsilon < 1e-2)) {
        r.error = "Finite-difference epsilon is out of range.";
        return r;
    }

    r.initial_forward = forward(cfg, cfg.theta);
    r.analytic = analytical_gradients(cfg, cfg.theta);
    r.finite_diff = finite_difference_gradients(cfg, cfg.theta, cfg.fd_epsilon);
    r.abs_grad_error.dw = std::abs(r.analytic.dw - r.finite_diff.dw);
    r.abs_grad_error.db = std::abs(r.analytic.db - r.finite_diff.db);
    r.abs_grad_error.dv = std::abs(r.analytic.dv - r.finite_diff.dv);
    r.abs_grad_error.dc = std::abs(r.analytic.dc - r.finite_diff.dc);
    r.fd_pass = r.abs_grad_error.dw <= 1e-5 && r.abs_grad_error.db <= 1e-5 &&
                r.abs_grad_error.dv <= 1e-5 && r.abs_grad_error.dc <= 1e-5;

    // One simultaneous update from the original parameters.
    r.updated = gradient_descent_step(cfg.theta, r.analytic, cfg.eta);

    NetworkParams theta = cfg.theta;
    r.history.reserve(cfg.steps + 1);
    for (int s = 0; s <= cfg.steps; ++s) {
        ForwardPass f = forward(cfg, theta);
        TrainPoint p;
        p.step = s;
        p.J = f.J;
        p.y_hat = f.y_hat;
        p.theta = theta;
        r.history.push_back(p);
        if (s == cfg.steps) break;
        Gradients g = analytical_gradients(cfg, theta);
        theta = gradient_descent_step(theta, g, cfg.eta);
    }
    r.final_loss = r.history.back().J;
    r.ok = true;
    return r;
}

inline std::string backprop_history_csv(const BackpropResult& r) {
    std::ostringstream os;
    os.precision(12);
    os << "# Handwritten-backprop training history. Single sample regression.\n";
    os << "step,loss_J,y_hat,w,b,v,c\n";
    for (const auto& p : r.history) {
        os << p.step << ',' << p.J << ',' << p.y_hat << ',' << p.theta.w << ',' << p.theta.b << ','
           << p.theta.v << ',' << p.theta.c << '\n';
    }
    return os.str();
}

}  // namespace uew
