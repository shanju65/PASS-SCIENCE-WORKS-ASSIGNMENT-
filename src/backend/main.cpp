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

    std::cout << "========================================================\n";
    std::cout << "  UNIFIED ENGINEERING WORKBENCH - C++17 CORE BACKEND    \n";
    std::cout << "  PASS SCIENCE WORKS | R&D INTERNSHIP 2026-2027         \n";
    std::cout << "========================================================\n\n";

    // Initialize embedded hardware manager
    auto hw_mgr = std::make_shared<uew::hardware::HardwareManager>();

    // Start HTTP server (attempt port 8080, fallback to 8081 if occupied)
    int port = 8080;
    uew::api::HttpServer server(port, "frontend");
    uew::api::register_all_routes(server, hw_mgr);

    bool started = server.start(true);
    if (!started) {
        port = 8081;
        std::cout << "[WARN] Port 8080 occupied, falling back to port " << port << "...\n";
        uew::api::HttpServer server_fallback(port, "frontend");
        uew::api::register_all_routes(server_fallback, hw_mgr);
        if (!server_fallback.start(true)) {
            std::cerr << "[ERROR] Failed to bind local HTTP server to port " << port << ".\n";
            return 1;
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
    server.stop();
    std::cout << "[INFO] Shutdown complete. Goodbye!\n";
    return 0;
}
