#ifndef UEW_PERFORMANCE_MULTICORE_ENGINE_HPP
#define UEW_PERFORMANCE_MULTICORE_ENGINE_HPP

#include <vector>
#include <string>
#include <chrono>
#include <cstdint>
#include "../core/thermal_solver.hpp"

namespace uew::performance {

struct ThermalCaseInput {
    int case_id = 0;
    core::ThermalParams params;
};

struct ThermalCaseOutput {
    int case_id = 0;
    double max_temp_C = 0.0;
    double avg_temp_C = 0.0;
    double max_midpoint_error_C = 0.0;
    double tip_temp_C = 0.0;
};

struct HardwareSystemInfo {
    std::string cpu_model;
    uint32_t physical_cores = 1;
    uint32_t logical_cores = 1;
    uint64_t total_ram_mb = 0;
    std::string compiler_info;
    std::string optimization_flags;
};

struct WorkerBenchmarkResult {
    int worker_count = 1;
    std::vector<double> repetition_times_ms; // 5 repetitions
    double median_time_ms = 0.0;
    double speedup = 1.0;
    double efficiency = 1.0;
};

struct BenchmarkReport {
    HardwareSystemInfo system_info;
    int total_cases = 1000;
    int mesh_size = 40;
    uint32_t random_seed = 42;
    
    double warmup_time_ms = 0.0;
    double serial_median_time_ms = 0.0;
    
    std::vector<WorkerBenchmarkResult> worker_results;
    
    // Serial vs Parallel Verification
    double max_temperature_discrepancy_C = 0.0;
    bool numerical_agreement_passed = false; // Must be <= 1e-8 C
    
    std::string analysis_notes;
};

/**
 * @brief Retrieves host CPU, cores, RAM, and compiler configuration.
 */
HardwareSystemInfo get_system_hardware_info();

/**
 * @brief Generates reproducible synthetic thermal simulation cases using a fixed PRNG seed.
 */
std::vector<ThermalCaseInput> generate_thermal_batch(int case_count, int mesh_size, uint32_t seed = 42);

/**
 * @brief Runs batch simulation serially.
 */
std::vector<ThermalCaseOutput> solve_batch_serial(const std::vector<ThermalCaseInput>& cases);

/**
 * @brief Runs batch simulation using a bounded multithreaded worker pool.
 * Preserves strictly deterministic order matching input case index.
 */
std::vector<ThermalCaseOutput> solve_batch_parallel(const std::vector<ThermalCaseInput>& cases, int worker_count);

/**
 * @brief Executes the complete benchmark suite across 1, 2, 3, 4 workers with warmup and 5 repetitions.
 */
BenchmarkReport run_multicore_benchmark(int case_count = 1000, int mesh_size = 40, int max_workers = 4);

/**
 * @brief Formats benchmark outcomes into standard CSV for export.
 */
std::string export_benchmark_csv(const BenchmarkReport& report);

} // namespace uew::performance

#endif // UEW_PERFORMANCE_MULTICORE_ENGINE_HPP
