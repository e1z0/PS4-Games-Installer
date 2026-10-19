#pragma once
#include "catalog.hpp"
#include "archive_auth.hpp"
#include <atomic>
#include <optional>
#include <functional>
#include <stdexcept>

namespace psgi {
// Cache packaged trust before libjbc changes access to /app0. Call before workers start.
void prepareHttpTrust();
std::string httpGet(const std::string& url, size_t limit = MaxCatalogBytes, const std::atomic<bool>* cancelled = nullptr);
// Public artwork has a separate console HTTPS compatibility policy.
std::string httpGetArtwork(const std::string& url, size_t limit, const std::atomic<bool>* cancelled = nullptr);
struct HttpTransferInfo { int status = 0; uint64_t length = 0; std::string effectiveUrl; };
// Bounded-memory package transfer. Returning false from the sink stops a probe
// without downloading the remaining body. total is the response Content-Length.
using HttpSink = std::function<bool(const void*, size_t, uint64_t)>;
HttpTransferInfo httpTransfer(const std::string& url, const HttpSink& sink, bool probeRange,
                             const std::atomic<bool>* cancelled = nullptr);
HttpTransferInfo httpTransferAuthenticated(const std::string& url, const HttpSink& sink, bool probeRange,
                                          const ArchiveSession& session, const std::atomic<bool>* cancelled = nullptr);
ArchiveSession archiveLogin(const std::string& email, const std::string& password,
                            const std::atomic<bool>* cancelled = nullptr);
void prepareConsole();
// Called after application resources and worker threads have been destroyed.
// Native builds hand termination to the PS4 shell; desktop builds return status.
int finishApplication(int status);
struct SubmissionFailure : std::runtime_error {
    bool storageReleased;
    int retainedTask;
    SubmissionFailure(const std::string& message, bool released, int task = -1)
        : std::runtime_error(message), storageReleased(released), retainedTask(task) {}
};
int queueInstall(const Entry& entry, int userId);
int queueLocalInstall(const Entry& entry, const std::string& path, int userId);
// Only call for failed/completed tasks owned/tracked by this application.
bool retireInactiveTask(int taskId);
// Explicit user cancellation, only for a task ID recorded as owned by this app.
void cancelInstallTask(int taskId);
struct TaskProgress {
    uint64_t transferred = 0, total = 0;
    int error = 0;
    bool found = true;
    bool retired = false;
    bool completed = false;
};
TaskProgress taskProgress(int taskId);
std::vector<Entry> scanInstalled();
void openMediaUrl(const std::string& url);
void launchTitle(const std::string& titleId);
void uninstallTitle(const std::string& titleId);
uint64_t freeSpace(const std::string& path);
}
