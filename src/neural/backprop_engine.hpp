#ifndef UEW_NEURAL_BACKPROP_ENGINE_HPP
#define UEW_NEURAL_BACKPROP_ENGINE_HPP

#include <vector>
#include <string>

namespace uew::neural {

struct NetworkParameters {
    double w = 0.5; // Input weight
    double b = 0.0; // Hidden bias
    double v = 1.0; // Output weight
    double c = 0.0; // Output bias
};

struct ForwardState {
    double x = 1.0;
    double y = 2.0;
    double z = 0.0;     // z = w*x + b
    double h = 0.0;     // h = tanh(z)
    double y_hat = 0.0; // y_hat = v*h + c
    double loss_J = 0.0;// J = 0.5*(y_hat - y)^2
};

struct Gradients {
    double dJ_dw = 0.0;
    double dJ_db = 0.0;
    double dJ_dv = 0.0;
    double dJ_dc = 0.0;
};

struct FiniteDifferenceCheck {
    std::string parameter_name;
    double analytical_gradient = 0.0;
    double numerical_gradient = 0.0;
    double absolute_error = 0.0;
    bool passed = false; // Must be <= 1e-5 with epsilon = 1e-6
};

struct StepRecord {
    int step = 0;
    double loss_J = 0.0;
    double y_hat = 0.0;
    double w = 0.0;
    double b = 0.0;
    double v = 0.0;
    double c = 0.0;
    double dJ_dw = 0.0;
    double dJ_db = 0.0;
    double dJ_dv = 0.0;
    double dJ_dc = 0.0;
};

struct BackpropReport {
    NetworkParameters initial_params;
    ForwardState initial_forward;
    Gradients initial_analytical_gradients;
    std::vector<FiniteDifferenceCheck> finite_diff_checks;
    bool all_finite_diff_checks_passed = false;
    
    // One simultaneous step updated parameters
    NetworkParameters step1_params;
    ForwardState step1_forward;

    // 100 training steps history
    std::vector<StepRecord> training_history;
    double final_loss = 0.0;
    
    std::string mathematical_explanation;
};

/**
 * @brief Computes forward pass given inputs x, y and network weights.
 */
ForwardState forward_pass(double x, double y, const NetworkParameters& p);

/**
 * @brief Computes exact analytical gradients using manual chain rule derivations.
 */
Gradients compute_analytical_gradients(const ForwardState& state, const NetworkParameters& p);

/**
 * @brief Verifies analytical gradients against central finite differences with epsilon = 1e-6.
 */
std::vector<FiniteDifferenceCheck> verify_gradients_finite_difference(
    double x, double y, const NetworkParameters& p, const Gradients& analytical, double epsilon = 1.0e-6);

/**
 * @brief Runs 100-step gradient descent training using learning rate eta = 0.1.
 */
BackpropReport run_full_backpropagation_study(double learning_rate = 0.1, int total_steps = 100);

/**
 * @brief Exports 100-step training loss and weight trajectories to CSV.
 */
std::string export_training_history_csv(const BackpropReport& report);

} // namespace uew::neural

#endif // UEW_NEURAL_BACKPROP_ENGINE_HPP
