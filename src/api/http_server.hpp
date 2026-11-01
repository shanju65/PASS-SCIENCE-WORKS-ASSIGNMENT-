#ifndef UEW_API_HTTP_SERVER_HPP
#define UEW_API_HTTP_SERVER_HPP

#include <string>
#include <functional>
#include <unordered_map>
#include <atomic>
#include <memory>

namespace uew::api {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query_string;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status_code = 200;
    std::string content_type = "text/plain";
    std::unordered_map<std::string, std::string> extra_headers;
    std::string body;

    static HttpResponse json(const std::string& json_str, int status = 200) {
        HttpResponse res;
        res.status_code = status;
        res.content_type = "application/json";
        res.body = json_str;
        return res;
    }

    static HttpResponse text(const std::string& txt, int status = 200) {
        HttpResponse res;
        res.status_code = status;
        res.content_type = "text/plain";
        res.body = txt;
        return res;
    }

    static HttpResponse error(int status, const std::string& message) {
        HttpResponse res;
        res.status_code = status;
        res.content_type = "application/json";
        res.body = "{\"success\":false,\"error\":\"" + message + "\"}";
        return res;
    }
};

using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

class HttpServer {
private:
    int port_ = 8080;
    std::string web_root_dir_ = "frontend";
    std::atomic<bool> is_running_{false};
    uintptr_t listen_socket_ = 0; // SOCKET representation

    std::unordered_map<std::string, RouteHandler> get_routes_;
    std::unordered_map<std::string, RouteHandler> post_routes_;

    void process_client(uintptr_t client_sock);
    HttpResponse serve_static_file(const std::string& rel_path);

public:
    HttpServer(int port = 8080, const std::string& web_root = "frontend");
    ~HttpServer();

    void add_get_route(const std::string& path, RouteHandler handler);
    void add_post_route(const std::string& path, RouteHandler handler);

    bool start(bool non_blocking = true);
    void stop();
    bool is_running() const { return is_running_.load(); }
    int get_port() const { return port_; }
};

} // namespace uew::api

#endif // UEW_API_HTTP_SERVER_HPP
