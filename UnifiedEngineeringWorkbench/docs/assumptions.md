# Engineering Assumptions, Units & File-Size Limits

## 1. Domain Units Convention
All modules adhere strictly to standard SI units unless explicitly noted:
- **Spatial Dimensions**: Meters ($m$)
- **Cross-Sectional Area**: Square meters ($m^2$)
- **Temperature**: Degrees Celsius ($^\circ\text{C}$)
- **Thermal Conductivity ($k$)**: Watts per meter-Kelvin ($W/(m\cdot K)$)
- **Volumetric Heat Generation ($q$)**: Watts per cubic meter ($W/m^3$)
- **Time**: Milliseconds ($ms$) for wall-clock benchmarks; Seconds ($s$) for timeouts
- **Electrical**: Volts ($V$), Milliamperes ($mA$)
- **Frequencies**: Megahertz ($MHz$)

---

## 2. Modeling Assumptions

### Task C: Mechanical Thermal FEA
- **1D Conduction**: Heat flows strictly along the axial coordinate $x \in [0, L]$. Lateral surfaces are perfectly insulated ($\frac{\partial T}{\partial n} = 0$).
- **Homogeneous Material**: Thermal conductivity $k = 10\text{ W/(m K)}$ and cross-sectional area $A = 10^{-4}\text{ m}^2$ are uniform and temperature-independent.
- **Uniform Internal Heat Generation**: $q = 10,000\text{ W/m}^3$ is constant along the entire length.
- **Steady-State**: Time derivatives vanish ($\frac{\partial T}{\partial t} = 0$).
- **Linear Elements**: Two-node linear shape functions ($C^0$ continuous).

### Task B: CAD & ANSYS Visualization
- **No Mesh-to-Result Registration**: The assignment explicitly states that registering or interpolating ANSYS temperature nodes onto arbitrary CAD solid surfaces is outside scope. The geometry is rendered as an STL surface mesh alongside a 3D coloured point cloud.
- **Tested File-Size Limits**:
  - ASCII STL: Tested up to $50\text{ MB}$ (~250,000 facets).
  - ANSYS CSV: Tested up to $20\text{ MB}$ (~100,000 nodes).
  - Parsing rejects non-finite values (NaN, Inf), empty files, and malformed syntax.

### Task D: Multicore Concurrency
- **Independence**: Each thermal simulation case in a batch is mathematically independent with its own parameter struct.
- **Determinism**: Case generation uses a fixed 64-bit seed (42). Worker results are indexed deterministically into an output vector matching the input sequence.
- **Hardware Ceiling**: Active worker thread counts are capped at the host's physical/logical core count (`std::thread::hardware_concurrency()`).

### Task E: Embedded Systems
- **Simulation**: No physical microcontrollers, FPGAs, or debug probes are required. All register states, pin transitions, and packet feeds are generated in C++.
- **Error Injection**: Simulated timeout triggers a recoverable communication fault state.

### Task F: Backpropagation
- **Single Regression Sample**: Fixed sample $(x=1, y=2)$.
- **No Autodiff**: All gradients are derived manually via chain rule.
- **Finite Difference Tolerance**: Step size $\epsilon = 10^{-6}$ with strict error assertion $|\Delta| \le 10^{-5}$.
