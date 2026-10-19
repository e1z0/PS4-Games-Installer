#include "queue.hpp"
#include "library.hpp"
#include "settings.hpp"
#include "sfo.hpp"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <cerrno>
#include <memory>
#include <set>
#include <stdexcept>
#include <unistd.h>

namespace psgi {
namespace {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
uint64_t number(const cJSON* item, const char* name, uint64_t limit = 9007199254740991ULL) {
    const auto* value = cJSON_GetObjectItemCaseSensitive(item, name);
    if (!value) return 0;
    if (!cJSON_IsNumber(value) || !std::isfinite(value->valuedouble) || value->valuedouble < 0 ||
        value->valuedouble > double(limit) || std::floor(value->valuedouble) != value->valuedouble) throw std::runtime_error("Invalid queue number");
    return value->valuedouble;
}
std::string string(const cJSON* item, const char* name, size_t limit) {
    const auto* value = cJSON_GetObjectItemCaseSensitive(item, name);
    if (!value) return "";
    if (!cJSON_IsString(value) || std::string(value->valuestring).size() > limit) throw std::runtime_error("Invalid queue text");
    return value->valuestring;
}
void validateEntry(const Entry& e) {
    if (!isHttpUrl(e.url) || e.name.empty() || e.name.size() >= 600 || (!e.titleId.empty() && !validTitleId(e.titleId)))
        throw std::runtime_error("Invalid queued package identity");
}
}
std::string queueStageName(QueueStage stage) {
    if (stage == QueueStage::Ready) return "Waiting to install";
    const char* names[] = {"Queued", "Checking URL", "Downloading", "Registering with PS4", "Installing", "Verifying installation", "Completed", "Failed", "Cancelled", "Cancelling"};
    const auto index = static_cast<unsigned>(stage); return index < 10 ? names[index] : "Unknown";
}
bool queueActive(QueueStage stage) {
    return stage != QueueStage::Completed && stage != QueueStage::Failed && stage != QueueStage::Cancelled;
}
bool queueNeedsVerification(const QueueItem& job) {
    return job.stage == QueueStage::Verifying ||
        (job.stage == QueueStage::Failed && job.systemCompleted &&
         job.message.find("PS4 task finished but installation could not be confirmed.") == 0);
}
QueueItem* findJob(std::vector<QueueItem>& queue, const std::string& url) {
    for (auto& job : queue) if (job.entry.url == url) return &job;
    return nullptr;
}
void enqueueJob(std::vector<QueueItem>& queue, const Entry& entry, uint64_t now) {
    validateEntry(entry);
    auto* existing = findJob(queue, entry.url);
    if (existing && (queueActive(existing->stage) || existing->task >= 0)) throw std::runtime_error("This package still has an active queue/system task");
    QueueItem fresh; fresh.entry = entry; fresh.updated = now; fresh.message = "Waiting to check the package URL";
    fresh.previouslyPresent = entry.baseInstalled || entry.installed; fresh.previousVersion = entry.installedVersion;
    if (existing) *existing = std::move(fresh);
    else {
        if (queue.size() >= 200) {
            auto old = std::find_if(queue.begin(), queue.end(), [](const QueueItem& job) { return !queueActive(job.stage) && job.task < 0; });
            if (old == queue.end()) throw std::runtime_error("Queue is full (200 active packages)");
            queue.erase(old);
        }
        queue.push_back(std::move(fresh));
    }
}
void verifyJob(QueueItem& job, const std::vector<Entry>& installed, uint64_t now) {
    if (!queueNeedsVerification(job)) return;
    const bool timedOut = job.stage == QueueStage::Failed;
    if (!job.verificationStarted) job.verificationStarted = now;
    const auto found = std::find_if(installed.begin(), installed.end(), [&](const Entry& e) { return !job.entry.titleId.empty() && e.titleId == job.entry.titleId; });
    bool verified = found != installed.end();
    // A catalog base-game version can describe a bundled update absent from the PKG.
    // Require a real installed title; native completion then confirms the base package.
    if (verified && (packageKind(job.entry) == 1 || (!job.systemCompleted && compareVersions(job.entry.version, job.entry.version)))) {
        const auto version = compareVersions(found->version, job.entry.version);
        verified = version && *version >= 0;
    }
    if (verified && !job.systemCompleted && job.previouslyPresent) {
        const auto advanced = compareVersions(found->version, job.previousVersion);
        verified = advanced && *advanced > 0;
    }
    if (packageKind(job.entry) == 2 || packageKind(job.entry) == 3) {
        // Add-ons/themes do not get their own game tile; base-title discovery
        // cannot prove that a specific DLC is installed.
        verified = job.systemCompleted;
        if (verified) job.message = "PS4 reports package installation complete. Check add-ons/themes in the system; no separate game tile is expected.";
    } else if (verified) {
        job.message = "Installed title" + (found->version.empty() ? std::string() : " v" + found->version) + " confirmed by a fresh PS4 library scan.";
        const auto version = compareVersions(found->version, job.entry.version);
        if (job.systemCompleted && packageKind(job.entry) == 0 && version && *version < 0)
            job.message += " PS4 reports base package complete; catalog version " + job.entry.version + " differs from the installed version.";
    }
    if (verified) { job.stage = QueueStage::Completed; job.updated = now; return; }
    if (timedOut) return; // Keep the original failure until fresh evidence actually confirms it.
    job.message = "Transfer/task finished. Waiting for the installed title/version to appear; installation is not confirmed.";
    if (now >= job.verificationStarted && now - job.verificationStarted >= 120) {
        job.stage = QueueStage::Failed; job.updated = now;
        job.message = "PS4 task finished but installation could not be confirmed. Check Notifications > Downloads, GoldHEN and storage, then refresh/retry. A completed download alone is not installation.";
    }
}
void sampleJob(QueueItem& job, uint64_t bytes, uint64_t total, double elapsed) {
    if (bytes < job.sampleBytes) { job.sampleTime = elapsed; job.sampleBytes = bytes; job.bytesPerSecond = 0; }
    if (elapsed - job.sampleTime >= 1) {
        job.bytesPerSecond = uint64_t(double(bytes - job.sampleBytes) / (elapsed - job.sampleTime));
        job.sampleTime = elapsed; job.sampleBytes = bytes;
    }
    job.transferred = bytes; job.total = total;
}
std::string queueProgress(const QueueItem& job) {
    std::string out = queueStageName(job.stage);
    if (job.total) out += "  Data " + std::to_string(unsigned(std::min(100.0, double(job.transferred) / job.total * 100))) + "%  " + formatSize(job.transferred) + " / " + formatSize(job.total);
    if (job.bytesPerSecond && (job.stage == QueueStage::Downloading || job.stage == QueueStage::Installing)) {
        out += "  " + formatSize(job.bytesPerSecond) + "/s";
        if (job.total > job.transferred) out += "  ~" + std::to_string((job.total - job.transferred) / job.bytesPerSecond / 60 + 1) + " min";
    }
    return out;
}
std::vector<QueueItem> parseQueue(const std::string& text) {
    if (text.size() > MaxCatalogBytes || text.find('\0') != std::string::npos) throw std::runtime_error("Invalid queue data");
    Json root(cJSON_ParseWithLengthOpts(text.c_str(), text.size() + 1, nullptr, true), cJSON_Delete);
    if (!cJSON_IsObject(root.get()) || number(root.get(), "schema", 1) != 1) throw std::runtime_error("Unsupported queue schema");
    const auto* jobs = cJSON_GetObjectItemCaseSensitive(root.get(), "jobs");
    if (!cJSON_IsArray(jobs)) throw std::runtime_error("Queue jobs are missing");
    std::vector<QueueItem> queue; std::set<std::string> urls; std::set<int> taskIds;
    for (const auto* item = jobs->child; item; item = item->next) {
        if (!cJSON_IsObject(item) || queue.size() >= 200) throw std::runtime_error("Invalid/oversized queue");
        QueueItem job;
        job.entry.url = string(item, "url", 2047); job.entry.name = string(item, "name", 599);
        job.entry.titleId = string(item, "title_id", 16); job.entry.category = string(item, "category", 32); job.entry.packageType = string(item, "package_type", 32);
        job.entry.version = string(item, "version", 64); job.entry.contentId = string(item, "content_id", 128);
        job.entry.minFirmware = string(item, "min_fw", 64);
        const auto required = string(item, "required_base_version", 64);
        if (!required.empty()) job.entry.metadata.emplace_back("required_base_version", required);
        job.dependsOn = string(item, "depends_on", 2047);
        if (!job.dependsOn.empty() && (!isHttpUrl(job.dependsOn) || job.dependsOn == job.entry.url)) throw std::runtime_error("Invalid queue dependency");
        job.entry.coverUrl = string(item, "cover_url", 2047); job.entry.size = number(item, "size"); validateEntry(job.entry);
        if (!urls.insert(job.entry.url).second) throw std::runtime_error("Duplicate queue URL");
        if (cJSON_GetObjectItemCaseSensitive(item, "task")) {
            job.task = number(item, "task", 2147483647);
            if (!taskIds.insert(job.task).second) throw std::runtime_error("Duplicate queue task");
        }
        const auto stage = number(item, "stage", 11);
        // r30 wrote a temporary unconfirmed state; resume the original verifier.
        job.stage = stage == 10 ? QueueStage::Verifying : static_cast<QueueStage>(stage);
        job.transferred = number(item, "transferred"); job.total = number(item, "total"); job.updated = number(item, "updated");
        job.verificationStarted = number(item, "verification_started");
        const auto* completed = cJSON_GetObjectItemCaseSensitive(item, "system_completed");
        if (completed && !cJSON_IsBool(completed)) throw std::runtime_error("Invalid queue completion flag");
        job.systemCompleted = cJSON_IsTrue(completed); job.message = string(item, "message", 4096);
        const auto* present = cJSON_GetObjectItemCaseSensitive(item, "previously_present");
        if (present && !cJSON_IsBool(present)) throw std::runtime_error("Invalid queue previous-presence flag");
        job.previouslyPresent = cJSON_IsTrue(present); job.previousVersion = string(item, "previous_version", 64);
        const auto* background = cJSON_GetObjectItemCaseSensitive(item, "background_transfer");
        if (background && !cJSON_IsBool(background)) throw std::runtime_error("Invalid queue transfer mode");
        job.backgroundTransfer = cJSON_IsTrue(background);
        queue.push_back(std::move(job));
    }
    return queue;
}
std::string serializeQueue(const std::vector<QueueItem>& queue) {
    Json root(cJSON_CreateObject(), cJSON_Delete); cJSON_AddNumberToObject(root.get(), "schema", 1);
    auto* jobs = cJSON_AddArrayToObject(root.get(), "jobs");
    for (const auto& job : queue) {
        auto* item = cJSON_CreateObject(); cJSON_AddItemToArray(jobs, item);
        for (const auto& field : std::vector<std::pair<const char*, std::string>>{{"url",job.entry.url},{"name",job.entry.name},{"title_id",job.entry.titleId},
            {"depends_on",job.dependsOn},{"min_fw",job.entry.minFirmware},{"required_base_version",requiredBaseVersion(job.entry)},{"category",job.entry.category},{"package_type",job.entry.packageType},{"version",job.entry.version},{"content_id",job.entry.contentId},{"cover_url",job.entry.coverUrl},{"message",job.message}})
            cJSON_AddStringToObject(item, field.first, field.second.c_str());
        cJSON_AddNumberToObject(item, "size", job.entry.size); cJSON_AddNumberToObject(item, "stage", static_cast<unsigned>(job.stage));
        if (job.task >= 0) cJSON_AddNumberToObject(item, "task", job.task);
        cJSON_AddNumberToObject(item, "transferred", job.transferred); cJSON_AddNumberToObject(item, "total", job.total);
        cJSON_AddNumberToObject(item, "updated", job.updated); cJSON_AddNumberToObject(item, "verification_started", job.verificationStarted);
        cJSON_AddBoolToObject(item, "system_completed", job.systemCompleted);
        cJSON_AddBoolToObject(item, "previously_present", job.previouslyPresent);
        cJSON_AddStringToObject(item, "previous_version", job.previousVersion.c_str());
        cJSON_AddBoolToObject(item, "background_transfer", job.backgroundTransfer);
    }
    std::unique_ptr<char, decltype(&cJSON_free)> text(printJson(root.get()), cJSON_free);
    if (!text) throw std::runtime_error("Cannot serialize queue");
    return text.get();
}
std::vector<QueueItem> loadQueue(const std::string& directory) {
    const auto path = directory + "/queue.json";
    if (access(path.c_str(), F_OK)) { if (errno == ENOENT) return {}; throw std::runtime_error("Cannot read queue file"); }
    auto queue = parseQueue(readFile(path));
    for (auto& job : queue) {
        if (job.task < 0 && (job.stage == QueueStage::Checking || job.stage == QueueStage::Downloading || job.stage == QueueStage::Registering || job.stage == QueueStage::Cancelling)) {
            job.stage = QueueStage::Failed; job.message = "Interrupted when the app closed. Retry to reuse saved package data.";
        } else if (job.task >= 0 && job.stage == QueueStage::Cancelling) {
            job.stage = QueueStage::Installing; job.message = "Cancellation was interrupted. Check status or cancel again.";
        }
    }
    return queue;
}
void saveQueue(const std::string& directory, const std::vector<QueueItem>& queue) {
    const auto text = serializeQueue(queue); parseQueue(text); atomicWrite(directory + "/queue.json", text);
}
}
