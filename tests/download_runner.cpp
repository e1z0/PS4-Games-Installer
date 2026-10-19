#include "download.hpp"
#include "settings.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>

// Reproduce the pinned PS4 SDK's misleading st_size for cached packages.
extern "C" int stat(const char* path, struct stat* result) noexcept {
    const int rc = fstatat(AT_FDCWD, path, result, 0);
    if (!rc && std::strstr(path, "/downloads/")) result->st_size = result->st_blocks;
    return rc;
}

namespace {
std::string mode, directory, url, remoteUrl;
int remoteCalls = 0, localCalls = 0, updates = 0;
std::atomic<bool> cancelled{false};
}
namespace psgi {
uint64_t freeSpace(const std::string&) { return mode == "space" ? 1024 : 1000000000; }
int queueInstall(const Entry& e, int user) { assert(user == 123 && e.url == remoteUrl); ++remoteCalls; return 91; }
int queueLocalInstall(const Entry& e, const std::string& path, int user) {
    assert(user == 123 && e.url == url && path == stagedPackagePath(directory, url));
    std::ifstream file(path, std::ios::binary | std::ios::ate); const auto size = file.tellg();
    assert(size == 1024 * 1024 + 4); file.seekg(0);
    std::string magic(4, '\0'); file.read(&magic[0], 4);
    assert(magic == std::string("\x7f" "CNT", 4));
    char chunk[8192]; size_t read = 4;
    while (file) { file.read(chunk, sizeof(chunk)); const auto n = file.gcount();
        for (int i = 0; i < n; ++i) assert(chunk[i] == char((read + i) % 251));
        read += n;
    }
    assert(std::streamoff(read) == size); ++localCalls;
    assert(readFile(path + ".lease", 32) == "in-use\n");
    if (mode == "safe-refusal" || mode == "uncertain-refusal") throw SubmissionFailure("Native refusal fixture", mode == "safe-refusal");
    return 92;
}
}
int main(int argc, char** argv) {
    assert(argc == 4 || argc == 5); url = argv[1]; directory = argv[2]; mode = argv[3];
    remoteUrl = argc == 5 ? argv[4] : url;
    psgi::Entry entry; entry.name = "Test package"; entry.url = url;
    const auto path = psgi::stagedPackagePath(directory, url);
    try {
        const int task = psgi::installPackage(entry, 123, directory, &cancelled, [&](uint64_t bytes, uint64_t total) {
            ++updates; assert(bytes <= total);
            if (mode == "cancel" && bytes) cancelled = true;
        });
        assert(entry.url == url); // Redirects must not change the catalog/queue identity.
        assert(mode == "range" || mode == "redirect" || mode == "local" || mode == "https-range" || mode == "upgrade");
        if (mode == "range" || mode == "redirect") assert(task == 91 && remoteCalls == 1 && localCalls == 0 && updates == 0);
        else {
            assert(task == 92 && localCalls == 1 && remoteCalls == 0 && updates > 1);
            assert(psgi::installPackage(entry, 123, directory, &cancelled, [](uint64_t, uint64_t) {}) == 92);
            assert(localCalls == 2); // A valid staged file survives a registration retry.
            psgi::removeStagedPackage(directory, url); assert(!std::filesystem::exists(path) && !std::filesystem::exists(path + ".lease"));
        }
    } catch (const std::exception& e) {
        const std::string message = e.what(); std::cout << message << '\n';
        if (mode == "safe-refusal" || mode == "uncertain-refusal") {
            assert(message == "Native refusal fixture" && localCalls == 1 && remoteCalls == 0);
            assert(std::filesystem::exists(path) && psgi::readFile(path + ".lease",32) == (mode == "safe-refusal" ? "released\n" : "in-use\n"));
            psgi::removeStagedPackage(directory,url); return 0;
        }
        if (mode == "missing") assert(message.find("HTTP 404") != std::string::npos);
        else if (mode == "html") assert(message.find("PS4 PKG") != std::string::npos);
        else if (mode == "cancel") assert(message.find("cancelled") != std::string::npos);
        else if (mode == "space") assert(message.find("Not enough internal") != std::string::npos);
        else if (mode == "unknown-length") assert(message.find("Content-Length") != std::string::npos);
        else if (mode == "truncated") assert(message.find("Package download failed") != std::string::npos);
        else if (mode == "tls-error") assert(message.find("certificate") != std::string::npos);
        else throw;
        assert(remoteCalls == 0 && localCalls == 0);
        assert(!std::filesystem::exists(path) && !std::filesystem::exists(path + ".part"));
    }
}
