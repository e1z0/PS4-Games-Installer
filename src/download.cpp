#include "download.hpp"
#include "settings.hpp"
#include <algorithm>
#include <array>
#include <cstring>
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
namespace {
struct PreparedPackage { std::string remoteUrl; uint64_t size = 0; };
PreparedPackage preparePackage(const Entry& entry, const std::string& directory,
                              const std::atomic<bool>* cancelled, const DownloadProgress& progress,
                              const std::function<void(const std::string&)>& note,
                              const std::function<void(InstallPhase)>& phase,
                              const ArchiveSession& session, bool forceStaging) {
    if (phase) phase(InstallPhase::Checking);
    if (note) note("Checking package header and download support");
    std::string magic;
    const auto probeHeader = [&](const std::string& url, bool range) {
        for (unsigned attempt = 0;; ++attempt) {
            try {
                magic.clear();
                auto info = httpTransferAuthenticated(url, [&](const void* data, size_t count, uint64_t) {
                    magic.append(static_cast<const char*>(data), std::min(count, size_t(4) - magic.size()));
                    return magic.size() < 4;
                }, range, session, cancelled);
                if (!packageMagic(magic)) throw std::runtime_error("Package URL did not return a PS4 PKG file");
                if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
                return info;
            } catch (const HttpTransferFailure&) {
                if ((cancelled && *cancelled) || attempt == 2) throw;
                if (note) note("Package connection interrupted; retrying header check (" + std::to_string(attempt + 1) + "/2)");
            }
        }
    };
    // Queue downloads always stage before serial installation. The compatibility
    // wrapper may still hand a plain-HTTP Range server directly to BGFT.
    const bool range = !forceStaging && entry.url.compare(0, 8, "https://") != 0;
    auto probe = probeHeader(entry.url, range);
    if (range && probe.effectiveUrl.compare(0, 8, "https://") == 0)
        probe = probeHeader(probe.effectiveUrl, false);
    if (!forceStaging && probe.effectiveUrl.compare(0, 7, "http://") == 0 && probe.status == 206 && probe.length == 4)
        return {probe.effectiveUrl, 0};
    if (!probe.length) throw std::runtime_error("Package server must send Content-Length for a direct download");
    if (note) note("Downloading package to internal storage before installation");

    const std::string folder = directory + "/downloads";
    if (mkdir(folder.c_str(), 0700) && errno != EEXIST) throw std::runtime_error("Cannot create package staging directory");
    const std::string path = stagedPackagePath(directory, entry.url), part = path + ".part";
    if (completeFile(path, probe.length)) {
        if (note) note("Reusing complete downloaded package; no new download needed");
        progress(probe.length, probe.length);
        return {{}, probe.length};
    }
    if (!access(path.c_str(), F_OK)) {
        std::string lease;
        try { lease = readFile(path + ".lease", 32); } catch (const std::exception&) {}
        if (lease != "released\n") throw std::runtime_error("Existing staged package may still be used by PS4. Finish/cancel it in Downloads before retrying.");
    }
    if (phase) phase(InstallPhase::Downloading);
    FILE* file = std::fopen(part.c_str(), "r+b");
    if (!file && errno == ENOENT) file = std::fopen(part.c_str(), "w+b");
    if (!file) throw std::runtime_error("Cannot write package staging file");
    bool discardPartial = false;
    try {
        if (fseeko(file, 0, SEEK_END) || ftello(file) < 0) throw std::runtime_error("Cannot read partial package size");
        uint64_t saved = ftello(file);
        if (saved > probe.length) { discardPartial = true; throw std::runtime_error("Package size changed; retry to start a fresh download"); }
        const uint64_t available = freeSpace(directory), reserve = 64 * 1024 * 1024;
        if (!available) throw std::runtime_error("Cannot read internal free space for package staging");
        if (available < reserve || probe.length - saved > available - reserve)
            throw std::runtime_error("Not enough internal space to stage this package. Free internal storage and retry.");
        progress(saved, probe.length);
        for (unsigned attempt = 0;; ++attempt) {
            if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
            if (fseeko(file, 0, SEEK_SET)) throw std::runtime_error("Cannot seek partial package");
            const uint64_t prefix = saved;
            uint64_t received = 0;
            bool appending = false;
            magic.clear();
            if (prefix && note) note("Rechecking " + formatSize(prefix) + " of saved package data; server stream must match before appending");
            try {
                httpTransferAuthenticated(probe.effectiveUrl, [&](const void* data, size_t count, uint64_t total) {
                    if (total != probe.length) { discardPartial = true; throw std::runtime_error("Package Content-Length changed; retry to start a fresh download"); }
                    if (received > total || count > total - received) { discardPartial = true; throw std::runtime_error("Package exceeds its advertised length"); }
                    if (magic.size() < 4) magic.append(static_cast<const char*>(data), std::min(count, size_t(4) - magic.size()));
                    if (magic.size() == 4 && !packageMagic(magic)) { discardPartial = true; throw std::runtime_error("Download is not a PS4 PKG file"); }
                    // This server may ignore Range. Compare every saved byte rather
                    // than blindly appending a second full response or mixing files.
                    const auto* bytes = static_cast<const char*>(data);
                    size_t checked = 0;
                    std::array<char, 8192> previous;
                    while (checked < count && received + checked < prefix) {
                        const size_t n = std::min({count - checked, previous.size(), size_t(prefix - received - checked)});
                        if (std::fread(previous.data(), 1, n, file) != n) throw std::runtime_error("Cannot read saved package data");
                        if (std::memcmp(previous.data(), bytes + checked, n)) {
                            discardPartial = true; throw std::runtime_error("Package source changed; retry to start a fresh download");
                        }
                        checked += n;
                    }
                    if (checked < count) {
                        if (!appending) {
                            if (fseeko(file, prefix, SEEK_SET)) throw std::runtime_error("Cannot seek partial package");
                            appending = true;
                        }
                        if (std::fwrite(bytes + checked, 1, count - checked, file) != count - checked)
                            throw std::runtime_error("Cannot write package; check internal free space");
                        saved += count - checked;
                    }
                    received += count;
                    progress(saved, probe.length);
                    return true;
                }, false, session, cancelled);
                if (received != probe.length || !packageMagic(magic)) throw HttpTransferFailure("Incomplete package download; retry installation");
                break;
            } catch (const HttpTransferFailure& failure) {
                if (std::fflush(file)) throw std::runtime_error("Cannot flush partial package");
                if ((cancelled && *cancelled) || attempt == 2) throw;
                if (note) note(std::string(failure.what()) + "; retained " + formatSize(saved) + ", reconnecting (" + std::to_string(attempt + 1) + "/2)");
            }
        }
        if (std::fflush(file) || fsync(fileno(file))) throw std::runtime_error("Cannot flush downloaded package");
        const int closed = std::fclose(file); file = nullptr;
        if (closed || rename(part.c_str(), path.c_str())) throw std::runtime_error("Cannot save downloaded package");
        atomicWrite(path + ".lease", "released\n");
    } catch (...) {
        if (file) {
            std::fflush(file);
            if (!fseeko(file, 0, SEEK_END) && ftello(file) == 0) discardPartial = true;
            std::fclose(file);
        }
        if (discardPartial) unlink(part.c_str());
        throw;
    }
    return {{}, probe.length};
}
}
uint64_t downloadPackage(const Entry& entry, const std::string& directory,
                         const std::atomic<bool>* cancelled, const DownloadProgress& progress,
                         const std::function<void(const std::string&)>& note, const ArchiveSession& session) {
    return preparePackage(entry, directory, cancelled, progress, note, {}, session, true).size;
}
int installDownloadedPackage(const Entry& entry, uint64_t size, int userId, const std::string& directory) {
    const auto path = stagedPackagePath(directory, entry.url);
    if (!completeFile(path, size)) throw std::runtime_error("Downloaded package is missing or incomplete; retry the download");
    atomicWrite(path + ".lease", "in-use\n");
    try { return queueLocalInstall(entry, path, userId); }
    catch (const SubmissionFailure& failure) {
        if (failure.storageReleased) atomicWrite(path + ".lease", "released\n");
        throw;
    }
}
int installPackage(const Entry& entry, int userId, const std::string& directory,
                   const std::atomic<bool>* cancelled, const DownloadProgress& progress,
                   const std::function<void(const std::string&)>& note,
                   const std::function<void(InstallPhase)>& phase, const ArchiveSession& session) {
    const auto prepared = preparePackage(entry, directory, cancelled, progress, note, phase, session, false);
    if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
    if (!prepared.remoteUrl.empty()) {
        if (phase) phase(InstallPhase::RegisteringRemote);
        auto resolved = entry; resolved.url = prepared.remoteUrl;
        return queueInstall(resolved, userId);
    }
    if (phase) phase(InstallPhase::Registering);
    if (note) note("Download complete. Requesting PS4 installation; not installed yet.");
    return installDownloadedPackage(entry, prepared.size, userId, directory);
}
}
