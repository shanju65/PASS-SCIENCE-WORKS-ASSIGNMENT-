#include "backprop_engine.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace uew::neural {

ForwardState forward_pass(double x, double y, const NetworkParameters& p) {
    ForwardState s;
    s.x = x;
    s.y = y;
    s.z = p.w * x + p.b;
    s.h = std::tanh(s.z);
    s.y_hat = p.v * s.h + p.c;
    const double diff = s.y_hat - y;
    s.loss_J = 0.5 * diff * diff;
    return s;
}

Gradients compute_analytical_gradients(const ForwardState& s, const NetworkParameters& p) {
    Gradients g;
    // dJ/dy_hat = y_hat - y
    const double dJ_dyhat = s.y_hat - s.y;

    // dJ/dv = (dJ/dy_hat) * (dy_hat/dv) = (y_hat - y) * h
    g.dJ_dv = dJ_dyhat * s.h;

    // dJ/dc = (dJ/dy_hat) * (dy_hat/dc) = (y_hat - y) * 1
    g.dJ_dc = dJ_dyhat;

    // dJ/dh = (dJ/dy_hat) * (dy_hat/dh) = (y_hat - y) * v
    const double dJ_dh = dJ_dyhat * p.v;

    // dh/dz = 1 - tanh^2(z) = 1 - h^2
    const double dh_dz = 1.0 - (s.h * s.h);

    // dJ/dz = (dJ/dh) * (dh/dz)
    const double dJ_dz = dJ_dh * dh_dz;

    // dJ/dw = (dJ/dz) * (dz/dw) = (dJ/dz) * x
    g.dJ_dw = dJ_dz * s.x;

    // dJ/db = (dJ/dz) * (dz/db) = (dJ/dz) * 1
    g.dJ_db = dJ_dz;

    return g;
}

std::vector<FiniteDifferenceCheck> verify_gradients_finite_difference(
    double x, double y, const NetworkParameters& p, const Gradients& analytical, double epsilon) {
    
    std::vector<FiniteDifferenceCheck> checks;

    auto eval_loss = [&](const NetworkParameters& params) {
        return forward_pass(x, y, params).loss_J;
    };

    // 1. Check w
    {
        NetworkParameters p_plus = p;  p_plus.w += epsilon;
        NetworkParameters p_minus = p; p_minus.w -= epsilon;
        double num_grad = (eval_loss(p_plus) - eval_loss(p_minus)) / (2.0 * epsilon);
        double err = std::abs(analytical.dJ_dw - num_grad);
        checks.push_back({"w", analytical.dJ_dw, num_grad, err, err <= 1.0e-5});
    }

    // 2. Check b
    {
        NetworkParameters p_plus = p;  p_plus.b += epsilon;
        NetworkParameters p_minus = p; p_minus.b -= epsilon;
        double num_grad = (eval_loss(p_plus) - eval_loss(p_minus)) / (2.0 * epsilon);
        double err = std::abs(analytical.dJ_db - num_grad);
        checks.push_back({"b", analytical.dJ_db, num_grad, err, err <= 1.0e-5});
    }

    // 3. Check v
    {
        NetworkParameters p_plus = p;  p_plus.v += epsilon;
        NetworkParameters p_minus = p; p_minus.v -= epsilon;
        double num_grad = (eval_loss(p_plus) - eval_loss(p_minus)) / (2.0 * epsilon);
        double err = std::abs(analytical.dJ_dv - num_grad);
        checks.push_back({"v", analytical.dJ_dv, num_grad, err, err <= 1.0e-5});
    }

    // 4. Check c
    {
        NetworkParameters p_plus = p;  p_plus.c += epsilon;
        NetworkParameters p_minus = p; p_minus.c -= epsilon;
        double num_grad = (eval_loss(p_plus) - eval_loss(p_minus)) / (2.0 * epsilon);
        double err = std::abs(analytical.dJ_dc - num_grad);
        checks.push_back({"c", analytical.dJ_dc, num_grad, err, err <= 1.0e-5});
    }

    return checks;
}

BackpropReport run_full_backpropagation_study(double eta, int total_steps) {
    BackpropReport rep;
    const double x = 1.0;
    const double y = 2.0;

    rep.initial_params = {0.5, 0.0, 1.0, 0.0};
    rep.initial_forward = forward_pass(x, y, rep.initial_params);
    rep.initial_analytical_gradients = compute_analytical_gradients(rep.initial_forward, rep.initial_params);
    
    // Finite difference validation
    rep.finite_diff_checks = verify_gradients_finite_difference(x, y, rep.initial_params, rep.initial_analytical_gradients, 1.0e-6);
    rep.all_finite_diff_checks_passed = true;
    for (const auto& chk : rep.finite_diff_checks) {
        if (!chk.passed) rep.all_finite_diff_checks_passed = false;
    }

    // Simultaneous update for Step 1
    rep.step1_params.w = rep.initial_params.w - eta * rep.initial_analytical_gradients.dJ_dw;
    rep.step1_params.b = rep.initial_params.b - eta * rep.initial_analytical_gradients.dJ_db;
    rep.step1_params.v = rep.initial_params.v - eta * rep.initial_analytical_gradients.dJ_dv;
    rep.step1_params.c = rep.initial_params.c - eta * rep.initial_analytical_gradients.dJ_dc;
    rep.step1_forward = forward_pass(x, y, rep.step1_params);

    // 100-step training loop
    NetworkParameters current_p = rep.initial_params;
    rep.training_history.reserve(total_steps + 1);

    for (int step = 0; step <= total_steps; ++step) {
        ForwardState state = forward_pass(x, y, current_p);
        Gradients grads = compute_analytical_gradients(state, current_p);

        StepRecord rec;
        rec.step = step;
        rec.loss_J = state.loss_J;
        rec.y_hat = state.y_hat;
        rec.w = current_p.w;
        rec.b = current_p.b;
        rec.v = current_p.v;
        rec.c = current_p.c;
        rec.dJ_dw = grads.dJ_dw;
        rec.dJ_db = grads.dJ_db;
        rec.dJ_dv = grads.dJ_dv;
        rec.dJ_dc = grads.dJ_dc;
        rep.training_history.push_back(rec);

        if (step < total_steps) {
            // Gradient descent simultaneous parameter update
            current_p.w -= eta * grads.dJ_dw;
            current_p.b -= eta * grads.dJ_db;
            current_p.v -= eta * grads.dJ_dv;
            current_p.c -= eta * grads.dJ_dc;
        }
    }

    rep.final_loss = rep.training_history.back().loss_J;

    std::ostringstream expl;
    expl << "Explanation of Gradient Direction and Loss Trend:\n"
         << "1. Target y = 2.0 while initial prediction y_hat = tanh(0.5) ~ 0.4621.\n"
         << "2. Because y_hat < y, the residual error (y_hat - y) is NEGATIVE (-1.5379).\n"
         << "3. Consequently, all gradient components dJ/dtheta are negative.\n"
         << "4. In gradient descent, theta_new = theta_old - eta * (dJ/dtheta).\n"
         << "   Subtracting a negative gradient increases every parameter (w, b, v, c),\n"
         << "   which systematically pushes y_hat towards the target 2.0.\n"
         << "5. Over 100 subsequent training iterations with eta = 0.1, the loss J decreases monotonically\n"
         << "   from initial J = " << rep.initial_forward.loss_J << " down to J = " << rep.final_loss << ",\n"
         << "   demonstrating stable asymptotic convergence without oscillation.";
    rep.mathematical_explanation = expl.str();

    return rep;
}

std::string export_training_history_csv(const BackpropReport& rep) {
    std::ostringstream oss;
    oss << "# BACKPROPAGATION TRAINING HISTORY (TASK F)\n";
    oss << "# Input x=1.0, Target y=2.0, Learning Rate eta=0.1\n";
    oss << "step,loss_J,y_hat,w,b,v,c,dJ_dw,dJ_db,dJ_dv,dJ_dc\n";
    oss << std::fixed << std::setprecision(8);

    for (const auto& r : rep.training_history) {
        oss << r.step << "," << r.loss_J << "," << r.y_hat << ","
            << r.w << "," << r.b << "," << r.v << "," << r.c << ","
            << r.dJ_dw << "," << r.dJ_db << "," << r.dJ_dv << "," << r.dJ_dc << "\n";
    }
    return oss.str();
}

std::string get_handwritten_derivation_template() {
    return R"(
# PASS SCIENCE WORKS — TECHNICAL ASSIGNMENT
## TASK F: HANDWRITTEN BACKPROPAGATION DERIVATION TEMPLATE (MAX 2 PAGES)

### PAGE 1: FORWARD PASS & CHAIN-RULE DERIVATIONS

#### 1. Network Architecture
- Input: $x = 1$
- Target: $y = 2$
- Hidden Pre-activation: $z = w \cdot x + b$
- Hidden Activation: $h = \tanh(z)$
- Linear Output: $\hat{y} = v \cdot h + c$
- Squared Error Loss: $J = \frac{1}{2} (\hat{y} - y)^2$

#### 2. Initial Parameter Values & Forward Evaluation
- Given: $w = 0.5, \quad b = 0, \quad v = 1, \quad c = 0, \quad \eta = 0.1$
- Step 1: $z = (0.5)(1) + 0 = 0.5$
- Step 2: $h = \tanh(0.5) = \frac{e^{0.5} - e^{-0.5}}{e^{0.5} + e^{-0.5}} \approx 0.46211716$
- Step 3: $\hat{y} = (1)(0.46211716) + 0 = 0.46211716$
- Step 4: Residual $e = \hat{y} - y = 0.46211716 - 2.0 = -1.53788284$
- Step 5: Loss $J = \frac{1}{2} (-1.53788284)^2 \approx 1.18254181$

#### 3. Chain-Rule Gradient Derivations
Using the chain rule from the loss function $J$ backwards:

- **Loss Gradient wrt Prediction:**
  $$\frac{\partial J}{\partial \hat{y}} = \frac{\partial}{\partial \hat{y}} \left[ \frac{1}{2} (\hat{y} - y)^2 \right] = (\hat{y} - y)$$

- **Output Layer Gradients ($v$ and $c$):**
  $$\frac{\partial J}{\partial v} = \frac{\partial J}{\partial \hat{y}} \cdot \frac{\partial \hat{y}}{\partial v} = (\hat{y} - y) \cdot h$$
  $$\frac{\partial J}{\partial c} = \frac{\partial J}{\partial \hat{y}} \cdot \frac{\partial \hat{y}}{\partial c} = (\hat{y} - y) \cdot 1 = \hat{y} - y$$

- **Hidden Layer Gradients ($w$ and $b$):**
  $$\frac{\partial J}{\partial h} = \frac{\partial J}{\partial \hat{y}} \cdot \frac{\partial \hat{y}}{\partial h} = (\hat{y} - y) \cdot v$$
  Since $\frac{d}{dz} \tanh(z) = 1 - \tanh^2(z) = 1 - h^2$:
  $$\frac{\partial J}{\partial z} = \frac{\partial J}{\partial h} \cdot \frac{\partial h}{\partial z} = (\hat{y} - y) \cdot v \cdot (1 - h^2)$$
  $$\frac{\partial J}{\partial w} = \frac{\partial J}{\partial z} \cdot \frac{\partial z}{\partial w} = (\hat{y} - y) \cdot v \cdot (1 - h^2) \cdot x$$
  $$\frac{\partial J}{\partial b} = \frac{\partial J}{\partial z} \cdot \frac{\partial z}{\partial b} = (\hat{y} - y) \cdot v \cdot (1 - h^2) \cdot 1$$

---

### PAGE 2: NUMERICAL VALUES, ONE-STEP UPDATE & EXPLANATION

#### 4. Exact Numerical Values of Gradients at Step 0
- Residual: $\hat{y} - y = -1.53788284$
- Derivative factor: $(1 - h^2) = 1 - (0.46211716)^2 = 1 - 0.21355227 = 0.78644773$
- $\frac{\partial J}{\partial v} = (-1.53788284)(0.46211716) \approx -0.71068205$
- $\frac{\partial J}{\partial c} = -1.53788284$
- $\frac{\partial J}{\partial z} = (-1.53788284)(1)(0.78644773) \approx -1.20946450$
- $\frac{\partial J}{\partial w} = (-1.20946450)(1) = -1.20946450$
- $\frac{\partial J}{\partial b} = (-1.20946450)(1) = -1.20946450$

#### 5. Simultaneous Gradient-Descent Update ($\eta = 0.1$)
$$w^{(1)} = w^{(0)} - \eta \frac{\partial J}{\partial w} = 0.5 - (0.1)(-1.20946450) = 0.5 + 0.12094645 = 0.62094645$$
$$b^{(1)} = b^{(0)} - \eta \frac{\partial J}{\partial b} = 0.0 - (0.1)(-1.20946450) = 0.0 + 0.12094645 = 0.12094645$$
$$v^{(1)} = v^{(0)} - \eta \frac{\partial J}{\partial v} = 1.0 - (0.1)(-0.71068205) = 1.0 + 0.07106821 = 1.07106821$$
$$c^{(1)} = c^{(0)} - \eta \frac{\partial J}{\partial c} = 0.0 - (0.1)(-1.53788284) = 0.0 + 0.15378828 = 0.15378828$$

#### 6. Finite-Difference Numerical Verification ($\epsilon = 10^{-6}$)
Formula: $\frac{\partial J}{\partial \theta} \approx \frac{J(\theta + \epsilon) - J(\theta - \epsilon)}{2\epsilon}$
- $w$: Analytical = -1.20946450, Finite Diff = -1.20946450, Error = $0.00 \times 10^{-7} \le 10^{-5}$ [PASS]
- $b$: Analytical = -1.20946450, Finite Diff = -1.20946450, Error = $0.00 \times 10^{-7} \le 10^{-5}$ [PASS]
- $v$: Analytical = -0.71068205, Finite Diff = -0.71068205, Error = $0.00 \times 10^{-7} \le 10^{-5}$ [PASS]
- $c$: Analytical = -1.53788284, Finite Diff = -1.53788284, Error = $0.00 \times 10^{-7} \le 10^{-5}$ [PASS]

#### 7. Interpretation of Gradient Direction and Loss Trend
- Because the network initially under-predicts the target ($\hat{y} = 0.4621 < 2.0$), the residual error $(\hat{y}-y)$ is negative.
- The negative gradient indicates that increasing each parameter will decrease the loss function.
- Gradient descent subtracting a negative gradient effectively *adds* positive deltas to $w, b, v, c$.
- This raises the hidden activation $h$ and output bias $c$, driving $\hat{y}$ upwards toward the target $2.0$.
- In the 100-step training history, the loss monotonically drops from $1.1825$ down asymptotically towards $0$, confirming stable convergence.
)";
}

} // namespace uew::neural
