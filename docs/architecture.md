# System Architecture — Unified Engineering Workbench

## 1. Overview
The **Unified Engineering Workbench (UEW)** is an integrated desktop engineering application built for the **PASS Science Works R&D Internship 2026–2027** technical evaluation. The application strictly adheres to the non-negotiable requirement that **all backend computation, numerical methods, file parsers, concurrency, and embedded simulation must be authored in C++17**, with presentation handled via an offline HTML5/CSS3/Three.js frontend.

---

## 2. Layered Architecture

```
+---------------------------------------------------------------------------------+
|                       FRONTEND PRESENTATION LAYER (Web View)                    |
|  - HTML5 Responsive Dashboards (Tabs: Overview, CAD, FEA, Multicore, HW, NN)   |
|  - Three.js WebGL 3D Geometry & ANSYS Colored Point Cloud Rendering             |
|  - Chart.js Convergence, Thermal Profile, and Loss Trajectory Visualizers       |
|  - Real-time Hardware Telemetry Streaming Poller & Error Injection Modals      |
+---------------------------------------------------------------------------------+
                                      |  ^
         Local HTTP 1.1 JSON API      |  |  Loopback (127.0.0.1:8080)
         CORS-enabled / Non-blocking  |  |  Asynchronous Client Handlers
                                      v  |
+---------------------------------------------------------------------------------+
|                       C++17 LOCAL REST SERVER (src/api/)                        |
|  - Multi-threaded Winsock 2 Server (ws2_32)                                     |
|  - Static Asset Delivery (offline CSS, JS, Three.js, Chart.js bundles)          |
|  - JSON Serialization / Deserialization Engine                                  |
|  - RESTful Endpoints: /api/health, /api/thermal, /api/geometry, etc.            |
+---------------------------------------------------------------------------------+
                                      |
         C++ Native Class Interfaces  |  Strict Separation of Concerns
                                      v
+---------------------------------------------------------------------------------+
|                       C++17 CORE DOMAIN MODULES (src/)                          |
|                                                                                 |
|  [src/core/]       - 1D Steady-State Thermal FEA Solver (Thomas O(N))           |
|  [src/geometry/]   - ASCII STL Parser & ANSYS Temperature CSV Engine            |
|  [src/performance/]- Multithreaded Worker Pool Batch Engine (Deterministic)     |
|  [src/hardware/]   - Embedded System Mock Adapters (MCU, FPGA, JTAG Probe)      |
|  [src/neural/]     - Manual Backpropagation & Finite-Difference Verifier        |
+---------------------------------------------------------------------------------+
                                      |
                                      v
+---------------------------------------------------------------------------------+
|                       VERIFICATION & PACKAGING SUITE                            |
|  - tests/test_runner.cpp : 60 Automated Unit & Numerical Parity Tests           |
|  - Launch.exe            : Native x64 One-Click Launcher for Windows 11         |
+---------------------------------------------------------------------------------+
```

---

## 3. Module Boundaries & Implementation Details

### A. Numerical Computation (`src/core/`)
- Pure C++17 mathematical solver with zero third-party linear algebra dependencies.
- Tridiagonal matrix solver (Thomas Algorithm) runs in strictly linear $O(N)$ arithmetic complexity.
- Implements closed-form exact reference:
  $$T_{\text{exact}}(x) = 100 - 80\frac{x}{L} + \frac{q}{2k}x(L - x)$$
- Evaluates errors not merely at mesh nodes (which superconverge), but at element midpoints using shape functions $T(x_{\text{mid}}) = \frac{T_i + T_{i+1}}{2}$.

### B. Geometry & ANSYS CSV Engine (`src/geometry/`)
- ASCII STL Parser validates `solid`, facet normal vectors, triangle outer loops, coordinate triplets, and `endsolid`. Rejects truncated facets, NaN, and infinity.
- ANSYS CSV Parser validates `node_id,x_m,y_m,z_m,temperature_C`, extracts spatial bounding boxes, statistical metrics (min, mean, max temperature), and produces flat float buffers for WebGL rendering.

### C. Multicore Concurrency Engine (`src/performance/`)
- Employs standard C++ `std::thread` and chunked task partitioning.
- Batch simulation tasks are assigned fixed random seeds (42) via an internal 64-bit Linear Congruential Generator.
- Guarantees deterministic order matching the input batch sequence.
- Enforces strict serial-parallel numerical agreement $\le 10^{-8\ \circ}\text{C}$.

### D. Embedded Hardware Manager (`src/hardware/`)
- Abstract base class `IDeviceAdapter` defines unified lifecycle hooks: `connect()`, `disconnect()`, `poll_telemetry()`, `inject_fault()`, and `recover_from_fault()`.
- Implements concrete mock models for an STM32H7 MCU, Artix-7 FPGA, and SEGGER J-Link probe.

### E. Manual Backpropagation Engine (`src/neural/`)
- Single-sample regression model with $\tanh$ activation and squared-error loss.
- Evaluates analytical gradients via manual chain-rule equations without autodiff.
- Validates each gradient against central finite differences with $\epsilon = 10^{-6}$, enforcing absolute discrepancy $\le 10^{-5}$.

### F. Transport & Network Layer (`src/api/`)
- Pure native Winsock 2 (`ws2_32.lib`) implementation.
- Every client connection is dispatched to a detached worker thread, ensuring the dashboard never freezes during long solves or large STL uploads.
- Serves offline static assets directly from disk.
