#pragma once

#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Small JSON helper used by the local API. Not a general-purpose library.
namespace uew {

inline std::string json_escape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    o += buf;
                } else {
                    o += static_cast<char>(c);
                }
        }
    }
    return o;
}

class Json {
public:
    enum Type { Null, Bool, Number, String, Array, Object };

    Json() : type_(Null) {}
    explicit Json(bool v) : type_(Bool), bool_(v) {}
    explicit Json(int v) : type_(Number), num_(static_cast<double>(v)) {}
    explicit Json(double v) : type_(Number), num_(v) {}
    explicit Json(const char* v) : type_(String), str_(v ? v : "") {}
    explicit Json(std::string v) : type_(String), str_(std::move(v)) {}

    static Json array() {
        Json j;
        j.type_ = Array;
        return j;
    }
    static Json object() {
        Json j;
        j.type_ = Object;
        return j;
    }
    static Json number_or_null(double v) {
        if (!std::isfinite(v)) return Json();
        return Json(v);
    }

    Type type() const { return type_; }
    bool is_null() const { return type_ == Null; }
    bool is_object() const { return type_ == Object; }
    bool is_array() const { return type_ == Array; }
    bool is_string() const { return type_ == String; }
    bool is_number() const { return type_ == Number; }
    bool is_bool() const { return type_ == Bool; }

    Json& operator[](const std::string& key) {
        if (type_ != Object) {
            *this = object();
        }
        return obj_[key];
    }
    const Json& operator[](const std::string& key) const {
        static Json nulls;
        auto it = obj_.find(key);
        return it == obj_.end() ? nulls : it->second;
    }

    void push(Json v) {
        if (type_ != Array) {
            *this = array();
        }
        arr_.push_back(std::move(v));
    }

    const std::vector<Json>& items() const { return arr_; }
    const std::map<std::string, Json>& fields() const { return obj_; }
    std::size_t size() const { return type_ == Array ? arr_.size() : obj_.size(); }

    bool as_bool(bool def = false) const { return type_ == Bool ? bool_ : def; }
    double as_number(double def = 0) const { return type_ == Number ? num_ : def; }
    int as_int(int def = 0) const { return type_ == Number ? static_cast<int>(num_) : def; }
    const std::string& as_string() const {
        static std::string empty;
        return type_ == String ? str_ : empty;
    }
    std::string as_string(const std::string& def) const {
        return type_ == String ? str_ : def;
    }

    bool has(const std::string& key) const {
        return type_ == Object && obj_.count(key) != 0;
    }

    std::string dump() const {
        std::ostringstream os;
        dump_to(os);
        return os.str();
    }

    static Json parse(const std::string& text, std::string& error) {
        Parser p(text);
        try {
            p.skip();
            Json v = p.parse_value();
            p.skip();
            if (p.i < p.s.size()) {
                error = "Unexpected trailing JSON content.";
                return Json();
            }
            error.clear();
            return v;
        } catch (const std::exception& ex) {
            error = ex.what();
            return Json();
        }
    }

private:
    Type type_ = Null;
    bool bool_ = false;
    double num_ = 0;
    std::string str_;
    std::vector<Json> arr_;
    std::map<std::string, Json> obj_;

    void dump_to(std::ostringstream& os) const {
        switch (type_) {
            case Null: os << "null"; break;
            case Bool: os << (bool_ ? "true" : "false"); break;
            case Number: {
                if (!std::isfinite(num_)) {
                    os << "null";
                    break;
                }
                os.precision(15);
                os << std::noshowpoint;
                os << num_;
                break;
            }
            case String: os << '"' << json_escape(str_) << '"'; break;
            case Array: {
                os << '[';
                for (std::size_t i = 0; i < arr_.size(); ++i) {
                    if (i) os << ',';
                    arr_[i].dump_to(os);
                }
                os << ']';
                break;
            }
            case Object: {
                os << '{';
                bool first = true;
                for (const auto& kv : obj_) {
                    if (!first) os << ',';
                    first = false;
                    os << '"' << json_escape(kv.first) << "\":";
                    kv.second.dump_to(os);
                }
                os << '}';
                break;
            }
        }
    }

    struct Parser {
        const std::string& s;
        std::size_t i = 0;
        explicit Parser(const std::string& in) : s(in) {}

        void skip() {
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        }
        char peek() {
            skip();
            return i < s.size() ? s[i] : '\0';
        }
        char get() {
            skip();
            if (i >= s.size()) throw std::runtime_error("Unexpected end of JSON.");
            return s[i++];
        }
        Json parse_value() {
            char c = peek();
            if (c == '{') return parse_object();
            if (c == '[') return parse_array();
            if (c == '"') return Json(parse_string());
            if (c == 't' || c == 'f') return Json(parse_bool());
            if (c == 'n') {
                parse_null();
                return Json();
            }
            return Json(parse_number());
        }
        Json parse_object() {
            if (get() != '{') throw std::runtime_error("Expected '{'.");
            Json o = object();
            skip();
            if (peek() == '}') {
                ++i;
                return o;
            }
            while (true) {
                std::string key = parse_string();
                if (get() != ':') throw std::runtime_error("Expected ':' after object key.");
                o[key] = parse_value();
                char c = get();
                if (c == '}') break;
                if (c != ',') throw std::runtime_error("Expected ',' or '}' in object.");
            }
            return o;
        }
        Json parse_array() {
            if (get() != '[') throw std::runtime_error("Expected '['.");
            Json a = array();
            if (peek() == ']') {
                ++i;
                return a;
            }
            while (true) {
                a.push(parse_value());
                char c = get();
                if (c == ']') break;
                if (c != ',') throw std::runtime_error("Expected ',' or ']' in array.");
            }
            return a;
        }
        std::string parse_string() {
            if (get() != '"') throw std::runtime_error("Expected string.");
            std::string out;
            while (i < s.size()) {
                char c = s[i++];
                if (c == '"') return out;
                if (c == '\\') {
                    if (i >= s.size()) throw std::runtime_error("Bad string escape.");
                    char e = s[i++];
                    switch (e) {
                        case '"':
                        case '\\':
                        case '/': out += e; break;
                        case 'n': out += '\n'; break;
                        case 'r': out += '\r'; break;
                        case 't': out += '\t'; break;
                        case 'u': {
                            if (i + 4 > s.size()) throw std::runtime_error("Bad unicode escape.");
                            i += 4;
                            out += '?';
                            break;
                        }
                        default: throw std::runtime_error("Unknown string escape.");
                    }
                } else {
                    out += c;
                }
            }
            throw std::runtime_error("Unterminated string.");
        }
        bool parse_bool() {
            if (s.compare(i, 4, "true") == 0) {
                i += 4;
                return true;
            }
            if (s.compare(i, 5, "false") == 0) {
                i += 5;
                return false;
            }
            throw std::runtime_error("Invalid boolean.");
        }
        void parse_null() {
            if (s.compare(i, 4, "null") == 0) {
                i += 4;
                return;
            }
            throw std::runtime_error("Invalid null.");
        }
        double parse_number() {
            std::size_t start = i;
            if (peek() == '-') ++i;
            if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) {
                throw std::runtime_error("Invalid number.");
            }
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
            if (i < s.size() && s[i] == '.') {
                ++i;
                if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) {
                    throw std::runtime_error("Invalid number fraction.");
                }
                while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
            }
            if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
                ++i;
                if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
                if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) {
                    throw std::runtime_error("Invalid number exponent.");
                }
                while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
            }
            try {
                return std::stod(s.substr(start, i - start));
            } catch (...) {
                throw std::runtime_error("Number out of range.");
            }
        }
    };
};

inline Json error_json(const std::string& code, const std::string& message) {
    Json j = Json::object();
    j["ok"] = Json(false);
    j["error_code"] = Json(code);
    j["error"] = Json(message);
    return j;
}

}  // namespace uew
