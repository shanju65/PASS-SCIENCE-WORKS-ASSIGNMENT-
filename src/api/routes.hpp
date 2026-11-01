#ifndef UEW_API_ROUTES_HPP
#define UEW_API_ROUTES_HPP

#include "http_server.hpp"
#include "../hardware/hardware_manager.hpp"

namespace uew::api {

/**
 * @brief Registers all REST API endpoints for the Unified Engineering Workbench.
 */
void register_all_routes(HttpServer& server, std::shared_ptr<hardware::HardwareManager> hw_mgr);

} // namespace uew::api

#endif // UEW_API_ROUTES_HPP
