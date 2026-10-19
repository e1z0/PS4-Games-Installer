#include "archive_auth.hpp"
#include "settings.hpp"
#include "json.hpp"
#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace psgi {
namespace {
constexpr size_t MaxSessionBytes = 16 * 1024;
constexpr size_t MaxLoginBytes = 64 * 1024;
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;

Json parseObject(const std::string& text, size_t limit) {
    // cJSON strings are NUL-terminated, so reject escaped NULs before parsing.
    if (text.size() > limit || text.find('\0') != std::string::npos ||
        text.find("\\u0000") != std::string::npos)
        throw std::runtime_error("Invalid Archive.org session data");
    Json root(cJSON_ParseWithLengthOpts(text.c_str(), text.size() + 1, nullptr, true), cJSON_Delete);
    if (!root || !cJSON_IsObject(root.get())) throw std::runtime_error("Invalid Archive.org session data");
    return root;
}
const cJSON* field(const cJSON* object, const char* name) {
    const cJSON* found = nullptr;
    if (cJSON_IsObject(object)) for (const auto* item = object->child; item; item = item->next) {
        if (item->string && std::strcmp(item->string, name) == 0) {
            if (found) throw std::runtime_error("Invalid Archive.org session data");
            found = item;
        }
    }
    return found;
}
void validateSession(const ArchiveSession& session) {
    for (const auto* value : {&session.user, &session.sig}) {
        if (value->empty() || value->size() > 4096) throw std::runtime_error("Invalid Archive.org session");
        for (const unsigned char c : *value)
            if (c < 0x21 || c > 0x7e || c == '"' || c == ',' || c == ';' || c == '\\')
                throw std::runtime_error("Invalid Archive.org session");
    }
}
ArchiveSession readCookies(const cJSON* object) {
    const auto* user = field(object, "logged-in-user");
    const auto* sig = field(object, "logged-in-sig");
    if (!cJSON_IsString(user) || !cJSON_IsString(sig)) throw std::runtime_error("Invalid Archive.org session");
    ArchiveSession session{user->valuestring, sig->valuestring};
    validateSession(session);
    return session;
}
bool archiveOrigin(const std::string& url) {
    if (url.size() < 8 || lower(url.substr(0, 8)) != "https://") return false;
    for (const unsigned char c : url) if (c <= 32 || c == 127 || c == '\\') return false;
    auto host = lower(url.substr(8, url.find_first_of("/?#", 8) - 8));
    const auto port = host.find(':');
    if (port != std::string::npos) {
        if (host.substr(port) != ":443") return false;
        host.resize(port);
    }
    if (host != "archive.org" &&
        (host.size() <= 12 || host.compare(host.size() - 12, 12, ".archive.org") != 0)) return false;
    if (host.size() > 253) return false;
    size_t start = 0;
    for (size_t end = 0; end <= host.size(); ++end) {
        if (end == host.size() || host[end] == '.') {
            if (end == start || end - start > 63 || host[start] == '-' || host[end - 1] == '-') return false;
            start = end + 1;
        } else if (!((host[end] >= 'a' && host[end] <= 'z') ||
                     (host[end] >= '0' && host[end] <= '9') || host[end] == '-')) return false;
    }
    return true;
}
std::string sessionPath(const std::string& directory) { return directory + "/archive-session.json"; }
}

ArchiveSession parseArchiveLogin(const std::string& response) {
    try {
        const auto root = parseObject(response, MaxLoginBytes);
        if (!cJSON_IsTrue(field(root.get(), "success"))) throw std::runtime_error("Login rejected");
        return readCookies(field(field(root.get(), "values"), "cookies"));
    } catch (const std::exception&) {
        throw std::runtime_error("Archive.org sign-in failed; check your email and password");
    }
}
std::string archiveCookieForUrl(const std::string& url, const ArchiveSession& session) {
    if (!archiveOrigin(url) || (session.user.empty() && session.sig.empty())) return {};
    validateSession(session);
    return "logged-in-user=" + session.user + "; logged-in-sig=" + session.sig;
}
ArchiveSession loadArchiveSession(const std::string& directory) {
    try {
        const auto path = sessionPath(directory);
        struct stat st{};
        if (lstat(path.c_str(), &st)) {
            if (errno == ENOENT) return {};
            throw std::runtime_error("Session unavailable");
        }
        if (!S_ISREG(st.st_mode)) throw std::runtime_error("Invalid session file");
        const auto root = parseObject(readFile(path, MaxSessionBytes), MaxSessionBytes);
        const auto* schema = field(root.get(), "schema");
        if (!cJSON_IsNumber(schema) || schema->valuedouble != 1) throw std::runtime_error("Invalid session version");
        return readCookies(root.get());
    } catch (const std::exception&) {
        throw std::runtime_error("Cannot read Archive.org session; sign out and sign in again");
    }
}
void saveArchiveSession(const std::string& directory, const ArchiveSession& session) {
    try {
        validateSession(session);
        Json root(cJSON_CreateObject(), cJSON_Delete);
        if (!root || !cJSON_AddNumberToObject(root.get(), "schema", 1) ||
            !cJSON_AddStringToObject(root.get(), "logged-in-user", session.user.c_str()) ||
            !cJSON_AddStringToObject(root.get(), "logged-in-sig", session.sig.c_str()))
            throw std::runtime_error("Session allocation failed");
        std::unique_ptr<char, decltype(&cJSON_free)> text(printJson(root.get()), cJSON_free);
        if (!text) throw std::runtime_error("Session allocation failed");
        if (mkdir(directory.c_str(), 0700) && errno != EEXIST) throw std::runtime_error("Session directory unavailable");
        // A stale temporary file must not retain a previously permissive mode.
        const auto path = sessionPath(directory);
        if (unlink((path + ".tmp").c_str()) && errno != ENOENT) throw std::runtime_error("Session temporary file unavailable");
        atomicWrite(path, text.get());
    } catch (const std::exception&) {
        throw std::runtime_error("Cannot save Archive.org session");
    }
}
void clearArchiveSession(const std::string& directory) {
    const auto path = sessionPath(directory);
    const bool temporaryFailed = unlink((path + ".tmp").c_str()) && errno != ENOENT;
    const bool sessionFailed = unlink(path.c_str()) && errno != ENOENT;
    if (temporaryFailed || sessionFailed) throw std::runtime_error("Cannot clear Archive.org session");
}
}
