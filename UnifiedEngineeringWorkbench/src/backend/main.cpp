#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

#include "api/http_server.hpp"
#include "api/routes.hpp"
#include "hardware/hardware_manager.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

std::atomic<bool> g_keep_running(true);

void signal_handler(int signum) {
    (void)signum;
    g_keep_running.store(false);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

#if defined(_WIN32)
    // Anchor current working directory to the directory where Launch.exe resides
    // This ensures double-clicking from shortcuts, start menu, or external directories always finds frontend/ and data/
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_path, MAX_PATH)) {
        std::string path_str(exe_path);
        size_t last_slash = path_str.find_last_of("\\/");
        if (last_slash != std::string::npos) {
            SetCurrentDirectoryA(path_str.substr(0, last_slash).c_str());
        }
    }
#endif

    std::cout << "========================================================\n";
    std::cout << "  UNIFIED ENGINEERING WORKBENCH - C++17 CORE BACKEND    \n";
    std::cout << "  PASS SCIENCE WORKS | R&D INTERNSHIP 2026-2027         \n";
    std::cout << "========================================================\n\n";

    // Initialize embedded hardware manager
    auto hw_mgr = std::make_shared<uew::hardware::HardwareManager>();

    // Start HTTP server (attempt port 8080, fallback to 8081 or 8082 if occupied)
    int port = 8080;
    std::unique_ptr<uew::api::HttpServer> active_server = std::make_unique<uew::api::HttpServer>(port, "frontend");
    uew::api::register_all_routes(*active_server, hw_mgr);

    bool started = active_server->start(true);
    if (!started) {
        port = 8081;
        std::cout << "[WARN] Port 8080 occupied, falling back to port " << port << "...\n";
        active_server = std::make_unique<uew::api::HttpServer>(port, "frontend");
        uew::api::register_all_routes(*active_server, hw_mgr);
        started = active_server->start(true);
        if (!started) {
            port = 8082;
            std::cout << "[WARN] Port 8081 occupied, falling back to port " << port << "...\n";
            active_server = std::make_unique<uew::api::HttpServer>(port, "frontend");
            uew::api::register_all_routes(*active_server, hw_mgr);
            started = active_server->start(true);
            if (!started) {
                std::cerr << "[ERROR] Failed to bind local HTTP server to ports 8080, 8081, or 8082.\n";
                return 1;
            }
        }
    }

    std::string app_url = "http://localhost:" + std::to_string(port);
    std::cout << "[INFO] Backend online at: " << app_url << "\n";
    std::cout << "[INFO] Launching default web browser frontend...\n";

#if defined(_WIN32)
    // One-click launch requirement: open web browser automatically
    ShellExecuteA(NULL, "open", app_url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif

    std::cout << "[INFO] Unified Engineering Workbench is ready and listening.\n";
    std::cout << "[INFO] Press Ctrl+C in this console or close window to shutdown.\n\n";

    // Application loop
    while (g_keep_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n[INFO] Graceful shutdown initiated...\n";
    if (active_server) {
        active_server->stop();
    }
    std::cout << "[INFO] Shutdown complete. Goodbye!\n";
    return 0;
}
