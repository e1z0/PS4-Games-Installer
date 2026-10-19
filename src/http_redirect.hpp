#pragma once
#include "catalog.hpp"
#include <stdexcept>

namespace psgi {
constexpr size_t MaxHttpHeaderBytes = 64 * 1024;
constexpr unsigned MaxHttpRedirects = 5;

inline bool isHttpRedirect(int status) {
    return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

inline std::string redirectLocation(const std::string& headers) {
    if (headers.size() > MaxHttpHeaderBytes) throw std::runtime_error("HTTP response headers exceed limit");
    std::string location;
    bool found = false;
    for (size_t begin = 0; begin < headers.size();) {
        const size_t end = headers.find('\n', begin);
        const std::string line = headers.substr(begin, end == std::string::npos ? end : end - begin);
        const size_t colon = line.find(':');
        if (colon != std::string::npos && lower(line.substr(0, colon)) == "location") {
            if (found) throw std::runtime_error("HTTP redirect has multiple Location headers");
            found = true;
            const size_t first = line.find_first_not_of(" \t", colon + 1);
            const size_t last = line.find_last_not_of(" \t\r");
            if (first != std::string::npos && first <= last) location = line.substr(first, last - first + 1);
        }
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    if (location.empty()) throw std::runtime_error("HTTP redirect is missing Location");
    return location;
}

// RFC 3986 dot-segment removal without decoding signed/escaped URL paths.
inline std::string redirectPath(std::string input) {
    std::string output;
    while (!input.empty()) {
        if (input.compare(0, 3, "../") == 0) input.erase(0, 3);
        else if (input.compare(0, 2, "./") == 0) input.erase(0, 2);
        else if (input.compare(0, 3, "/./") == 0 || input == "/.") input.replace(0, 3, "/");
        else if (input.compare(0, 4, "/../") == 0 || input == "/..") {
            input.replace(0, 4, "/");
            const size_t slash = output.rfind('/');
            output.erase(slash == std::string::npos ? 0 : slash);
        } else if (input == "." || input == "..") input.clear();
        else {
            const size_t end = input.find('/', input[0] == '/' ? 1 : 0);
            output += input.substr(0, end);
            input.erase(0, end == std::string::npos ? input.size() : end);
        }
    }
    return output;
}

inline std::string nextHttpUrl(const std::string& current, const std::string& location, unsigned redirects) {
    if (redirects >= MaxHttpRedirects) throw std::runtime_error("Too many HTTP redirects (max 5)");
    const size_t schemeEnd = current.find("://");
    const size_t authorityEnd = current.find_first_of("/?", schemeEnd + 3);
    const std::string origin = current.substr(0, authorityEnd);
    std::string target;
    if (location.compare(0, 7, "http://") == 0 || location.compare(0, 8, "https://") == 0) target = location;
    else if (location.compare(0, 2, "//") == 0) target = current.substr(0, schemeEnd + 1) + location;
    else {
        if (location.empty() || location.find(':') < location.find_first_of("/?"))
            throw std::runtime_error("Invalid HTTP redirect URL");
        std::string path = authorityEnd == std::string::npos ? "/" : current.substr(authorityEnd);
        if (path[0] == '?') path.insert(0, "/");
        path.erase(path.find('?') == std::string::npos ? path.size() : path.find('?'));
        if (location[0] == '?') target = origin + path + location;
        else {
            const std::string relative = location[0] == '/' ? location : path.substr(0, path.rfind('/') + 1) + location;
            const size_t query = relative.find('?');
            target = origin + redirectPath(relative.substr(0, query));
            if (query != std::string::npos) target += relative.substr(query);
        }
    }
    if (!isHttpUrl(target)) throw std::runtime_error("Invalid HTTP redirect URL");
    if (current.compare(0, 8, "https://") == 0 && target.compare(0, 8, "https://") != 0)
        throw std::runtime_error("HTTPS redirect to HTTP is not allowed");
    return target;
}
}
