#include "download.hpp"
#include "settings.hpp"
#include <algorithm>
#include <cstdio>
#include <cerrno>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace psgi {
namespace {
bool packageMagic(const std::string& bytes) { return bytes == std::string("\x7f" "CNT", 4); }
bool completeFile(const std::string& path, uint64_t expected) {
    if (!expected) return false;
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    const bool matches = !fseeko(file, 0, SEEK_END) && ftello(file) >= 0 && uint64_t(ftello(file)) == expected && !fseeko(file, 0, SEEK_SET);
    if (!matches) { std::fclose(file); return false; }
    char bytes[4]; const size_t count = std::fread(bytes, 1, sizeof(bytes), file); std::fclose(file);
    return count == 4 && packageMagic(std::string(bytes, 4));
}
}
std::string stagedPackagePath(const std::string& directory, const std::string& url) {
    // Stable across restarts; a URL never becomes a filesystem path.
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : url) { hash ^= c; hash *= 1099511628211ULL; }
    char name[32]; std::snprintf(name, sizeof(name), "/%016llx.pkg", static_cast<unsigned long long>(hash));
    return directory + "/downloads" + name;
}
void removeStagedPackage(const std::string& directory, const std::string& url) {
    const auto path = stagedPackagePath(directory, url);
    if (!unlink(path.c_str()) || errno == ENOENT) unlink((path + ".lease").c_str());
}
int installPackage(const Entry& entry, int userId, const std::string& directory,
                   const std::atomic<bool>* cancelled, const DownloadProgress& progress,
                   const std::function<void(const std::string&)>& note,
                   const std::function<void(InstallPhase)>& phase,
                   const ArchiveSession& session) {
    if (phase) phase(InstallPhase::Checking);
    if (note) note("Checking package header and download support");
    std::string magic;
    const auto probeHeader = [&](const std::string& url, bool range) {
        magic.clear();
        auto info = httpTransferAuthenticated(url, [&](const void* data, size_t count, uint64_t) {
            magic.append(static_cast<const char*>(data), std::min(count, size_t(4) - magic.size()));
            return magic.size() < 4;
        }, range, session, cancelled);
        if (!packageMagic(magic)) throw std::runtime_error("Package URL did not return a PS4 PKG file");
        if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
        return info;
    };
    // BGFT has its own firmware trust store. Keep HTTPS in our verified HTTP
    // client and obtain the full length so completed staging files can be reused.
    const bool range = entry.url.compare(0, 8, "https://") != 0;
    auto probe = probeHeader(entry.url, range);
    if (range && probe.effectiveUrl.compare(0, 8, "https://") == 0)
        probe = probeHeader(probe.effectiveUrl, false);
    if (probe.effectiveUrl.compare(0, 7, "http://") == 0 && probe.status == 206 && probe.length == 4) {
        if (phase) phase(InstallPhase::RegisteringRemote);
        if (note) note("Range supported; registering remote package with BGFT");
        auto resolved = entry;
        resolved.url = probe.effectiveUrl;
        return queueInstall(resolved, userId);
    }
    if (note) note("Downloading package to internal storage before installation");

    const std::string folder = directory + "/downloads";
    if (mkdir(folder.c_str(), 0700) && errno != EEXIST) throw std::runtime_error("Cannot create package staging directory");
    const std::string path = stagedPackagePath(directory, entry.url), part = path + ".part";
    if (!completeFile(path, probe.length)) {
        if (!access(path.c_str(), F_OK)) {
            std::string lease;
            try { lease = readFile(path + ".lease", 32); } catch (const std::exception&) {}
            if (lease != "released\n") throw std::runtime_error("Existing staged package may still be used by PS4. Finish/cancel it in Downloads before retrying.");
        }
        if (phase) phase(InstallPhase::Downloading);
        progress(0, probe.length);
        FILE* file = std::fopen(part.c_str(), "wb");
        if (!file) throw std::runtime_error("Cannot write package staging file");
        uint64_t received = 0, expected = 0;
        try {
            magic.clear();
            const uint64_t available = freeSpace(directory), reserve = 64 * 1024 * 1024;
            httpTransferAuthenticated(probe.effectiveUrl, [&](const void* data, size_t count, uint64_t total) {
                if (!received) {
                    if (!total) throw std::runtime_error("Package server must send Content-Length for a direct download");
                    if (!available) throw std::runtime_error("Cannot read internal free space for package staging");
                    if (available < reserve || total > available - reserve)
                        throw std::runtime_error("Not enough internal space to stage this package. Free internal storage and retry.");
                    expected = total;
                }
                if (count > expected - received) throw std::runtime_error("Package exceeds its advertised length");
                if (magic.size() < 4) magic.append(static_cast<const char*>(data), std::min(count, size_t(4) - magic.size()));
                if (magic.size() == 4 && !packageMagic(magic)) throw std::runtime_error("Download is not a PS4 PKG file");
                if (std::fwrite(data, 1, count, file) != count) throw std::runtime_error("Cannot write package; check internal free space");
                received += count; progress(received, expected);
                return true;
            }, false, session, cancelled);
            if (!expected || received != expected || !packageMagic(magic)) throw std::runtime_error("Incomplete package download; retry installation");
            if (std::fflush(file) || fsync(fileno(file))) throw std::runtime_error("Cannot flush downloaded package");
            const int closed = std::fclose(file); file = nullptr;
            if (closed || rename(part.c_str(), path.c_str())) throw std::runtime_error("Cannot save downloaded package");
            atomicWrite(path + ".lease", "released\n");
        } catch (...) { if (file) std::fclose(file); unlink(part.c_str()); throw; }
    } else if (note) note("Reusing complete downloaded package; no new download needed");
    if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
    if (phase) phase(InstallPhase::Registering);
    if (note) note("Download complete. Requesting PS4 installation; not installed yet.");
    atomicWrite(path + ".lease", "in-use\n");
    try { return queueLocalInstall(entry, path, userId); }
    catch (const SubmissionFailure& failure) {
        if (failure.storageReleased) atomicWrite(path + ".lease", "released\n");
        throw;
    }
}
}
