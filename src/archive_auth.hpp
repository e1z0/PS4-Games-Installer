#pragma once
#include <string>

namespace psgi {
struct ArchiveSession {
    std::string user, sig;
    bool empty() const { return user.empty() || sig.empty(); }
};
ArchiveSession parseArchiveLogin(const std::string& response);
std::string archiveCookieForUrl(const std::string& url, const ArchiveSession& session);
ArchiveSession loadArchiveSession(const std::string& directory);
void saveArchiveSession(const std::string& directory, const ArchiveSession& session);
void clearArchiveSession(const std::string& directory);
}
