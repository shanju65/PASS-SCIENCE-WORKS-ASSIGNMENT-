#pragma once

#include "../core/thermal_fem.hpp"
#include "host_info.hpp"
#include "thread_pool.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace uew {

struct SimCase {
    int id = 0;
    double k = 0;
    double q = 0;
};

struct CaseResult {
    int id = 0;
    double k = 0;
    double q = 0;
    std::vector<double> T_nodes_C;
    bool ok = false;
    std::string error;
};

struct WorkerRun {
    int workers = 1;
    std::vector<double> times_s;
    double median_s = 0;
    double speedup = 1;
    double efficiency = 1;
};

struct BenchmarkRequest {
    int case_count = 64;
    int mesh_size = 200;
    unsigned seed = 42;
    int warmup = 1;
    int repeats = 5;
    std::vector<int> worker_counts = {1, 2, 3, 4};
};

struct BenchmarkResult {
    bool ok = false;
    std::string error;
    HostInfo host;
    BenchmarkRequest request;
    std::vector<SimCase> cases;
    std::vector<CaseResult> serial;
    std::vector<CaseResult> parallel_last;
    double max_abs_temp_diff_C = 0;
    bool agreement = false;
    std::vector<WorkerRun> runs;
};

inline std::vector<SimCase> generate_cases(int count, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> k_dist(2.0, 40.0);       // W/(m K), always > 0
    std::uniform_real_distribution<double> q_dist(1000.0, 20000.0);  // W/m^3, always > 0
    std::vector<SimCase> cases;
    cases.reserve(count);
    for (int i = 0; i < count; ++i) {
        SimCase c;
        c.id = i;
        c.k = k_dist(rng);
        c.q = q_dist(rng);
        cases.push_back(c);
    }
    return cases;
}

inline CaseResult run_case(const SimCase& c, int mesh_size) {
    ThermalParams p;
    p.k = c.k;
    p.q = c.q;
    p.n_elements = mesh_size;
    ThermalResult tr = solve_thermal(p);
    CaseResult r;
    r.id = c.id;
    r.k = c.k;
    r.q = c.q;
    r.ok = tr.ok;
    r.error = tr.error;
    r.T_nodes_C = std::move(tr.T_nodes_C);
    return r;
}

inline std::vector<CaseResult> run_serial(const std::vector<SimCase>& cases, int mesh_size) {
    std::vector<CaseResult> out(cases.size());
    for (std::size_t i = 0; i < cases.size(); ++i) out[i] = run_case(cases[i], mesh_size);
    return out;
}

inline std::vector<CaseResult> run_parallel(const std::vector<SimCase>& cases, int mesh_size, int workers) {
    if (workers <= 1) return run_serial(cases, mesh_size);
    ThreadPool pool(workers);
    std::vector<std::future<CaseResult>> futs;
    futs.reserve(cases.size());
    for (const auto& c : cases) {
        int mesh = mesh_size;
        futs.push_back(pool.submit([c, mesh] { return run_case(c, mesh); }));
    }
    std::vector<CaseResult> out(cases.size());
    for (std::size_t i = 0; i < futs.size(); ++i) {
        out[i] = futs[i].get();  // deterministic output order: case index
    }
    return out;
}

inline double median_copy(std::vector<double> v) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    if (n % 2 == 1) return v[n / 2];
    return 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

inline double wall_time_s(const std::function<void()>& fn) {
    const auto t0 = std::chrono::steady_clock::now();
    fn();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(t1 - t0).count();
}

inline double max_temp_diff(const std::vector<CaseResult>& a, const std::vector<CaseResult>& b) {
    double m = 0;
    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t nt = std::min(a[i].T_nodes_C.size(), b[i].T_nodes_C.size());
        for (std::size_t k = 0; k < nt; ++k) {
            m = std::max(m, std::abs(a[i].T_nodes_C[k] - b[i].T_nodes_C[k]));
        }
    }
    return m;
}

inline BenchmarkResult run_benchmark(BenchmarkRequest req) {
    BenchmarkResult r;
    r.request = req;
    r.host = detect_host_info();
    if (req.case_count < 0) {
        r.error = "Case count cannot be negative.";
        return r;
    }
    if (req.case_count == 0) {
        r.ok = true;
        r.agreement = true;
        r.error.clear();
        return r;
    }
    if (req.mesh_size < 2 || req.mesh_size > 2000) {
        r.error = "Mesh size must be between 2 and 2000 elements.";
        return r;
    }
    if (req.repeats < 1 || req.repeats > 21) {
        r.error = "Repeat count must be between 1 and 21.";
        return r;
    }
    const int logical = std::max(1, r.host.logical_cores);
    for (int w : req.worker_counts) {
        if (w < 1) {
            r.error = "Worker count must be at least 1.";
            return r;
        }
        if (w > logical) {
            r.error = "Requested worker count exceeds logical CPU count (" + std::to_string(logical) + ").";
            return r;
        }
    }

    r.cases = generate_cases(req.case_count, req.seed);
    r.serial = run_serial(r.cases, req.mesh_size);

    for (int w : req.worker_counts) {
        for (int i = 0; i < req.warmup; ++i) {
            (void)run_parallel(r.cases, req.mesh_size, w);
        }
        WorkerRun run;
        run.workers = w;
        run.times_s.resize(req.repeats);
        for (int i = 0; i < req.repeats; ++i) {
            std::vector<CaseResult> tmp;
            run.times_s[i] = wall_time_s([&] { tmp = run_parallel(r.cases, req.mesh_size, w); });
            if (w == req.worker_counts.back() && i == req.repeats - 1) r.parallel_last = std::move(tmp);
        }
        run.median_s = median_copy(run.times_s);
        r.runs.push_back(std::move(run));
    }

    double t1 = 0;
    for (auto& run : r.runs) {
        if (run.workers == 1) t1 = run.median_s;
    }
    for (auto& run : r.runs) {
        run.speedup = (run.median_s > 0 && t1 > 0) ? (t1 / run.median_s) : 0;
        run.efficiency = run.workers > 0 ? run.speedup / static_cast<double>(run.workers) : 0;
    }

    if (r.parallel_last.empty()) r.parallel_last = r.serial;
    r.max_abs_temp_diff_C = max_temp_diff(r.serial, r.parallel_last);
    r.agreement = r.max_abs_temp_diff_C <= 1e-8;
    r.ok = true;
    return r;
}

}  // namespace uew
