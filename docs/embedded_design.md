# Embedded-System Integration Design (Task E)

## 1. System Overview
The embedded subsystem simulates a modular hardware test bench comprising three heterogeneous targets connected to a Windows 11 host workstation:
1. **Compute Board 1**: STMicroelectronics STM32H743ZI (ARM Cortex-M7 @ 480 MHz).
2. **Compute Board 2**: Digilent Basys 3 (Xilinx Artix-7 XC7A35T FPGA).
3. **Hardware Debug Adapter**: SEGGER J-Link / FTDI FT2232H Dual-Channel JTAG Probe.

---

## 2. Interface, Pinout & Voltage Domain Matrix

| Device Under Test | Power Requirements | Grounding Strategy | Voltage Domains | Logic Levels | Level Shifting Required? | Primary Physical Connectors | Host Communication | JTAG Signal Ownership |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Microcontroller**<br>(STM32H743ZI) | 5.0V USB VBUS rail (~180 mA typical) | Single-point star ground to chassis | VDD = 3.3V, VCORE = 1.25V internal LDO | 3.3V LVCMOS (5V tolerant pins) | Yes, when talking to 5V legacy buses or 1.8V sensors | Micro-USB Type-B, ST Morpho, Arduino Uno headers | USB CDC-ACM Virtual COM port @ 115200 baud | **Slave / Target Role** (Listens on SWCLK/SWDIO/JTMS; drives TDO only during Shift-DR) |
| **FPGA Board**<br>(Artix-7 XC7A35T) | 5.0V external or USB (~650 mA peak boot) | Multi-point low-impedance ground plane | VCCINT = 1.0V, VCCAUX = 1.8V, VCCO = 3.3V | 3.3V LVCMOS across user I/O banks | Mandatory for LVDS pairs or 1.8V Pmod sensors | Micro-USB, 4x 12-pin Pmods, 100-pin FX2 header | FTDI FT2232HQ USB-to-UART / FIFO stream | **Slave / Target Role** (Boundary-scan TAP chain, receives TCK/TMS/TDI, outputs TDO) |
| **JTAG Debugger**<br>(SEGGER J-Link) | 5.0V Host USB rail (< 100 mA drawn) | Common ground tie to target VTref rail | Dynamic target tracking (1.2V to 5.0V) | Matches target reference automatically | **Built-in active bidirectional level shifters** track VTref | Standard 20-pin 0.1" pitch ARM JTAG header | USB 2.0 High-Speed Bulk Endpoints | **Master / Controller Role** (Owns and actively drives TCK, TMS, TDI, nRESET lines) |

---

## 3. Why JTAG is a Debug Interface (Not a Generic Compute Board)

### 1. Asymmetric Master/Target Protocol
JTAG (IEEE 1149.1 / IEEE 1149.7) is fundamentally a synchronous serial boundary-scan test interface. The debug probe operates as the sole bus master, driving:
- `TCK` (Test Clock)
- `TMS` (Test Mode Select)
- `TDI` (Test Data In)

The targets (the MCU and FPGA) are slave Test Access Ports (TAPs) that respond to state transitions on `TMS` and shift internal instruction/data registers out over `TDO` (Test Data Out). JTAG does not define peer-to-peer compute, packet routing, or multi-master arbitration. Connecting two debug adapters to each other would create catastrophic output buffer contention.

### 2. Signal Ownership & Drive Strength
In JTAG:
- The **debugger owns** `TCK`, `TMS`, and `TDI` as push-pull outputs.
- The **target owns** `TDO` as a tri-state output (enabled exclusively in the `Shift-IR` and `Shift-DR` TAP states).
- Attempting to use a JTAG port as a data transmission channel requires bit-banging JTAG register scans, introducing severe latency overhead compared to native UART, SPI, or USB endpoints.

### 3. Voltage Heterogeneity & Dynamic Level Tracking
Target computing boards feature distinct internal I/O voltages (e.g. 1.8V for high-speed FPGA banks, 3.3V for standard microcontrollers). The JTAG debugger relies on a high-impedance sense pin (`VTref`, Pin 1) to sense the target supply rail and dynamically bias its internal level translators (e.g. TI TXS0108E / SN74AVC4T245). Driving fixed 3.3V or 5.0V logic into an unpowered or 1.8V device causes destructive substrate latch-up or gate-oxide breakdown.
