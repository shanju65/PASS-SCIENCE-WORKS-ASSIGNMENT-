#include "hardware_manager.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>

namespace uew::hardware {

static std::string get_current_time_iso() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#if defined(_WIN32)
    localtime_s(&tm_buf, &now_c);
#else
    localtime_r(&now_c, &tm_buf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return std::string(buf);
}

// -------------------------------------------------------------
// Microcontroller Adapter (STM32H743ZI ARM Cortex-M7)
// -------------------------------------------------------------
class McuAdapter : public IDeviceAdapter {
private:
    DeviceDescriptor desc_;
    ConnectionStatus status_ = ConnectionStatus::Disconnected;
    uint32_t packet_seq_ = 0;
    bool fault_injected_ = false;

public:
    McuAdapter() {
        desc_.device_id = "DEV_MCU_STM32H7";
        desc_.display_name = "STMicroelectronics STM32H743ZI (Nucleo-144)";
        desc_.type = DeviceType::Microcontroller;
        desc_.vendor = "STMicroelectronics";
        desc_.serial_number = "ST-0924718-H7";
        desc_.firmware_version = "v2.4.1-rc3";
        desc_.host_interface = "USB 2.0 Full-Speed Virtual COM Port (CDC-ACM)";
        desc_.power_requirements = "5.0V USB VBUS (or external 7-12V VIN); 180 mA typical";
        desc_.logic_levels = "3.3V CMOS / LVTTL (5V tolerant on designated I/O pins)";
        desc_.connector_type = "Micro-USB Type-B + Arduino UNO V3 + ST Morpho Headers";
        desc_.is_simulated = true;

        desc_.pinout_table = {
            {"VBUS", "Host 5V Power Supply", "5.0V", "Power", "Direct from USB host rail"},
            {"GND", "Common Signal Ground", "0.0V", "Ground", "Shared ground reference"},
            {"3V3", "Regulated Logic Rail", "3.3V", "Power Output", "From onboard LDO regulator"},
            {"PA9", "USART1 TX (Telemetry)", "3.3V", "Output", "115200 baud serial uplink"},
            {"PA10", "USART1 RX (Commands)", "3.3V", "Input", "115200 baud serial downlink"},
            {"PA13", "SWDIO / JTMS", "3.3V", "Bi-directional", "SWD data line / JTAG mode select"},
            {"PA14", "SWCLK / JTCK", "3.3V", "Input", "JTAG/SWD debug clock input"}
        };
    }

    DeviceDescriptor get_descriptor() const override { return desc_; }
    ConnectionStatus get_status() const override { return status_; }

    bool connect() override {
        if (fault_injected_) {
            status_ = ConnectionStatus::Error_Timeout;
            return false;
        }
        status_ = ConnectionStatus::Connected;
        return true;
    }

    bool disconnect() override {
        status_ = ConnectionStatus::Disconnected;
        return true;
    }

    DeviceTelemetry poll_telemetry() override {
        DeviceTelemetry tel;
        tel.timestamp_iso = get_current_time_iso();
        tel.packet_counter = ++packet_seq_;

        if (status_ != ConnectionStatus::Connected && status_ != ConnectionStatus::Streaming) {
            tel.error_flag = true;
            tel.status_payload = "Device offline or disconnected";
            return tel;
        }

        if (fault_injected_) {
            status_ = ConnectionStatus::Error_Timeout;
            tel.error_flag = true;
            tel.status_payload = "COMMUNICATION TIMEOUT: UART buffer overflow / ACK timeout";
            return tel;
        }

        status_ = ConnectionStatus::Streaming;
        tel.core_voltage_V = 1.25 + 0.01 * std::sin(packet_seq_ * 0.2);
        tel.io_voltage_V = 3.31 + 0.005 * std::cos(packet_seq_ * 0.1);
        tel.current_mA = 184.5 + 4.2 * std::sin(packet_seq_ * 0.5);
        tel.junction_temp_C = 42.8 + 1.2 * std::cos(packet_seq_ * 0.05);
        tel.status_payload = "NORMAL: Heartbeat OK, Free RTOS tick: " + std::to_string(packet_seq_ * 100);
        tel.error_flag = false;
        return tel;
    }

    void inject_fault(bool enable) override {
        fault_injected_ = enable;
        if (enable) {
            status_ = ConnectionStatus::Error_Timeout;
        }
    }

    bool recover_from_fault() override {
        fault_injected_ = false;
        status_ = ConnectionStatus::Connected;
        return true;
    }
};

// -------------------------------------------------------------
// FPGA Adapter (Xilinx Artix-7 XC7A35T)
// -------------------------------------------------------------
class FpgaAdapter : public IDeviceAdapter {
private:
    DeviceDescriptor desc_;
    ConnectionStatus status_ = ConnectionStatus::Disconnected;
    uint32_t packet_seq_ = 0;
    bool fault_injected_ = false;

public:
    FpgaAdapter() {
        desc_.device_id = "DEV_FPGA_ARTIX7";
        desc_.display_name = "Digilent Basys 3 (Xilinx Artix-7 XC7A35T-1CPG236C)";
        desc_.type = DeviceType::FPGA;
        desc_.vendor = "Xilinx / Digilent";
        desc_.serial_number = "BASYS3-410-183-A7";
        desc_.firmware_version = "Bitstream: rev_3.18_dual_dsp";
        desc_.host_interface = "USB-JTAG / USB-UART Bridge (FTDI FT2232HQ)";
        desc_.power_requirements = "5.0V USB External or Wall Adapter; 650 mA peak during bitstream boot";
        desc_.logic_levels = "VCCINT = 1.0V, VCCAUX = 1.8V, VCCO (Banks 14/34/35) = 3.3V LVCMOS";
        desc_.connector_type = "USB Standard-B Micro + 4x 12-pin Pmod headers + 100-pin Hirose FX2";
        desc_.is_simulated = true;

        desc_.pinout_table = {
            {"VCCINT", "Core Logic Voltage", "1.0V", "Internal Rail", "Internal logic fabric supply"},
            {"VCCO_34", "I/O Bank 34 Voltage", "3.3V", "Power", "Supplies Pmod JB and JC logic"},
            {"GND", "Chassis & Signal Ground", "0.0V", "Ground", "Multi-point ground return"},
            {"PROG_B", "Configuration Reset", "3.3V", "Input (Active Low)", "Triggers FPGA bitstream reload"},
            {"DONE", "Bitstream Config Done", "3.3V", "Output (Open Drain)", "Asserted high when boot completes"},
            {"CLK100MHZ", "Primary System Oscillator", "3.3V", "Input Clock", "100 MHz clock to pin W5"}
        };
    }

    DeviceDescriptor get_descriptor() const override { return desc_; }
    ConnectionStatus get_status() const override { return status_; }

    bool connect() override {
        if (fault_injected_) {
            status_ = ConnectionStatus::Error_Timeout;
            return false;
        }
        status_ = ConnectionStatus::Connected;
        return true;
    }

    bool disconnect() override {
        status_ = ConnectionStatus::Disconnected;
        return true;
    }

    DeviceTelemetry poll_telemetry() override {
        DeviceTelemetry tel;
        tel.timestamp_iso = get_current_time_iso();
        tel.packet_counter = ++packet_seq_;

        if (status_ != ConnectionStatus::Connected && status_ != ConnectionStatus::Streaming) {
            tel.error_flag = true;
            tel.status_payload = "FPGA board unconfigured or disconnected";
            return tel;
        }

        if (fault_injected_) {
            status_ = ConnectionStatus::Error_Timeout;
            tel.error_flag = true;
            tel.status_payload = "TRANSACTION ERROR: PCIe/UART DMA transfer failure / CRC mismatch";
            return tel;
        }

        status_ = ConnectionStatus::Streaming;
        tel.core_voltage_V = 0.998 + 0.004 * std::sin(packet_seq_ * 0.3);
        tel.io_voltage_V = 3.295 + 0.008 * std::cos(packet_seq_ * 0.2);
        tel.current_mA = 412.0 + 15.0 * std::sin(packet_seq_ * 0.4);
        tel.junction_temp_C = 51.4 + 2.1 * std::sin(packet_seq_ * 0.1);
        tel.status_payload = "NORMAL: Bitstream active, DSP MAC utilization: 44%, MMCM locked";
        tel.error_flag = false;
        return tel;
    }

    void inject_fault(bool enable) override {
        fault_injected_ = enable;
        if (enable) {
            status_ = ConnectionStatus::Error_Timeout;
        }
    }

    bool recover_from_fault() override {
        fault_injected_ = false;
        status_ = ConnectionStatus::Connected;
        return true;
    }
};

// -------------------------------------------------------------
// JTAG Debug Adapter (FTDI FT2232H / Segger J-Link Base)
// -------------------------------------------------------------
class JtagAdapter : public IDeviceAdapter {
private:
    DeviceDescriptor desc_;
    ConnectionStatus status_ = ConnectionStatus::Disconnected;
    uint32_t packet_seq_ = 0;
    bool fault_injected_ = false;

public:
    JtagAdapter() {
        desc_.device_id = "DEV_DEBUG_JTAG";
        desc_.display_name = "SEGGER J-Link / FTDI MPSSE Dual-Channel JTAG Probe";
        desc_.type = DeviceType::JTAG_Debugger;
        desc_.vendor = "SEGGER Microcontroller / FTDI Chip";
        desc_.serial_number = "JL-59401823-PROBE";
        desc_.firmware_version = "J-Link ARM V11.00 compiled Feb 2026";
        desc_.host_interface = "USB 2.0 High-Speed (480 Mbps) Bulk Endpoint";
        desc_.power_requirements = "5.0V from Host USB bus; < 100 mA drawn";
        desc_.logic_levels = "VREF sensing input (1.2V to 5.0V). Level-shifted driver stages.";
        desc_.connector_type = "Standard 20-pin 0.1\" ARM JTAG Header / 10-pin Cortex Debug Header";
        desc_.is_simulated = true;

        desc_.pinout_table = {
            {"VTref (Pin 1)", "Target Reference Voltage", "1.2V - 5.0V", "Input (Sense)", "Drives bidirectional voltage level shifters"},
            {"nTRST (Pin 3)", "TAP Reset", "VTref domain", "Output (Probe)", "Optional asynchronous TAP reset"},
            {"TDI (Pin 5)", "Test Data In", "VTref domain", "Output (Probe)", "Serial test data shifted into target"},
            {"TMS (Pin 7)", "Test Mode Select", "VTref domain", "Output (Probe)", "Controls target TAP state machine"},
            {"TCK (Pin 9)", "Test Clock", "VTref domain", "Output (Probe)", "Clock output (configured up to 15 MHz)"},
            {"TDO (Pin 13)", "Test Data Out", "VTref domain", "Input (Probe)", "Serial test data shifted out from target"},
            {"nRESET (Pin 15)", "Target System Reset", "Open-drain", "Bi-directional", "Asserts system-level target reboot"},
            {"GND (Pins 4,6..)", "Ground Pins", "0.0V", "Ground", "Multiple pins ensure minimal ground bounce"}
        };
    }

    DeviceDescriptor get_descriptor() const override { return desc_; }
    ConnectionStatus get_status() const override { return status_; }

    bool connect() override {
        if (fault_injected_) {
            status_ = ConnectionStatus::Error_Timeout;
            return false;
        }
        status_ = ConnectionStatus::Connected;
        return true;
    }

    bool disconnect() override {
        status_ = ConnectionStatus::Disconnected;
        return true;
    }

    DeviceTelemetry poll_telemetry() override {
        DeviceTelemetry tel;
        tel.timestamp_iso = get_current_time_iso();
        tel.packet_counter = ++packet_seq_;

        if (status_ != ConnectionStatus::Connected && status_ != ConnectionStatus::Streaming) {
            tel.error_flag = true;
            tel.status_payload = "Debug probe uninitialized or disconnected";
            return tel;
        }

        if (fault_injected_) {
            status_ = ConnectionStatus::Error_Timeout;
            tel.error_flag = true;
            tel.status_payload = "JTAG TAP ERROR: Target VREF dropped below threshold / TDO line floating";
            return tel;
        }

        status_ = ConnectionStatus::Streaming;
        tel.core_voltage_V = 3.305 + 0.002 * std::sin(packet_seq_ * 0.1); // VTref sensed
        tel.io_voltage_V = 5.012 + 0.005 * std::cos(packet_seq_ * 0.1);   // USB rail
        tel.current_mA = 68.2 + 1.5 * std::sin(packet_seq_ * 0.8);
        tel.junction_temp_C = 36.1 + 0.5 * std::cos(packet_seq_ * 0.05);
        tel.status_payload = "NORMAL: TAP State: Shift-DR, IDCODE: 0x4BA00477 (ARM Cortex-M7), TCK: 12.0 MHz";
        tel.error_flag = false;
        return tel;
    }

    void inject_fault(bool enable) override {
        fault_injected_ = enable;
        if (enable) {
            status_ = ConnectionStatus::Error_Timeout;
        }
    }

    bool recover_from_fault() override {
        fault_injected_ = false;
        status_ = ConnectionStatus::Connected;
        return true;
    }
};

// -------------------------------------------------------------
// HardwareManager Implementation
// -------------------------------------------------------------
HardwareManager::HardwareManager() {
    discover_devices();
}

void HardwareManager::discover_devices() {
    devices_.clear();
    devices_.push_back(std::make_shared<McuAdapter>());
    devices_.push_back(std::make_shared<FpgaAdapter>());
    devices_.push_back(std::make_shared<JtagAdapter>());
}

std::vector<DeviceDescriptor> HardwareManager::list_devices() const {
    std::vector<DeviceDescriptor> list;
    for (const auto& dev : devices_) {
        list.push_back(dev->get_descriptor());
    }
    return list;
}

std::shared_ptr<IDeviceAdapter> HardwareManager::get_device(const std::string& device_id) {
    for (const auto& dev : devices_) {
        if (dev->get_descriptor().device_id == device_id) {
            return dev;
        }
    }
    return nullptr;
}

std::string HardwareManager::get_architecture_diagram_markdown() const {
    return R"(
```mermaid
graph TD
    subgraph Host Workstation [Windows 11 x64 Host Workstation]
        UEW[Unified Engineering Workbench]
        API[C++ Local REST API Server]
        UEW <--> API
    end

    subgraph USB Interface Layer
        USB_HUB[Host USB 3.0 / 2.0 Root Hub]
        API <-->|WinUSB / CDC-ACM| USB_HUB
    end

    subgraph Target System [Target Hardware Ecosystem]
        direction TB
        
        subgraph Compute Node 1 [Microcontroller Board: STM32H7]
            MCU_CORE[ARM Cortex-M7 @ 480 MHz]
            MCU_UART[UART Telemetry Transceiver - 3.3V]
            MCU_SWD[SWD / JTAG Target Header - 3.3V]
            MCU_CORE --- MCU_UART
            MCU_CORE --- MCU_SWD
        end

        subgraph Compute Node 2 [FPGA Board: Digilent Basys 3]
            FPGA_FABRIC[Xilinx Artix-7 Fabric - VCCINT 1.0V]
            FPGA_IO[VCCO Bank 3.3V IO & Pmods]
            FPGA_JTAG_HDR[FPGA JTAG Tap Chain - 3.3V]
            FPGA_FABRIC --- FPGA_IO
            FPGA_FABRIC --- FPGA_JTAG_HDR
        end

        subgraph Debug Node 3 [Hardware Debug Adapter: SEGGER J-Link]
            PROBE_ENGINE[JTAG TAP Controller Engine]
            LEVEL_SHIFTER[Bidirectional Voltage Level Shifters]
            VTREF_SENSE[VTref Sensing Stage: 1.2V - 5.0V]
            PROBE_ENGINE --- LEVEL_SHIFTER
            VTREF_SENSE --- LEVEL_SHIFTER
        end
    end

    USB_HUB <==>|Virtual COM Port| MCU_UART
    USB_HUB <==>|UART Data Stream| FPGA_IO
    USB_HUB <==>|High-Speed Bulk MPSSE| PROBE_ENGINE

    LEVEL_SHIFTER -.->|TCK, TMS, TDI, TDO| MCU_SWD
    LEVEL_SHIFTER -.->|Optional JTAG Chain| FPGA_JTAG_HDR
    MCU_SWD -.->|Target VDD Sensing| VTREF_SENSE
```
)";
}

std::string HardwareManager::get_pin_voltage_table_markdown() const {
    return R"(
### Comprehensive Hardware Interface & Pin-Voltage Domain Table

| Target Device | Operating Voltage Domains | Logic Levels | Level Shifting Required? | Primary Physical Connectors | Host Communication Protocol | JTAG Signal Ownership | Grounding & Electrical Safety |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Microcontroller**<br>(STM32H743ZI) | **Core:** 1.25V internal LDO<br>**I/O:** 3.3V VDD rail<br>**Analog:** 3.3V VDDA | 3.3V CMOS / LVTTL (Designated pins are 5V tolerant) | **Yes**, when interfacing to 5.0V legacy systems or 1.8V sensors. | Micro-USB Type-B, ST Morpho double headers, Arduino Uno V3 pinout. | USB CDC-ACM Virtual COM port @ 115200 baud uplink. | **Slave / Target Role** (Listens on SWCLK/SWDIO/JTMS; drives TDO only during Shift-IR/DR). | Star-topology ground return to USB shield; decoupled analog AGND via 0-ohm ferrite bead. |
| **FPGA Board**<br>(Artix-7 XC7A35T) | **Core:** 1.0V VCCINT<br>**Aux:** 1.8V VCCAUX<br>**I/O Banks:** 3.3V VCCO | 3.3V LVCMOS / LVTTL across Banks 14, 34, 35 | **Mandatory** for any off-board differential pairs (LVDS) or non-3.3V Pmods. | Micro-USB (FT2232 bridge), 4x 12-pin Pmod headers, 100-pin high density expansion. | FTDI FT2232HQ USB-to-FIFO / UART channel B. | **Slave / Target Role** (Boundary-scan TAP chain, receives TCK/TMS/TDI, outputs TDO). | Multi-layer PCB power planes with dedicated low-impedance ground plane; ESD transient TVS diodes. |
| **JTAG Debugger**<br>(SEGGER J-Link / FTDI MPSSE) | **VBUS:** 5.0V Host rail<br>**Target Ref:** 1.2V to 5.0V sensed via pin 1 (VTref) | Dynamic: matches target reference voltage ($V_{\text{target}}$) automatically | **Built-in active hardware level shifters** automatically track target VTref. | Standard 20-pin 0.1" pitch header and 10-pin Cortex Debug miniature header. | USB 2.0 High-Speed (480 Mbps) Bulk Endpoints. | **Master / Controller Role** (Owns and actively drives TCK, TMS, TDI, and nTRST/nRESET). | Connects all ground pins (Pins 4, 6, 8, 10, 12, 14, 16, 18, 20) to host USB return ground. |

#### Critical Engineering Distinction: Why JTAG is a Debug Interface, NOT a Generic Compute Board
1. **Asymmetric Master-Slave Topology**: A JTAG debug adapter is fundamentally a hardware master controller designed to shift boundary-scan vectors into IEEE 1149.1 / IEEE 1149.7 Test Access Ports (TAP). It contains state-machine microcode to manipulate registers in other chips, not application execution cores.
2. **Signal Ownership**: In JTAG, the debugger actively drives the clock (`TCK`), mode select (`TMS`), and test data (`TDI`), while the target chips (`MCU` or `FPGA`) passively sample those lines and drive only `TDO`. Connecting two debug adapters together would cause catastrophic output buffer contention.
3. **Voltage Heterogeneity**: Target compute boards run at distinct internal voltages (e.g., 1.0V FPGA core vs 3.3V MCU). The JTAG debugger relies on a high-impedance sense pin (`VTref`) to bias its internal level translators, preventing back-powering unpowered chips or blowing fragile 1.2V gates.
)";
}

} // namespace uew::hardware
