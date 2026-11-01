#include "http_server.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

namespace uew::api {

static std::string get_mime_type(const std::string& path) {
    if (path.rfind(".html") != std::string::npos || path.rfind(".htm") != std::string::npos) return "text/html; charset=utf-8";
    if (path.rfind(".css") != std::string::npos) return "text/css; charset=utf-8";
    if (path.rfind(".js") != std::string::npos) return "application/javascript; charset=utf-8";
    if (path.rfind(".json") != std::string::npos) return "application/json; charset=utf-8";
    if (path.rfind(".stl") != std::string::npos) return "text/plain; charset=utf-8";
    if (path.rfind(".csv") != std::string::npos) return "text/csv; charset=utf-8";
    if (path.rfind(".png") != std::string::npos) return "image/png";
    if (path.rfind(".jpg") != std::string::npos || path.rfind(".jpeg") != std::string::npos) return "image/jpeg";
    if (path.rfind(".svg") != std::string::npos) return "image/svg+xml";
    return "text/plain; charset=utf-8";
}

HttpServer::HttpServer(int port, const std::string& web_root) 
    : port_(port), web_root_dir_(web_root) {
#if defined(_WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

HttpServer::~HttpServer() {
    stop();
#if defined(_WIN32)
    WSACleanup();
#endif
}

void HttpServer::add_get_route(const std::string& path, RouteHandler handler) {
    get_routes_[path] = handler;
}

void HttpServer::add_post_route(const std::string& path, RouteHandler handler) {
    post_routes_[path] = handler;
}

HttpResponse HttpServer::serve_static_file(const std::string& rel_path) {
    std::string clean_path = rel_path;
    if (clean_path == "/" || clean_path.empty()) {
        clean_path = "/index.html";
    }

    // Try in web_root_dir_ first
    std::string full_path = web_root_dir_ + clean_path;
    std::ifstream file(full_path, std::ios::binary);

    // If not found and path starts with /data/, try root data directory
    if (!file.is_open() && clean_path.rfind("/data/", 0) == 0) {
        full_path = clean_path.substr(1); // remove leading slash -> "data/..."
        file.open(full_path, std::ios::binary);
    }

    if (!file.is_open()) {
        return HttpResponse::error(404, "File not found: " + clean_path);
    }

    std::ostringstream ss;
    ss << file.rdbuf();

    HttpResponse res;
    res.status_code = 200;
    res.content_type = get_mime_type(clean_path);
    res.body = ss.str();
    return res;
}

void HttpServer::process_client(uintptr_t client_sock_handle) {
    SOCKET client_sock = static_cast<SOCKET>(client_sock_handle);
    char buffer[16384];
    int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received <= 0) {
        closesocket(client_sock);
        return;
    }

    buffer[bytes_received] = '\0';
    std::string request_raw(buffer, bytes_received);

    // Parse HTTP request line
    std::istringstream stream(request_raw);
    std::string method, full_uri, protocol;
    stream >> method >> full_uri >> protocol;

    std::string path = full_uri;
    std::string query_string;
    size_t q_pos = full_uri.find('?');
    if (q_pos != std::string::npos) {
        path = full_uri.substr(0, q_pos);
        query_string = full_uri.substr(q_pos + 1);
    }

    HttpRequest req;
    req.method = method;
    req.path = path;
    req.query_string = query_string;

    // Headers
    std::string line;
    std::getline(stream, line); // finish request line
    int content_length = 0;

    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string header_name = line.substr(0, colon);
            std::string header_val = line.substr(colon + 1);
            while (!header_val.empty() && (header_val.front() == ' ' || header_val.front() == '\t')) header_val.erase(0, 1);
            while (!header_val.empty() && (header_val.back() == '\r' || header_val.back() == ' ')) header_val.pop_back();
            req.headers[header_name] = header_val;

            if (header_name == "Content-Length" || header_name == "content-length") {
                try { content_length = std::stoi(header_val); } catch (...) {}
            }
        }
    }

    // Body extraction
    size_t header_end_pos = request_raw.find("\r\n\r\n");
    if (header_end_pos != std::string::npos) {
        req.body = request_raw.substr(header_end_pos + 4);
        // If content length indicates more bytes than initially read, read remaining bytes
        int remaining = content_length - static_cast<int>(req.body.size());
        while (remaining > 0) {
            int more = recv(client_sock, buffer, std::min<int>(remaining, sizeof(buffer)), 0);
            if (more <= 0) break;
            req.body.append(buffer, more);
            remaining -= more;
        }
    }

    // Route matching
    HttpResponse resp;
    if (method == "OPTIONS") {
        // CORS preflight
        resp.status_code = 204;
        resp.content_type = "text/plain";
    } else if (method == "GET") {
        auto it = get_routes_.find(path);
        if (it != get_routes_.end()) {
            resp = it->second(req);
        } else {
            resp = serve_static_file(path);
        }
    } else if (method == "POST") {
        auto it = post_routes_.find(path);
        if (it != post_routes_.end()) {
            resp = it->second(req);
        } else {
            resp = HttpResponse::error(404, "Endpoint not found: " + path);
        }
    } else {
        resp = HttpResponse::error(405, "Method Not Allowed");
    }

    // Build HTTP response
    std::ostringstream out;
    out << "HTTP/1.1 " << resp.status_code << " OK\r\n";
    out << "Content-Type: " << resp.content_type << "\r\n";
    out << "Content-Length: " << resp.body.size() << "\r\n";
    out << "Access-Control-Allow-Origin: *\r\n";
    out << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    out << "Access-Control-Allow-Headers: Content-Type\r\n";
    out << "Connection: close\r\n";

    for (const auto& h : resp.extra_headers) {
        out << h.first << ": " << h.second << "\r\n";
    }
    out << "\r\n";
    out << resp.body;

    std::string response_str = out.str();
    send(client_sock, response_str.data(), static_cast<int>(response_str.size()), 0);
    closesocket(client_sock);
}

bool HttpServer::start(bool non_blocking) {
    if (is_running_.load()) return true;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return false;
    }

    // Set reuse address
    int opt = 1;
#if defined(_WIN32)
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(static_cast<u_short>(port_));

    if (bind(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return false;
    }

    if (listen(sock, 32) == SOCKET_ERROR) {
        closesocket(sock);
        return false;
    }

    listen_socket_ = static_cast<uintptr_t>(sock);
    is_running_.store(true);

    auto accept_loop = [this, sock]() {
        while (is_running_.load()) {
            sockaddr_in client_addr;
            int client_len = sizeof(client_addr);
            SOCKET client_sock = accept(sock, (sockaddr*)&client_addr, &client_len);

            if (client_sock == INVALID_SOCKET) {
                if (!is_running_.load()) break;
                continue;
            }

            // Spawn worker thread for each client to ensure interface never hangs
            std::thread([this, client_sock]() {
                this->process_client(static_cast<uintptr_t>(client_sock));
            }).detach();
        }
    };

    if (non_blocking) {
        std::thread(accept_loop).detach();
    } else {
        accept_loop();
    }

    return true;
}

void HttpServer::stop() {
    if (is_running_.exchange(false)) {
        if (listen_socket_ != 0) {
            closesocket(static_cast<SOCKET>(listen_socket_));
            listen_socket_ = 0;
        }
    }
}

} // namespace uew::api
