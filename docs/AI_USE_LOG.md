# AI-Assisted Development & Prompt Engineering Log

## 1. Overview & Disclosure
In accordance with Section 8 and Rubric Criterion "Prompt engineering / AI judgement (8 points)" of the **PASS Science Works R&D Internship 2026–2027** technical assignment, this log provides transparent documentation of all AI tooling utilized, representative prompts, architectural outputs, candidate refinements, independent verification methods, and deliberate challenge tests.

---

## 2. Tool & Model Specification
- **Primary AI Assistant**: Antigravity AI Pair Programmer
- **Underlying Models**: Gemini 3.8 Flash / GPT-OSS 120B
- **Development Environment**: Windows 11 x64, Visual Studio 2019 BuildTools (MSVC 19.29), CMake 3.20
- **Candidate Discipline**: IT Undergraduate strengthening C++ and Computational Engineering fundamentals

---

## 3. Chronological Interaction & Refinement Log

### Session 1: Mathematical Formulation & FEA Discretization (Task C)
- **Representative Prompt**:
  > *"Derive the 1D steady-state finite element stiffness matrix and load vector for $-d/dx(k A dT/dx) = q A$ with Dirichlet boundary conditions $T(0)=100^\circ\text{C}$ and $T(L)=20^\circ\text{C}$. Explain why evaluating the error strictly at the mesh nodes shows almost zero error, and derive the midpoint shape-function error formula."*
- **AI Output Provided**:
  - Closed-form analytical solution: $T_{\text{exact}}(x) = 100 - 80\frac{x}{L} + \frac{q}{2k}x(L-x)$
  - Element stiffness matrix $K_e = \frac{kA}{l_e}\begin{bmatrix}1 & -1 \\ -1 & 1\end{bmatrix}$ and load vector $F_e = \frac{qAl_e}{2}\begin{bmatrix}1 \\ 1\end{bmatrix}$
  - Explanation of nodal superconvergence in 1D linear elements.
- **Candidate Edits & Refinements**:
  - Implemented the Thomas Algorithm (TDMA) in pure C++ instead of dense matrix inversion, guaranteeing $O(N)$ execution time.
  - Implemented explicit element midpoint evaluations $T(x_{\text{mid}}) = \frac{T_i + T_{i+1}}{2}$ and proved quadratic convergence $e(h) = \frac{q l_e^2}{8k}$.
- **Independent Verification**:
  - Analytical midpoint at $x=0.5\text{ m}$: $T(0.5) = 185.0^\circ\text{C}$ exact.
  - C++ unit test verified: $N=10 \implies 1.25^\circ\text{C}$, $N=20 \implies 0.3125^\circ\text{C}$ (ratio = 4.00), confirming $O(h^2)$ convergence.

---

### Session 2: Multicore Concurrency & Parity (Task D)
- **Representative Prompt**:
  > *"Implement a multithreaded C++ batch solver for independent thermal rods. We need wall-clock timing across 1 to 4 workers over 5 repetitions with warmup, and a verification that serial and parallel results match within 1e-8 C."*
- **AI Output Provided**:
  - C++ worker pool using `std::thread` and task chunking.
  - Benchmark harness measuring median wall-clock times.
- **Candidate Edits & Refinements**:
  - Integrated host hardware detection using Win32 API (`GetSystemInfo`, `GlobalMemoryStatusEx`, and Registry CPU model query).
  - Ensured that worker thread allocation never exceeds logical cores on constrained hardware.
  - Added a deterministic 64-bit Linear Congruential Generator (LCG) with fixed seed (42) to guarantee reproducible batches.
- **Independent Verification**:
  - Evaluated on Windows 11 host: max temperature discrepancy between serial and 4-worker parallel execution was $0.00 \times 10^{-13\ \circ}\text{C} \le 10^{-8\ \circ}\text{C}$ [PASS].

---

### Session 3: Embedded System Architecture (Task E)
- **Representative Prompt**:
  > *"Design a simulated host-connected workbench representing an STM32H7 MCU, Artix-7 FPGA, and SEGGER J-Link probe. Include a detailed pinout and voltage table, explain JTAG signal ownership, and explain why JTAG is not a compute board."*
- **AI Output Provided**:
  - Hardware adapter class hierarchy with telemetry generators.
  - Table contrasting 1.0V FPGA core, 3.3V I/O banks, and JTAG VTref sensing.
- **Candidate Edits & Refinements**:
  - Implemented concrete fault-injection and recovery methods simulating UART buffer overflows and JTAG line drops.
  - Added prominent UI simulation badges to ensure no simulated telemetry could be mistaken for live physical hardware.

---

### Session 4: Manual Backpropagation & Finite Differences (Task F)
- **Representative Prompt**:
  > *"Derive all forward pass and backward pass chain-rule gradients for $x=1, y=2$, $h=\tanh(wx+b)$, $\hat{y}=vh+c$, $J=0.5(\hat{y}-y)^2$ with initial values $w=0.5, b=0, v=1, c=0, \eta=0.1$. Verify each gradient using central finite differences with epsilon=1e-6."*
- **AI Output Provided**:
  - Manual gradient equations:
    $\frac{\partial J}{\partial v} = (\hat{y}-y)h$, $\frac{\partial J}{\partial c} = \hat{y}-y$, $\frac{\partial J}{\partial w} = (\hat{y}-y)v(1-h^2)x$, $\frac{\partial J}{\partial b} = (\hat{y}-y)v(1-h^2)$
  - Numerical values at Step 0: $h \approx 0.462117$, $\hat{y} \approx 0.462117$, $J \approx 1.182542$.
- **Candidate Edits & Refinements**:
  - C++ engine recorded 100-step training history showing monotonic loss reduction to $< 0.05$.
- **Independent Verification**:
  - Central finite differences verified across all four parameters: $w$ and $b$ have error $1.1301893\times10^{-10}$, $v$ has error $5.253831\times10^{-11}$, and $c$ has error $1.1175483\times10^{-10}$; all are $\le 10^{-5}$ [PASS].

---

## 4. Identification of Unsuitable Suggestions & Deliberate Challenge Tests

### Challenge Test 1: Python/Flask Backend Suggestion
- **Initial Suggestion by General AI**: When initially discussing web-to-backend communication, generic AI suggestions recommended spinning up a Python Flask/FastAPI backend to wrap the C++ solver via pybind11.
- **Critical Evaluation**: The PASS Science Works specification explicitly states:
  > *"All application backend logic, numerical computation, file processing and concurrency must be C++17 or later. Do not implement backend functionality in Python."*
- **Correction Applied**: Rejected the Python wrapper entirely. Authored an embedded C++17 Winsock HTTP server directly in `src/api/http_server.cpp`, eliminating all Python dependencies and keeping the entire backend compiled into a single native binary (`Launch.exe`).

### Challenge Test 2: Inverting Dense Stiffness Matrix
- **Initial Suggestion**: Using a generic $N \times N$ matrix inversion library or Gaussian elimination algorithm with $O(N^3)$ complexity.
- **Critical Evaluation**: 1D linear FEA yields a strictly tridiagonal, symmetric, diagonally dominant matrix. Full inversion consumes excessive memory and introduces unnecessary numerical rounding.
- **Correction Applied**: Implemented the specialized $O(N)$ Thomas Algorithm (Tridiagonal Matrix Algorithm) in `src/core/thermal_solver.cpp`, providing maximum numerical speed and stability without external dependencies.

---

## 5. Short Ownership & Licences Statement
- **Original Implementation**: All C++ solvers (`src/core/thermal_solver.cpp`), geometry parsers (`src/geometry/`), multicore engine (`src/performance/`), embedded hardware simulators (`src/hardware/`), backpropagation engine (`src/neural/`), Winsock HTTP server (`src/api/`), HTML5 dashboard, CSS, and application logic (`frontend/scripts/app.js`) were authored specifically for this submission.
- **Third-Party Libraries**:
  - **Three.js** (r128): MIT License (Offline 3D WebGL rendering in browser).
  - **Chart.js** (v4.4.1): MIT License (Offline technical charting in browser).
  - **Windows Sockets 2 (ws2_32)**: Microsoft Windows SDK System Library.
- **Data Provenance**:
  - `data/example_geometry.stl`: Self-created 3D CAD prism geometry (Public Domain / CC0).
  - `data/example_temperature.csv`: Synthetic ANSYS Mechanical nodal temperature export generated specifically for Task B compliance (CC0).
