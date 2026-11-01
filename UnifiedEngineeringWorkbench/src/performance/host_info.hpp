#pragma once

#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace uew {

struct HostInfo {
    std::string cpu_model = "unknown";
    int physical_cores = 0;
    int logical_cores = 0;
    double ram_gb = 0;
    std::string compiler = "unknown";
    std::string compiler_version = "unknown";
    std::string optimization_flags = "unknown";
};

inline HostInfo detect_host_info() {
    HostInfo h;
#if defined(_MSC_VER)
    h.compiler = "MSVC";
    h.compiler_version = std::to_string(_MSC_VER);
#if defined(NDEBUG)
    h.optimization_flags = "/O2 /DNDEBUG";
#else
    h.optimization_flags = "/Od (debug)";
#endif
#elif defined(__clang__)
    h.compiler = "Clang";
    h.compiler_version = __clang_version__;
#ifdef NDEBUG
    h.optimization_flags = "-O2 -DNDEBUG";
#else
    h.optimization_flags = "-O0";
#endif
#elif defined(__GNUC__)
    h.compiler = "GCC";
    h.compiler_version = std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#ifdef NDEBUG
    h.optimization_flags = "-O2 -DNDEBUG";
#else
    h.optimization_flags = "-O0";
#endif
#endif

#ifdef _WIN32
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    h.logical_cores = static_cast<int>(si.dwNumberOfProcessors);

    DWORD len = 0;
    GetLogicalProcessorInformation(nullptr, &len);
    std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buf;
    if (len > 0) {
        buf.resize(len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) + 1);
        if (GetLogicalProcessorInformation(buf.data(), &len)) {
            int packages = 0;
            const int count = static_cast<int>(len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
            for (int i = 0; i < count; ++i) {
                if (buf[i].Relationship == RelationProcessorCore) ++packages;
            }
            h.physical_cores = packages;
        }
    }
    if (h.physical_cores <= 0) h.physical_cores = h.logical_cores > 0 ? (h.logical_cores / 2) : 0;

    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        h.ram_gb = static_cast<double>(mem.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
    }

    HKEY key;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0, KEY_READ, &key) == ERROR_SUCCESS) {
        char name[256];
        DWORD nlen = sizeof(name);
        DWORD type = 0;
        if (RegQueryValueExA(key, "ProcessorNameString", nullptr, &type, reinterpret_cast<LPBYTE>(name), &nlen) ==
            ERROR_SUCCESS) {
            h.cpu_model = name;
            while (!h.cpu_model.empty() && h.cpu_model.front() == ' ') h.cpu_model.erase(h.cpu_model.begin());
        }
        RegCloseKey(key);
    }
#else
    h.logical_cores = static_cast<int>(std::thread::hardware_concurrency());
    h.physical_cores = h.logical_cores;
#endif
    if (h.logical_cores <= 0) h.logical_cores = 1;
    return h;
}

}  // namespace uew
