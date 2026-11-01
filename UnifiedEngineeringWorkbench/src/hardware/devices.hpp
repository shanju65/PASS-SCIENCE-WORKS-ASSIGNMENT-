#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace uew {

enum class DeviceKind { Microcontroller, Fpga, JtagAdapter };

inline const char* kind_name(DeviceKind k) {
    switch (k) {
        case DeviceKind::Microcontroller: return "microcontroller";
        case DeviceKind::Fpga: return "fpga";
        case DeviceKind::JtagAdapter: return "jtag_adapter";
    }
    return "unknown";
}

struct DeviceInfo {
    std::string id;
    std::string display_name;
    DeviceKind kind = DeviceKind::Microcontroller;
    std::string role;
    std::vector<std::string> capabilities;
    bool simulated = true;
};

struct HardwareStatus {
    bool ok = false;
    std::string error;
    std::string error_code;
    DeviceInfo info;
    bool connected = false;
    std::string telemetry_note;
    std::string telemetry_payload;
};

class SimulatedDevice {
public:
    explicit SimulatedDevice(DeviceInfo info) : info_(std::move(info)) {}

    DeviceInfo info() const {
        std::lock_guard<std::mutex> lock(mu_);
        return info_;
    }

    HardwareStatus connect() {
        std::lock_guard<std::mutex> lock(mu_);
        HardwareStatus s;
        s.info = info_;
        if (force_timeout_) {
            force_timeout_ = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms_ + 20));
            s.error_code = "TIMEOUT";
            s.error = "SIMULATED timeout while opening " + info_.display_name +
                      ". No physical bus traffic occurred.";
            return s;
        }
        if (force_fail_) {
            force_fail_ = false;
            s.error_code = "RECOVERABLE_FAILURE";
            s.error = "SIMULATED recoverable failure on " + info_.display_name +
                      ". The next connect attempt is allowed to succeed.";
            return s;
        }
        connected_ = true;
        s.ok = true;
        s.connected = true;
        return s;
    }

    HardwareStatus disconnect() {
        std::lock_guard<std::mutex> lock(mu_);
        HardwareStatus s;
        s.info = info_;
        connected_ = false;
        s.ok = true;
        s.connected = false;
        return s;
    }

    HardwareStatus telemetry() {
        std::lock_guard<std::mutex> lock(mu_);
        HardwareStatus s;
        s.info = info_;
        s.connected = connected_;
        if (!connected_) {
            s.error_code = "NOT_CONNECTED";
            s.error = "Connect the simulated device before requesting telemetry.";
            return s;
        }
        s.ok = true;
        s.telemetry_note = "SYNTHETIC telemetry. Not a physical measurement.";
        if (info_.kind == DeviceKind::Microcontroller) {
            s.telemetry_payload = "heartbeat=ok; firmware=sim-0.1; adc_counts=512";
        } else if (info_.kind == DeviceKind::Fpga) {
            s.telemetry_payload = "heartbeat=ok; bitstream=sim-placeholder; fabric_temp_flag=nominal";
        } else {
            s.telemetry_payload = "heartbeat=ok; tap_state=Run-Test/Idle; ir_length_bits=4";
        }
        return s;
    }

    void inject_timeout() {
        std::lock_guard<std::mutex> lock(mu_);
        force_timeout_ = true;
    }
    void inject_failure() {
        std::lock_guard<std::mutex> lock(mu_);
        force_fail_ = true;
    }

private:
    mutable std::mutex mu_;
    DeviceInfo info_;
    bool connected_ = false;
    bool force_timeout_ = false;
    bool force_fail_ = false;
    int timeout_ms_ = 80;
};

class HardwareHub {
public:
    HardwareHub() {
        DeviceInfo mcu;
        mcu.id = "sim-mcu-01";
        mcu.display_name = "Simulated MCU board";
        mcu.kind = DeviceKind::Microcontroller;
        mcu.role = "Application microcontroller (compute/control).";
        mcu.capabilities = {"discover", "connect", "disconnect", "telemetry", "timeout", "recoverable_failure"};
        mcu.simulated = true;

        DeviceInfo fpga;
        fpga.id = "sim-fpga-01";
        fpga.display_name = "Simulated FPGA board";
        fpga.kind = DeviceKind::Fpga;
        fpga.role = "Reconfigurable logic board (not interchangeable with the MCU).";
        fpga.capabilities = {"discover", "connect", "disconnect", "telemetry", "timeout", "recoverable_failure"};
        fpga.simulated = true;

        DeviceInfo jtag;
        jtag.id = "sim-jtag-01";
        jtag.display_name = "Simulated JTAG adapter";
        jtag.kind = DeviceKind::JtagAdapter;
        jtag.role = "Debug/programming adapter. Not a compute board.";
        jtag.capabilities = {"discover", "connect", "disconnect", "telemetry", "timeout", "recoverable_failure"};
        jtag.simulated = true;

        devices_.emplace(mcu.id, SimulatedDevice(mcu));
        devices_.emplace(fpga.id, SimulatedDevice(fpga));
        devices_.emplace(jtag.id, SimulatedDevice(jtag));
        order_ = {mcu.id, fpga.id, jtag.id};
    }

    std::vector<DeviceInfo> discover() {
        std::vector<DeviceInfo> out;
        for (const auto& id : order_) out.push_back(devices_.at(id).info());
        return out;
    }

    SimulatedDevice* get(const std::string& id) {
        auto it = devices_.find(id);
        return it == devices_.end() ? nullptr : &it->second;
    }

private:
    std::map<std::string, SimulatedDevice> devices_;
    std::vector<std::string> order_;
};

}  // namespace uew
