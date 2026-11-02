# Unified Engineering Workbench (UEW)
### PASS SCIENCE WORKS | R&D INTERNSHIP 2026–2027 TECHNICAL EVALUATION

[![Platform](https://img.shields.io/badge/Platform-Windows%2011%20x64-0078D6.svg)](https://microsoft.com)
[![Standard](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org)
[![Build](https://img.shields.io/badge/Build-CMake%203.20%2B%20%7C%20MSVC-brightgreen.svg)](https://cmake.org)
[![Tests](https://img.shields.io/badge/Tests-60%2F60%20Passing-success.svg)](tests/test_runner.cpp)
[![One--Click](https://img.shields.io/badge/Standalone-Launch.exe-orange.svg)](Launch.exe)

---
DEMO VIDEO: https://drive.google.com/file/d/1Znhid6DeKUz_H0cAMGKOgLWnrJI2Kbp1/view?usp=drive_link

## 1. Quick Start: One-Click Execution (Windows 11)

> **No installation, no terminal commands, no PowerShell, no administrator rights, and no internet connection required.**

1. Extract the submission ZIP: `CandidateID_Workbench.zip`.
2. Open the extracted `CandidateID_Workbench` folder.
3. Double-click **`Launch.exe`** in that folder.
4. The native C++17 Winsock backend will initialize on `http://localhost:8080` and automatically open your default browser.
5. Interact with the full engineering workbench!

*(To stop the application, press `Ctrl+C` in the backend console window or simply close the console).*

## Prerequisites
- 64-bit Windows 10/11.
- A desktop browser for viewing the local dashboard (Microsoft Edge, Chrome, or Firefox).
- For one-click use, no additional runtime or package installation is required.
- For source builds: Visual Studio 2019 Build Tools with MSVC C++ workload, Windows SDK 10.0.19041 or compatible SDK, CMake 3.20+, and Git for version-control commands.
- `127.0.0.1:8080` must be available; no internet connection is required.

---

## 2. Supported Formats & File Types

| Format | Category | Schema / Specifications | C++ Parser |
| :--- | :--- | :--- | :--- |
| **ASCII STL** | 3D Geometry | Standard triangular facets: `solid ... facet normal ... vertex ... endfacet ... endsolid`. Tested up to 50 MB. | `src/geometry/stl_parser.cpp` |
| **ANSYS CSV** | Thermal Mesh Results | Required header: `node_id,x_m,y_m,z_m,temperature_C`. Rejects non-finite values (NaN, Inf). Tested up to 20 MB. | `src/geometry/csv_parser.cpp` |
| **Results CSV** | Numerical Export | Raw exports for thermal FEA profiles, multicore speedup telemetry, and backpropagation loss history. | `src/core/thermal_solver.cpp` |

Sample files provided with verified data provenance:
- `data/example_geometry.stl`: Valid 3D CAD prism bracket.
- `data/example_temperature.csv`: Synthetic ANSYS Mechanical nodal temperature export.

---

## 3. High-Level Architecture & Separation of Concerns

The workbench enforces a strict decoupled architecture:

```
[ Frontend: HTML5 / CSS3 / Vanilla JS / Three.js / Chart.js ]
                             ⇅ (Local HTTP REST on 127.0.0.1:8080)
[ C++17 Winsock HTTP Server (src/api/http_server.cpp) ]
                             ⇅ (Native C++ Interfaces)
[ C++17 Domain Core: FEA Solver | Parsers | Concurrency | HW Mocks | Backprop ]
```

- **All computation, numerical methods, file parsers, and concurrency are implemented strictly in C++17**.
- **JavaScript handles only presentation, 3D WebGL rendering, and DOM updates**.
- All dependencies are local: `three.min.js` and `chart.min.js` are bundled offline in `frontend/scripts/`.

---

## 4. Verification Suite & Test Results

The automated test runner (`tests/test_runner.cpp`) validates all 5 core modules:

```text
========================================================
  UNIFIED ENGINEERING WORKBENCH - VERIFICATION SUITE   
  PASS SCIENCE WORKS | R&D INTERNSHIP 2026-2027        
========================================================

=== [TEST SUITE 1] Thermal FEA Solver Tests ===
  [PASS] Thermal solve succeeds for standard 10-element problem
  [PASS] Left Dirichlet boundary condition T(0) == 100 C exact
  [PASS] Right Dirichlet boundary condition T(L) == 20 C exact
  [PASS] Analytical solution at x=0.5 m evaluates exactly to 185.0 C
  [PASS] Convergence study contains all 4 required mesh sizes
  [PASS] Error decreases from N=10 to N=20
  [PASS] Error decreases from N=20 to N=40
  [PASS] Error decreases from N=40 to N=80
  [PASS] Mesh convergence exhibits theoretical quadratic O(h^2) rate (~4x error reduction)
  [PASS] Rejects N=0 element count
  [PASS] Rejects negative thermal conductivity
  [PASS] Rejects zero length
  [PASS] Rejects NaN parameter input

=== [TEST SUITE 2] CAD STL & ANSYS CSV Parser Tests ===
  [PASS] Valid ASCII STL parses successfully
  [PASS] Example STL contains 12 triangular facets (standard cube/prism)
  [PASS] Flat position buffer contains 108 floats
  [PASS] Total surface area strictly positive
  [PASS] Rejects empty 0-byte STL with clear diagnostic
  [PASS] Rejects malformed STL with missing vertex coordinates
  [PASS] Valid synthetic ANSYS CSV parses successfully
  [PASS] Parsed records count is greater than zero
  [PASS] Min temperature < Max temperature
  [PASS] Point coordinate buffer matches 3x node count
  [PASS] Rejects CSV with missing column (z_m omitted)
  [PASS] Rejects CSV containing NaN values

=== [TEST SUITE 3] Multicore Batch Simulation Tests ===
  [PASS] Detected >= 1 logical CPU core
  [PASS] CPU model string detected
  [PASS] Batch generator produces configured case count
  [PASS] Batch generator is strictly deterministic with fixed seed
  [PASS] Serial and parallel outputs match in size
  [PASS] Maximum temperature difference between serial and parallel <= 1e-8 C requirement
  [PASS] Benchmark suite verifies serial/parallel agreement pass
  [PASS] Benchmark records worker results

=== [TEST SUITE 4] Embedded Hardware Simulation Tests ===
  [PASS] Discovered exactly 3 simulated hardware devices (MCU, FPGA, JTAG)
  [PASS] MCU adapter successfully instantiated
  [PASS] FPGA adapter successfully instantiated
  [PASS] JTAG adapter successfully instantiated
  [PASS] MCU connects cleanly
  [PASS] MCU telemetry streaming nominal
  [PASS] MCU core voltage within 1.25V window
  [PASS] Fault injection triggers simulated timeout / communication error
  [PASS] Status transitions to Error_Timeout
  [PASS] Device executes successful error recovery
  [PASS] Device returns to Connected state
  [PASS] MCU disconnects cleanly

=== [TEST SUITE 5] Backpropagation Tests ===
  [PASS] z == 0.5 exact
  [PASS] h == tanh(0.5) ~ 0.462117 exact
  [PASS] Initial Loss J ~ 1.18254 exact
  [PASS] dJ/dc is negative (pushing prediction towards 2.0)
  [PASS] dJ/dv is negative
  [PASS] dJ/dw is negative
  [PASS] dJ/db is negative
  [PASS] Finite difference executed for all 4 parameters (w, b, v, c)
  [PASS] Finite diff check for w (error: 1.1301893e-10 <= 1e-5)
  [PASS] Finite diff check for b (error: 1.1301893e-10 <= 1e-5)
  [PASS] Finite diff check for v (error: 5.253831e-11 <= 1e-5)
  [PASS] Finite diff check for c (error: 1.1175483e-10 <= 1e-5)
  [PASS] Recorded 100 training steps (steps 0 to 100)
  [PASS] Loss strictly decreased over training
  [PASS] Loss converged towards zero (J_100 < 0.05)

========================================================
  TEST SUMMARY: 60 PASSED, 0 FAILED
========================================================
```

---

## 5. Building from Source (CMake & MSVC)

To rebuild the solution from source:
```cmd
# Run automated build and test script:
build.bat

# Or invoke CMake manually:
cmake -B build -S . -G "Visual Studio 16 2019" -A x64
cmake --build build --config Release
build\Release\workbench_tests.exe
```

---

## 6. 3-to-5 Minute Demonstration Walkthrough Script

| Timestamp | Phase | Action & Expected Outcome |
| :--- | :--- | :--- |
| **00:00 – 00:30** | **Launch & Dashboard** | Double-click `Launch.exe`. Browser opens to `http://localhost:8080`. Green status pill displays `Backend: Connected (MSVC x64)`. Telemetry table populates with host CPU, cores, and RAM. |
| **00:30 – 01:15** | **CAD & ANSYS 3D** | Switch to **CAD & ANSYS 3D** tab. Click *Load Example STL* to view 3D bracket mesh in Three.js (rotate, zoom, pan). Click *Load Synthetic CSV* to display 275-node coloured thermal point cloud. Hover/click a node to inspect exact $(x,y,z)$ coordinates and temperature in $^\circ\text{C}$. |
| **01:15 – 02:00** | **Thermal FEA Solver** | Switch to **Thermal FEA Solver** tab. With $L=1\text{ m}, k=10\text{ W/(m K)}, q=10000\text{ W/m}^3$, click *Solve Thermal FEA*. View numerical curve vs exact curve $T(x) = 100 - 80x + 500x(1-x)$. Click *Run Mesh Convergence Study* to observe quadratic $O(h^2)$ error drop across $N \in \{10, 20, 40, 80\}$. Click *Export Results CSV*. |
| **02:00 – 02:45** | **Multicore Benchmark** | Switch to **Multicore Benchmark** tab. Click *Execute Multicore Benchmark* for 1000 independent thermal rods. Watch 5 repetitions execute across 1, 2, 3, 4 workers. Observe speedup chart and verified numerical parity banner ($\max |\Delta| \le 10^{-8\ \circ}\text{C}$). |
| **02:45 – 03:30** | **Embedded Systems** | Switch to **Embedded Systems** tab. Note *SIMULATED HARDWARE* banner. Connect the STM32H7 MCU and observe live voltage/current telemetry. Click *Inject Fault* to simulate UART timeout (`Error_Timeout`). Click *Recover* to return to nominal streaming. Inspect the pinout and voltage table. |
| **03:30 – 04:15** | **Backpropagation** | Switch to **Backpropagation** tab. Review manual chain-rule derivations. Click *Train 100 Steps*. Verify central finite-difference error table ($\le 10^{-5}$ across $w, b, v, c$). View 100-step monotonic loss decay curve. Download CSV. |
| **04:15 – 04:30** | **Summary & Clean Exit** | Switch to **Reports & Exports** tab to inspect all generated CSV files. Close browser and press `Ctrl+C` in console to cleanly shut down. |

---

---

## 8. Known Limitations
1. **1D Axial Heat Conduction**: The thermal finite element formulation models axial 1D heat flow; 2D/3D thermal stress and non-linear radiation are outside scope.
2. **CAD Mesh-to-Result Registration**: Geometry and ANSYS point clouds are co-rendered in 3D space; surface interpolation onto arbitrary solids is omitted as permitted by the specification.
3. **Simulated Embedded Targets**: MCU, FPGA, and JTAG hardware are simulated in software. No physical electrical measurements are made.
