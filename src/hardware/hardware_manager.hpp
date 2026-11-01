#ifndef UEW_HARDWARE_HARDWARE_MANAGER_HPP
#define UEW_HARDWARE_HARDWARE_MANAGER_HPP

#include <string>
#include <vector>
#include <memory>
#include <chrono>

namespace uew::hardware {

enum class DeviceType {
    Microcontroller,
    FPGA,
    JTAG_Debugger
};

enum class ConnectionStatus {
    Disconnected,
    Connecting,
    Connected,
    Streaming,
    Error_Timeout
};

struct DeviceTelemetry {
    std::string timestamp_iso;
    double core_voltage_V = 0.0;
    double io_voltage_V = 0.0;
    double current_mA = 0.0;
    double junction_temp_C = 0.0;
    uint32_t packet_counter = 0;
    std::string status_payload;
    bool error_flag = false;
};

struct PinInterfaceDef {
    std::string pin_name;
    std::string signal_role;
    std::string voltage_domain;
    std::string direction; // Input, Output, Bi-directional, Power
    std::string notes;
};

struct DeviceDescriptor {
    std::string device_id;
    std::string display_name;
    DeviceType type;
    std::string vendor;
    std::string serial_number;
    std::string firmware_version;
    std::string host_interface; // USB 2.0 High-Speed, UART, FTDI MPSSE
    std::string power_requirements;
    std::string logic_levels;
    std::string connector_type;
    bool is_simulated = true;
    std::vector<PinInterfaceDef> pinout_table;
};

/**
 * @brief Abstract Base Class for Embedded Hardware Adapters.
 */
class IDeviceAdapter {
public:
    virtual ~IDeviceAdapter() = default;
    virtual DeviceDescriptor get_descriptor() const = 0;
    virtual ConnectionStatus get_status() const = 0;
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual DeviceTelemetry poll_telemetry() = 0;
    virtual void inject_fault(bool enable_timeout) = 0;
    virtual bool recover_from_fault() = 0;
};

/**
 * @brief Manages discovery and lifecycle of all workbench hardware adapters.
 */
class HardwareManager {
private:
    std::vector<std::shared_ptr<IDeviceAdapter>> devices_;
public:
    HardwareManager();
    void discover_devices();
    std::vector<DeviceDescriptor> list_devices() const;
    std::shared_ptr<IDeviceAdapter> get_device(const std::string& device_id);
    
    // Global simulated state
    std::string get_architecture_diagram_markdown() const;
    std::string get_pin_voltage_table_markdown() const;
};

} // namespace uew::hardware

#endif // UEW_HARDWARE_HARDWARE_MANAGER_HPP
