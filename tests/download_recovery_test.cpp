#include "download.hpp"
#include "settings.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <unistd.h>

namespace {
std::string bytes = std::string("\x7f" "CNT", 4) + std::string(128 * 1024, 'a');
uint64_t length = bytes.size();
int fullRequests = 0, failures = 0, registrations = 0;
bool mutate = false, tlsFailure = false;
std::atomic<bool> cancelled{false};
}
namespace psgi {
uint64_t freeSpace(const std::string&) { return 200000000000ULL; }
HttpTransferInfo httpTransferAuthenticated(const std::string& url, const HttpSink& sink, bool range,
                                          const ArchiveSession&, const std::atomic<bool>* stop) {
    assert(!range); // Queued downloads stage even when a server supports Range.
    if (!sink(bytes.data(), 4, length)) return {200, length, url};
    ++fullRequests;
    if (tlsFailure) throw std::runtime_error("Untrusted certificate fixture");
    const auto response = mutate && fullRequests > 1 ? bytes.substr(0, 8) + "b" + bytes.substr(9) : bytes;
    for (size_t offset = 4; offset < response.size();) {
        if (stop && *stop) throw std::runtime_error("Download cancelled");
        if (fullRequests <= failures && offset >= 32768) throw HttpTransferFailure("Read HTTP response (0x80431068)");
        const size_t n = std::min(size_t(8192), response.size() - offset);
        if (!sink(response.data() + offset, n, length)) break;
        offset += n;
    }
    return {200, length, url};
}
int queueInstall(const Entry&, int) { assert(false && "Serial queue must never register a remote download"); return -1; }
int queueLocalInstall(const Entry&, const std::string& path, int user) {
    assert(user == 123 && readFile(path + ".lease", 32) == "in-use\n");
    FILE* file = std::fopen(path.c_str(), "rb"); assert(file);
    assert(!fseeko(file, 0, SEEK_END) && uint64_t(ftello(file)) == length); std::fclose(file);
    ++registrations; return 1234;
}
}
int main() {
    using namespace psgi;
    char temporary[] = "/tmp/psgi-recovery-XXXXXX"; assert(mkdtemp(temporary));
    const std::string root = temporary;
    Entry entry; entry.url = "http://fixture/ark.pkg"; entry.name = "Large package";
    const auto path = stagedPackagePath(root, entry.url);
    const auto progress = [](uint64_t done, uint64_t total) { assert(done <= total && total == length); };
    failures = 1;
    assert(downloadPackage(entry, root, &cancelled, progress) == bytes.size());
    assert(fullRequests == 2 && registrations == 0 && readFile(path) == bytes);
    assert(installDownloadedPackage(entry, length, 123, root) == 1234 && registrations == 1);
    removeStagedPackage(root, entry.url);

    // Exhausted retries retain exactly the partial bytes written; another app
    // session rechecks that prefix and completes without trusting append offsets.
    fullRequests = 0; failures = 3;
    bool rejected = false;
    try { downloadPackage(entry, root, &cancelled, progress); }
    catch (const HttpTransferFailure&) { rejected = true; }
    assert(rejected && fullRequests == 3 && !std::filesystem::exists(path));
    assert(readFile(path + ".part") == bytes.substr(0, 32772));
    failures = 0;
    assert(downloadPackage(entry, root, &cancelled, progress) == bytes.size() && readFile(path) == bytes);
    removeStagedPackage(root, entry.url);

    // A server silently replacing a same-length package must not mix old/new data.
    fullRequests = 0; failures = 1; mutate = true; rejected = false;
    try { downloadPackage(entry, root, &cancelled, progress); }
    catch (const std::runtime_error& e) { rejected = std::string(e.what()).find("source changed") != std::string::npos; }
    assert(rejected && fullRequests == 2 && !std::filesystem::exists(path + ".part"));
    mutate = false; failures = 0;

    // Cancellation keeps useful data, and retry consumes it safely.
    fullRequests = 0; rejected = false;
    try { downloadPackage(entry, root, &cancelled, [](uint64_t done, uint64_t) { if (done > 8000) cancelled = true; }); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected && std::filesystem::file_size(path + ".part") > 8000);
    cancelled = false;
    assert(downloadPackage(entry, root, &cancelled, progress) == bytes.size() && readFile(path) == bytes);
    removeStagedPackage(root, entry.url);

    fullRequests = 0; tlsFailure = true; rejected = false;
    try { downloadPackage(entry, root, &cancelled, progress); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected && fullRequests == 1); tlsFailure = false;
    std::filesystem::remove(path + ".part");

    // Reproduce ARK's exact advertised size without allocating/downloading 99 GB.
    length = 99383443456ULL;
    FILE* sparse = std::fopen(path.c_str(), "wb"); assert(sparse);
    assert(std::fwrite(bytes.data(), 1, 4, sparse) == 4);
    assert(!ftruncate(fileno(sparse), length)); assert(!std::fclose(sparse));
    fullRequests = 0;
    assert(downloadPackage(entry, root, &cancelled, progress) == length && fullRequests == 0);
    assert(installDownloadedPackage(entry, length, 123, root) == 1234);
    std::filesystem::remove_all(root);
    std::puts("Interrupted download recovery, prefix integrity, cancellation, retry bounds and 99 GB staging passed");
}
