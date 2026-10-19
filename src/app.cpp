#include "app.hpp"
#include "video.hpp"
#include "platform.hpp"
#include "download.hpp"
#include "json.hpp"
#include "revision.hpp"
#include <chrono>
#include <ctime>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>

namespace psgi {
namespace {
enum class ResultKind { Local, Remote, Test, Storage, Install, Uninstall, Poll, Cancel, SourceTest, Maintenance, ArchiveLogin };
struct Result {
    ResultKind kind = ResultKind::Test;
    std::optional<std::vector<Entry>> local, remote;
    Entry entry;
    int task = -1;
    uint64_t freeBytes = 0;
    std::optional<InstallStorage> storage;
    std::string message, catalogKey;
    std::map<std::string, TaskProgress> progress;
    std::optional<SourceReport> sourceReport;
    bool backgroundTransfer = false;
    std::optional<StorageUsage> usage;
    std::optional<ArchiveSession> archiveSession;
};
struct TransferUpdate { std::string url; QueueStage stage; uint64_t bytes = 0, total = 0; };
constexpr size_t CacheLimit = 64 * 1024 * 1024;
std::string ensureDirectory(const std::string& path) {
    if (mkdir(path.c_str(), 0700) && errno != EEXIST) throw std::runtime_error("Cannot create application data directory");
    return path;
}
std::string errorCode(int result) {
    char code[20]; std::snprintf(code, sizeof(code), "0x%08X", static_cast<unsigned>(result)); return code;
}
uint64_t timestamp() { const auto now = std::time(nullptr); return now > 0 ? uint64_t(now) : 0; }
InstallRequirements queueRequirements(const Entry& entry, const Ui& ui) {
    std::vector<Entry> bases;
    for (const auto& job : ui.downloads) if (queueActive(job.stage) && packageKind(job.entry) == 0) bases.push_back(job.entry);
    const auto required = installationRequirements(entry, ui.installed, bases);
    if (!required.blockers.empty()) throw std::runtime_error(required.blockers.front());
    return required;
}
void requireBackupDevice(const std::string& path) {
    const auto devices = scanBackupDevices();
    if (std::none_of(devices.begin(), devices.end(), [&](const BackupDevice& device) { return device.path == path; }))
        throw std::runtime_error("USB drive is disconnected or read-only. Reconnect it and refresh the drive list.");
}
}
struct App::Worker {
    TaskThread thread;
    std::mutex mutex;
    bool running = false;
    std::optional<Result> result;
    std::string update;
    std::optional<TransferUpdate> transfer;
    std::atomic<bool> cancelled{false};
    ~Worker() { cancelled = true; if (thread.joinable()) thread.join(); }
    bool busy() { std::lock_guard<std::mutex> lock(mutex); return running || result.has_value(); }
    bool start(std::function<Result()> job) {
        if (busy()) return false;
        if (thread.joinable()) thread.join();
        { std::lock_guard<std::mutex> lock(mutex); running = true; update.clear(); transfer.reset(); cancelled = false; }
        try { thread = TaskThread([this, job] {
            Result out;
            try { out = job(); } catch (const std::exception& e) { out.message = e.what(); }
            std::lock_guard<std::mutex> lock(mutex);
            result = std::move(out); running = false;
        }); } catch (...) { std::lock_guard<std::mutex> lock(mutex); running = false; throw; }
        return true;
    }
    std::optional<Result> take() {
        std::lock_guard<std::mutex> lock(mutex);
        auto out = std::move(result); result.reset(); return out;
    }
    void report(const std::string& message) { std::lock_guard<std::mutex> lock(mutex); update = message; }
    std::string takeUpdate() { std::lock_guard<std::mutex> lock(mutex); std::string message; message.swap(update); return message; }
    void reportTransfer(TransferUpdate value) { std::lock_guard<std::mutex> lock(mutex); transfer = std::move(value); }
    std::optional<TransferUpdate> takeTransfer() { std::lock_guard<std::mutex> lock(mutex); auto value = std::move(transfer); transfer.reset(); return value; }
};

App::App(const std::string& data, bool preview)
    : worker(new Worker), monitor(new Worker), directory(ensureDirectory(data)), demo(preview), artwork(directory + "/artwork") {
    try { ui.settings = loadSettings(directory); }
    catch (const std::exception& e) { ui.status = std::string("Settings: ") + e.what(); }
    if(!demo)log(std::string("Starting PS4 Games Installer ")+PSGI_VERSION);
    saved = ui.settings; ui.draft = saved;
    if (!demo) {
        try { archiveSession = loadArchiveSession(directory); ui.archiveSignedIn = !archiveSession.empty(); }
        catch (const std::exception& e) { ui.archiveNotice = e.what(); }
    }
    try { ui.player = loadPlayerData(directory); }
    catch (const std::exception& e) { playerWritable = false; log(std::string("Local records kept unchanged: ") + e.what()); }
    try { resume = loadViewState(directory); ui.recentSearches=resume->recentSearches; }
    catch (const std::exception& e) { log(std::string("Saved position: ") + e.what()); }
    if (demo) {
        auto entries = demoLibrary();
        std::vector<Entry> local;
        for (size_t i = 0; i < std::min(size_t(6), entries.size()); ++i) {
            auto e = entries[i]; e.installed = true; e.installedSize = e.size + 1500000000ULL; e.url.clear(); local.push_back(e);
        }
        ui.player.observeInstalled(local, 1792108800);
        if (!local.empty()) ui.player.rate(local[0].titleId, 5);
        if (entries.size() > 8) { ui.player.observeInstalled({entries[8]}, 1792108800); ui.player.rate(entries[8].titleId, 4); }
        if (!local.empty()) ui.player.setInfo(local[0].titleId, {"Action", "Fast combat and great music. Come back for the remaining secrets.", {"Weekend", "Single player"}});
        if (entries.size() > 8) ui.player.setInfo(entries[8].titleId, {"Adventure", "A peaceful underwater journey. Perfect for a quiet evening.", {"Weekend", "Short games"}});
        SavedCollection favorites; favorites.name = "Favorites for the weekend"; favorites.available = true; favorites.filter.minRating = 4; favorites.filter.tag = "Weekend";
        ui.player.saveCollection(favorites);
        if (!entries.empty()) {
            Entry update = entries[0]; update.category = "updates"; update.packageType = "update"; update.version = "1.12"; update.name += " update"; update.url += "?update=1"; update.size = 4200000000ULL; entries.push_back(update);
            Entry dlc = entries[0]; dlc.category = "dlc"; dlc.packageType = "dlc"; dlc.name += " expansion"; dlc.url += "?dlc=1"; dlc.size = 1600000000ULL; entries.push_back(dlc);
            const QueueStage stages[] = {QueueStage::Completed, QueueStage::Downloading, QueueStage::Installing, QueueStage::Failed, QueueStage::Waiting, QueueStage::Verifying};
            for (size_t i = 0; i < std::min(size_t(6), entries.size()); ++i) {
                QueueItem job; job.entry = entries[i]; job.stage = stages[i]; job.total = i == 4 ? 0 : entries[i].size; job.transferred = job.total * (i == 1 ? 43 : 100) / 100;
                job.message = i == 3 ? "PS4 refused registration (sample error 0x80990086). Check PS4 Downloads and storage, then retry. Downloaded package kept." : i == 5 ? "Transfer finished; installed title/version has not yet been confirmed by a library scan." : "Desktop sample progress; no package is being transferred.";
                job.bytesPerSecond = i == 1 ? 38000000 : 0; job.task = i == 2 ? 45 : -1; ui.downloads.push_back(job);
            }
        }
        ui.usage = {240000000, 36000000000ULL, 12000000000ULL, 24000000000ULL, 2, 1, true};
        ui.backupDevices = {{"/mnt/usb0", {"PS4-Games-Installer-player-1792152000-0.json"}}};
        ui.setLibraries(local, entries);
        ui.freeBytes = 423000000000ULL;
        ui.storage.internal = {true, ui.freeBytes}; ui.storage.external = {true, 1200000000000ULL};
        ui.storage.destination = InstallDestination::Internal;
        ui.storageChecked = true;
        ui.status = "Desktop preview — sample library; PS4 operations are disabled";
        ui.loading = false; bootReady = true; initialScan = false;
        return;
    }
    log("Loading saved catalog");
    try { ui.available = parseCatalog(readFile(catalogCachePath(directory,ui.settings),CacheLimit),"",CacheLimit);
        log("Saved catalog ready: "+std::to_string(ui.available.size())+" packages"); }
    catch (const std::exception&) {
        // Adopt the old single cache only for an existing unprofiled setup.
        if (ui.settings.profiles.empty() && access((directory+"/catalog-cache-v1").c_str(),F_OK) && errno==ENOENT) try {
            const auto legacy=readFile(directory+"/catalog.json",CacheLimit); ui.available=parseCatalog(legacy,"",CacheLimit);
            atomicWrite(catalogCachePath(directory,ui.settings),legacy);
        } catch (const std::exception&) {}
    }
    try { atomicWrite(directory+"/catalog-cache-v1","1\n"); } catch (const std::exception& e) { log(std::string("Catalog cache migration marker: ")+e.what()); }
    bool validQueueFile = false;
    try { ui.downloads = loadQueue(directory); validQueueFile = !access((directory + "/queue.json").c_str(), F_OK); }
    catch (const std::exception& e) { queueWritable = false; log(std::string("Queue file kept unchanged: ") + e.what()); }
    for (const auto& job : ui.downloads) if (job.task >= 0) tasks[job.entry.url] = job.task;
    try {
        auto text = readFile(directory + "/tasks.json");
        std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(cJSON_ParseWithLengthOpts(text.c_str(), text.size()+1, nullptr, true), cJSON_Delete);
        if (cJSON_IsObject(root.get())) for (auto* item = root->child; item; item = item->next)
            if (isHttpUrl(item->string) && cJSON_IsNumber(item) && item->valuedouble >= 0 && item->valuedouble <= 2147483647 && item->valuedouble == item->valueint) {
                if (tasks.count(item->string)) continue;
                auto* job = findJob(ui.downloads, item->string);
                // queue.json is authoritative after migration. A stale compatibility
                // copy must not resurrect a completed/cancelled/cleared task. Recover
                // a registration ID only if its queue write was interrupted.
                if (validQueueFile && (!job || job->stage != QueueStage::Failed || job->message.find("Interrupted") != 0)) continue;
                tasks[item->string] = item->valueint;
                if (job) { job->task = item->valueint; job->stage = QueueStage::Installing; job->message = "Recovered a task ID after interrupted queue persistence"; }
            }
    } catch (const std::exception&) {}
    for (const auto& item : tasks) if (!findJob(ui.downloads, item.first)) {
        Entry entry; entry.url = item.first; entry.name = "Tracked PS4 package";
        for (const auto& cached : ui.available) if (cached.url == item.first) { entry = cached; break; }
        QueueItem job; job.entry = entry; job.task = item.second; job.stage = QueueStage::Installing;
        job.message = "Recovered an owned task from the previous release"; ui.downloads.push_back(std::move(job));
    }
    syncQueued();
    log("Preparing cached library views");
    ui.rebuild();
    log("Startup library views ready");
    startRemote = saved.refreshOnStart;
    refresh(false);
}
App::~App() { worker.reset(); monitor.reset(); }
void App::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::string path = directory + "/app.log";
    struct stat st{};
    bool truncate = stat(path.c_str(), &st) == 0 && st.st_size > 256 * 1024;
    FILE* file = std::fopen(path.c_str(), truncate ? "wb" : "ab");
    if (file) { std::fprintf(file, "%s\n", message.c_str()); std::fclose(file); }
}
void App::savePosition() { if (!demo) saveViewState(directory, ui.captureView()); }
void App::savePlayer(const PlayerData& next) {
    if (!playerWritable) throw std::runtime_error("Local player.json needs repair; the original file was kept unchanged");
    if (!demo) savePlayerData(directory, next);
    ui.player = next; ui.rebuild();
}
void App::saveTasks() {
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(cJSON_CreateObject(), cJSON_Delete);
    for (const auto& item : tasks) cJSON_AddNumberToObject(root.get(), item.first.c_str(), item.second);
    std::unique_ptr<char, decltype(&cJSON_free)> text(printJson(root.get()), cJSON_free);
    if (text) atomicWrite(directory + "/tasks.json", text.get());
}
void App::persistQueue() {
    if (!queueWritable) throw std::runtime_error("queue.json needs repair; original file was kept unchanged");
    if (!demo) saveQueue(directory, ui.downloads);
}
void App::syncQueued() {
    ui.queued.clear();
    for (const auto& job : ui.downloads) if (queueActive(job.stage) || tasks.count(job.entry.url)) ui.queued.insert(job.entry.url);
    for (const auto& task : tasks) ui.queued.insert(task.first);
    ui.rebuild();
}
std::set<std::string> App::protectedStaging() const {
    std::set<std::string> paths;
    for (const auto& job : ui.downloads) if (queueActive(job.stage) || job.task >= 0) paths.insert(stagedPackagePath(directory, job.entry.url));
    for (const auto& task : tasks) paths.insert(stagedPackagePath(directory, task.first));
    if (!activeDownloadUrl.empty()) paths.insert(stagedPackagePath(directory, activeDownloadUrl));
    return paths;
}
void App::startNextDownload(int user) {
    if (worker->busy() || !queueWritable) return;
    auto found = ui.downloads.end(); bool changed = false;
    for (auto it = ui.downloads.begin(); it != ui.downloads.end(); ++it) if (it->stage == QueueStage::Waiting) {
        if (!it->dependsOn.empty()) {
            const auto* base = findJob(ui.downloads, it->dependsOn);
            if (base && (base->stage == QueueStage::Failed || base->stage == QueueStage::Cancelled)) {
                it->stage = QueueStage::Failed; it->message = "Base-game installation failed or was cancelled. Install the base game, then retry this package."; changed = true; continue;
            }
            if (base && base->stage != QueueStage::Completed) {
                it->message = "Waiting for the base game to finish installation and verification"; continue;
            }
        }
        const auto required = installationRequirements(it->entry, ui.installed);
        if (!required.blockers.empty()) { it->stage = QueueStage::Failed; it->message = required.blockers.front(); changed = true; continue; }
        found = it; break;
    }
    if (changed) { persistQueue(); syncQueued(); }
    if (found == ui.downloads.end()) return;
    const auto entry = found->entry;
    found->stage = QueueStage::Checking; found->message = "Checking package URL and HTTP Range support"; found->updated = timestamp();
    try { persistQueue(); }
    catch (const std::exception& e) { found->stage = QueueStage::Failed; found->message = e.what(); ui.status = e.what(); syncQueued(); return; }
    activeDownloadUrl = entry.url;
    const auto session = archiveSession;
    worker->start([this, entry, user, session] {
        Result r; r.kind = ResultKind::Install; r.entry = entry;
        QueueStage stage = QueueStage::Checking; uint64_t bytes = 0, total = 0;
        try {
            if (packageKind(entry) == 1 || packageKind(entry) == 2) {
                const auto required = installationRequirements(entry, scanInstalled());
                if (!required.blockers.empty()) throw std::runtime_error(required.blockers.front());
            }
            r.task = installPackage(entry, user, directory, &worker->cancelled, [&](uint64_t done, uint64_t length) {
            bytes = done; total = length; worker->reportTransfer({entry.url, stage, done, length});
        }, [this](const std::string& message) { log(message); worker->report(message); }, [&](InstallPhase phase) {
            r.backgroundTransfer = phase == InstallPhase::RegisteringRemote;
            stage = phase == InstallPhase::Checking ? QueueStage::Checking : phase == InstallPhase::Downloading ? QueueStage::Downloading : QueueStage::Registering;
            worker->reportTransfer({entry.url, stage, bytes, total});
        }, session); } catch (const SubmissionFailure& e) { r.message = e.what(); r.task = e.retainedTask; }
        catch (const std::exception& e) { r.message = e.what(); }
        return r;
    });
    ui.status = "Checking package URL and server range support..."; ui.loading = true;
}
void App::refresh(bool remote) {
    if (worker->busy()) { ui.status = "A background operation is running. Please try again shortly."; return; }
    if (demo) { ui.status = "Desktop preview library refreshed"; return; }
    Settings settings = ui.settings;
    bool started = worker->start([this, remote, settings] {
        Result result; result.catalogKey=catalogCachePath("",settings);
        result.kind = remote ? ResultKind::Remote : ResultKind::Local;
        if (!remote) {
            result.local = scanInstalled(); result.freeBytes = freeSpace("/user");
            result.storage = scanInstallStorage();
            result.message = std::to_string(result.local->size()) + " installed games found";
            return result;
        }
        std::vector<Entry> combined;
        size_t count = 0;
        for (const auto& source : settings.sources) if (!source.url.empty()) {
            if (worker->cancelled) throw std::runtime_error("Refresh cancelled");
            try {
                log("Fetching remote catalog: "+source.category);
                const auto document=httpGet(source.url,MaxCatalogBytes,&worker->cancelled);
                log("Parsing remote catalog: "+std::to_string(document.size())+" bytes");
                mergeCatalog(combined,parseCatalog(document,source.category));
            }
            catch (const std::exception& e) { throw std::runtime_error(source.category + ": " + e.what() + ". Keeping the last valid library."); }
            ++count;
        }
        if (!count) throw std::runtime_error("Open Options > Settings and enter your Games JSON URL.");
        result.message = std::to_string(combined.size()) + " remote packages loaded";
        log("Serializing remote catalog cache");
        std::string cached = serializeCatalog(combined);
        log("Remote catalog serialized: "+std::to_string(cached.size())+" bytes");
        if (cached.size() <= CacheLimit) {
            try { atomicWrite(catalogCachePath(directory,settings), cached); }
            catch (const std::exception&) { result.message += " (offline cache could not be saved)"; }
        } else result.message += " (library exceeds offline cache limit)";
        result.remote = std::move(combined);
        return result;
    });
    if (started) { ui.loading = true; ui.status = remote ? "Refreshing remote library..." : "Reading installed games..."; }
}
void App::tick(double elapsed, Video& video) {
    for(const auto& message:artwork.takeMessages())if(!demo)log(message);
    if (auto update = worker->takeTransfer()) if (auto* job = findJob(ui.downloads, update->url)) {
        if (job->stage != QueueStage::Cancelling) {
            const bool changed = job->stage != update->stage; job->stage = update->stage;
            sampleJob(*job, update->bytes, update->total, elapsed);
            job->message = job->stage == QueueStage::Downloading ? "Keep the app open. Installation starts after this package download." : queueStageName(job->stage);
            ui.status = queueProgress(*job);
            if (changed) { try { persistQueue(); } catch (const std::exception& e) { log(e.what()); } }
        }
    }
    if (auto message = worker->takeUpdate(); !message.empty()) { ui.status = message; log(message); }
    auto result = worker->take();
    if (!result) result = monitor->take();
    if (result) {
        ui.loading = worker->busy();
        if (result->remote && result->catalogKey!=catalogCachePath("",ui.settings)) {
            result->remote.reset(); result->message="Catalog settings changed; refreshing the selected server.";
        }
        if (result->remote) {
            try { const auto cached=serializeCatalog(*result->remote); if (cached.size()<=CacheLimit) atomicWrite(directory+"/catalog.json",cached); }
            catch (const std::exception&) {} // Compatibility cache is optional; keyed cache is authoritative.
        }
        ui.status = result->message;
        if (initialScan) { initialScan = false; bootReady = true; }
        if (result->local) { lastLocalScan = elapsed; ui.freeBytes = result->freeBytes; }
        if (result->storage) { ui.storage = *result->storage; ui.storageChecked = true; }
        if (result->usage) ui.usage = *result->usage;
        if (result->kind == ResultKind::Maintenance) { cleaningStaging = false; ui.featureNotice = result->message; }
        if (result->kind == ResultKind::ArchiveLogin) {
            ui.archiveBusy = false;
            if (result->archiveSession) archiveSession = std::move(*result->archiveSession);
            ui.archiveSignedIn = !archiveSession.empty();
            ui.archiveNotice = result->message;
        }
        if (result->sourceReport) {
            ui.sourceTesting = false;
            if (result->sourceReport->source.url == ui.setupSource.url && result->sourceReport->source.category == ui.setupSource.category) ui.sourceReport = *result->sourceReport;
            ui.featureNotice = ui.sourceReport.message;
        }
        if (result->local && playerWritable) {
            auto next = ui.player;
            if (next.observeInstalled(*result->local, timestamp())) {
                try { savePlayer(next); }
                catch (const std::exception& e) { log(std::string("Save installation history: ") + e.what()); ui.status += " (history could not be saved)"; }
            }
        }
        const bool receivedLocal = result->local.has_value();
        if (result->local || result->remote) ui.setLibraries(std::move(result->local), std::move(result->remote));
        if (resume && (resume->available ? !ui.available.empty() : receivedLocal)) { ui.restoreView(*resume); resume.reset(); }
        if (result->kind == ResultKind::Install) {
            activeDownloadUrl.clear();
            if (auto* job = findJob(ui.downloads, result->entry.url)) {
                job->updated = timestamp();
                if (result->task >= 0) {
                    reportedTaskErrors.erase(result->task);
                    tasks[result->entry.url] = result->task; job->task = result->task;
                    job->backgroundTransfer = result->backgroundTransfer;
                    job->stage = pendingCancels.count(result->entry.url) ? QueueStage::Cancelling : result->message.empty() ? QueueStage::Installing : QueueStage::Failed;
                    job->message = "Registered with PS4. Waiting for installation, then a fresh library scan.";
                    ui.status = "Installation queued. Manage it here or in PS4 Notifications > Downloads.";
                    if (!result->message.empty()) { job->message = ui.status = result->message; ui.installError = result->message; ui.failedInstall = result->entry; }
                    try { saveTasks(); } catch (const std::exception&) { ui.status += " Legacy task tracking could not be saved."; }
                    log("BGFT installation task " + std::to_string(result->task) + (result->message.empty() ? " started for " : " retained after failed start for ") + result->entry.titleId);
                } else if (pendingCancels.erase(result->entry.url)) {
                    job->stage = QueueStage::Cancelled; job->message = "Download cancelled by you. Any complete staged package was kept for retry.";
                    ui.status = job->message;
                } else {
                    job->stage = QueueStage::Failed; job->message = result->message;
                    ui.installError = result->message; ui.failedInstall = result->entry;
                }
                try { persistQueue(); } catch (const std::exception& e) { ui.status += " Queue save: " + std::string(e.what()); }
            }
            syncQueued();
        }
        if (result->kind == ResultKind::Cancel) {
            auto* job = findJob(ui.downloads, result->entry.url);
            if (job) {
                pendingCancels.erase(result->entry.url);
                if (result->message.empty()) {
                    tasks.erase(result->entry.url); job->task = -1; job->stage = QueueStage::Cancelled;
                    job->message = "Owned task cancelled and released by PS4. You can retry this package.";
                    removeStagedPackage(directory, result->entry.url); ui.status = job->message;
                } else {
                    job->stage = QueueStage::Installing; job->message = result->message;
                    ui.status = result->message; // Retain ownership and staging on any service failure.
                }
                ui.featureNotice = ui.status;
                try { persistQueue(); saveTasks(); } catch (const std::exception& e) { log(e.what()); }
            }
            syncQueued();
        }
        if (result->kind == ResultKind::Poll) {
            bool changed = false;
            for (const auto& item : result->progress) {
                if (!tasks.count(item.first)) continue; // A newer cancellation result may already have released it.
                auto* job = findJob(ui.downloads, item.first);
                if (!job) continue;
                const auto previousStage = job->stage;
                const auto& progress = item.second;
                sampleJob(*job, progress.transferred, progress.total, elapsed);
                if (progress.error) {
                    job->stage = QueueStage::Failed;
                    job->message = "Install error " + errorCode(progress.error) +
                        (progress.retired ? "; failed task removed, installation can be retried." : "; cancel the failed task in PS4 Downloads before retrying.");
                    ui.status = job->message;
                    const auto reported = reportedTaskErrors.find(tasks.at(item.first));
                    if (reported == reportedTaskErrors.end() || reported->second != progress.error) {
                        ui.installError = job->message; ui.failedInstall = job->entry;
                    }
                    reportedTaskErrors[tasks.at(item.first)] = progress.error;
                    changed = true;
                } else if (progress.completed || !progress.found) {
                    if (job->stage != QueueStage::Completed && job->stage != QueueStage::Verifying) {
                        job->stage = QueueStage::Verifying; job->verificationStarted = timestamp(); changed = true;
                    }
                    job->systemCompleted = job->systemCompleted || progress.completed;
                } else if (job->stage != QueueStage::Cancelling && job->stage != QueueStage::Failed && job->stage != QueueStage::Completed &&
                           !(job->stage == QueueStage::Verifying && job->systemCompleted)) {
                    const bool downloading = job->backgroundTransfer && (!progress.total || progress.transferred < progress.total);
                    job->stage = downloading ? QueueStage::Downloading : QueueStage::Installing;
                    job->message = downloading ? "PS4 background download is running; installation follows. This transfer continues if you close the app." :
                        progress.total && progress.transferred >= progress.total ? "Transfer complete; PS4 installation is still running." : "PS4 is preparing or installing the package.";
                    ui.status = job->message;
                }
                changed = changed || previousStage != job->stage;
                if (!progress.found || progress.retired) {
                    pendingCancels.erase(item.first); // A poll may release the task before the cancel worker runs.
                    removeStagedPackage(directory, item.first);
                    reportedTaskErrors.erase(tasks.at(item.first));
                    tasks.erase(item.first); job->task = -1; changed = true;
                }
            }
            // Only a fresh installed scan may confirm base-game or update presence.
            if (receivedLocal) for (auto& job : ui.downloads) if (queueNeedsVerification(job)) {
                const auto before = job.stage; verifyJob(job, ui.installed, timestamp());
                changed = changed || before != job.stage;
                ui.status = job.message;
            }
            if (changed) { syncQueued(); try { saveTasks(); persistQueue(); } catch (const std::exception& e) { log(e.what()); } }
        } else if (receivedLocal) {
            bool changed = false;
            for (auto& job : ui.downloads) if (queueNeedsVerification(job)) {
                const auto before = job.stage; verifyJob(job, ui.installed, timestamp()); changed = changed || before != job.stage;
                if (before != job.stage) ui.status = job.message;
            }
            if (changed) { syncQueued(); try { persistQueue(); } catch (const std::exception& e) { log(e.what()); } }
        }
        if (!ui.status.empty()) log(ui.status);
        if (startRemote && !worker->busy()) { startRemote = false; refresh(true); }
    }
    if (bootReady && ui.screen == Screen::Splash) ui.screen = Screen::Dashboard;
    Command command;
    while (ui.take(command)) {
        try {
            switch (command.type) {
                case CommandType::Quit:
                    try { savePosition(); } catch (const std::exception& e) { log(e.what()); }
                    quitting = true; worker->cancelled = true; monitor->cancelled = true; break;
                case CommandType::Keyboard:
                    try { video.startKeyboard(ui); }
                    catch (...) { ui.finishEdit({}); throw; }
                    break;
                case CommandType::ArchiveSignIn: {
                    if (demo) throw std::runtime_error("Sign in on your PS4; desktop preview does not use accounts");
                    if (worker->busy()) throw std::runtime_error("Finish or cancel the current operation, then sign in");
                    const bool started = worker->start([this, email = std::move(command.archiveEmail), password = std::move(command.archivePassword)]() mutable {
                        Result r; r.kind = ResultKind::ArchiveLogin;
                        try {
                            auto session = archiveLogin(email, password, &worker->cancelled);
                            std::fill(password.begin(), password.end(), '\0'); password.clear();
                            if (worker->cancelled) throw std::runtime_error("Archive.org sign-in cancelled");
                            saveArchiveSession(directory, session);
                            r.archiveSession = std::move(session);
                            r.message = "Signed into Archive.org. Retry the failed download from Downloads.";
                        } catch (const std::exception& e) { r.message = e.what(); }
                        std::fill(password.begin(), password.end(), '\0'); password.clear();
                        return r;
                    });
                    if (!started) throw std::runtime_error("Finish or cancel the current operation, then sign in");
                    ui.archiveBusy = true; ui.loading = true;
                    ui.archiveNotice = ui.status = "Signing into Archive.org...";
                    break;
                }
                case CommandType::ArchiveSignOut:
                    if (worker->busy()) throw std::runtime_error("Finish or cancel the current operation, then sign out");
                    if (!demo) clearArchiveSession(directory);
                    archiveSession = {}; ui.archiveSignedIn = false; ui.archiveBusy = false;
                    ui.archiveEmail.clear(); ui.archivePassword.clear();
                    ui.archiveNotice = ui.status = "Signed out of Archive.org on this PS4";
                    break;
                case CommandType::SaveSession:
                    resume.reset();
                    try { savePosition(); }
                    catch (const std::exception& e) { ui.browseNotice=std::string("Could not save browsing history: ")+e.what(); throw; }
                    break;
                case CommandType::Save: {
                    Settings next = command.configuration.value(); syncActiveProfile(next);
                    bool changedSource = next.sources.size() != saved.sources.size();
                    for (size_t i = 0; !changedSource && i < next.sources.size(); ++i)
                        changedSource = next.sources[i].url != saved.sources[i].url || next.sources[i].category != saved.sources[i].category;
                    try { saveSettings(directory, next); }
                    catch (const std::exception& e) {
                        ui.settings = saved; ui.rebuild();
                        ui.settingsNotice = std::string("Save failed: ") + e.what();
                        throw;
                    }
                    ui.settings = saved = next; ui.draft = saved;
                    if (changedSource) {
                        ui.available.clear(); ui.markedTitles.clear();
                        for (auto& local:ui.installed) { local.url.clear(); local.coverUrl.clear(); local.description.clear(); local.igdb={}; local.igdbJson.clear(); local.minFirmware.clear(); local.release.clear(); local.latestVersion.clear(); local.size=0; }
                        try { ui.available=parseCatalog(readFile(catalogCachePath(directory,next),CacheLimit),"",CacheLimit); } catch (const std::exception&) {}
                        unlink((directory+"/catalog.json").c_str());
                    }
                    ui.rebuild();
                    ui.settingsNotice = "Saved to " + directory + "/config.json";
                    ui.status = "Settings saved";
                    try { savePosition(); } catch (const std::exception& e) { log(e.what()); }
                    log(ui.settingsNotice);
                    if (changedSource) { startRemote = true; if (!worker->busy()) { startRemote = false; refresh(true); } }
                    break;
                }
                case CommandType::Refresh: startRemote = true; refresh(false); break;
                case CommandType::Storage:
                case CommandType::SetStorage: {
                    if (demo) { ui.status = "Desktop preview — sample storage capacities"; break; }
                    const bool apply = command.type == CommandType::SetStorage;
                    const auto destination = command.source ? InstallDestination::External : InstallDestination::Internal;
                    if (!worker->start([apply, destination] {
                        if (apply) setInstallDestination(destination);
                        Result r; r.kind = ResultKind::Storage; r.storage = scanInstallStorage();
                        r.message = !r.storage->error.empty() ? r.storage->error : apply ?
                            std::string("Installation destination: ") + (destination == InstallDestination::External ? "External USB" : "Internal storage") : "Installation storage checked";
                        return r;
                    })) throw std::runtime_error("Wait for the current operation, then press Triangle to check storage");
                    ui.status = "Checking installation storage..."; ui.loading = true; break;
                }
                case CommandType::Test: {
                    Settings next = ui.draft;
                    if (!worker->start([this, next] {
                        size_t count = 0, sources = 0;
                        for (const auto& source : next.sources) if (!source.url.empty()) {
                            if (worker->cancelled) throw std::runtime_error("Connection test cancelled");
                            try { count += inspectSource(source, httpGet(source.url, MaxCatalogBytes, &worker->cancelled)).packages; ++sources; }
                            catch (const std::exception& e) { throw std::runtime_error(sourceFailure(source, e.what())); }
                        }
                        if (!sources) throw std::runtime_error("Enter a Games JSON URL before testing");
                        Result r; r.message = "Connection OK: " + std::to_string(count) + " packages in " + std::to_string(sources) + " catalog(s)"; return r;
                    })) throw std::runtime_error("Wait for the current operation, then test again");
                    ui.status = "Testing catalog connection..."; ui.loading = true; break;
                }
                case CommandType::ClearArtwork:
                    if (demo) { ui.featureNotice = ui.status = "Preview: no real cache files deleted"; break; }
                    artwork.clear(); ui.featureNotice = ui.status = "Artwork cache cleared";
                    if (ui.screen == Screen::Cleanup) ui.usage = scanStorageUsage(directory, protectedStaging(), queueWritable);
                    break;
                case CommandType::ScanUsage:
                case CommandType::CleanStaging: {
                    if (demo) { ui.featureNotice = "Preview: sample usage; no real files deleted"; break; }
                    const auto paths = protectedStaging(); const bool trusted = queueWritable;
                    const bool clean = command.type == CommandType::CleanStaging;
                    if (!worker->start([this, paths, trusted, clean] {
                        Result r; r.kind = ResultKind::Maintenance;
                        try {
                            if (clean) {
                                const auto removed = cleanStaging(directory, paths, trusted);
                                r.message = "Removed " + std::to_string(removed.files) + " files / " + formatSize(removed.bytes) + (removed.failed ? "; " + std::to_string(removed.failed) + " files could not be removed" : "; active and uncertain packages kept");
                            } else r.message = "Storage usage checked. Unmarked legacy packages stay protected.";
                            r.usage = scanStorageUsage(directory, paths, trusted);
                        } catch (const std::exception& e) { r.message = e.what(); }
                        return r;
                    })) throw std::runtime_error("Wait for the current operation, then refresh or clean storage");
                    cleaningStaging = clean; ui.loading = true; ui.featureNotice = ui.status = clean ? "Cleaning released staging files..." : "Checking cache and staging usage..."; break;
                }
                case CommandType::ExportDiagnostics: {
                    if (demo) throw std::runtime_error("Desktop preview does not export sample diagnostics to USB");
                    DiagnosticSnapshot snapshot; snapshot.revision = PSGI_REVISION; snapshot.installed = ui.installed.size(); snapshot.available = ui.available.size();
                    snapshot.storage = ui.storage; snapshot.usage = ui.usage; snapshot.queue = ui.downloads;
                    {
                        std::lock_guard<std::mutex> lock(logMutex);
                        FILE* file = std::fopen((directory + "/app.log").c_str(), "rb");
                        if (file) {
                            if (!fseeko(file,0,SEEK_END)) { const auto length = ftello(file); if (length >= 0 && !fseeko(file, length > 262144 ? length - 262144 : 0, SEEK_SET)) {
                                snapshot.logTail.resize(size_t(std::min<off_t>(length,262144))); snapshot.logTail.resize(std::fread(snapshot.logTail.data(),1,snapshot.logTail.size(),file));
                            } }
                            std::fclose(file);
                        }
                    }
                    const auto device = command.text;
                    if (!worker->start([snapshot, device] {
                        Result r; r.kind = ResultKind::Maintenance;
                        try { requireBackupDevice(device); r.message = "Diagnostics saved: " + exportDiagnostics(device, snapshot, timestamp()); }
                        catch (const std::exception& e) { r.message = e.what(); }
                        return r;
                    })) throw std::runtime_error("Wait for the current operation, then export diagnostics");
                    ui.loading = true; ui.featureNotice = ui.status = "Writing sanitized diagnostics to USB..."; break;
                }
                case CommandType::RefreshArtwork: artwork.refresh(command.entry,command.source==1); ui.status = "Artwork will be reloaded"; break;
                case CommandType::Rate: {
                    auto next = ui.player; next.rate(command.entry.titleId, command.source); savePlayer(next);
                    ui.status = command.source ? "Local rating saved: " + std::to_string(command.source) + "/5" : "Local rating removed";
                    log(ui.status); break;
                }
                case CommandType::SaveLocalInfo: {
                    try {
                        auto next = ui.player; next.setInfo(command.entry.titleId, command.personal.value());
                        auto position = ui.captureView(); savePlayer(next); ui.restoreView(position);
                        ui.localDraft = ui.player.games.at(command.entry.titleId).info;
                        ui.localNotice = ui.status = "Local info saved on this PS4";
                        if (command.returnToActions) { ui.quickAssignment = false; ui.screen = Screen::Actions; }
                    } catch (const std::exception& e) { ui.localNotice = std::string("Save failed: ") + e.what(); throw; }
                    break;
                }
                case CommandType::BulkAssign: {
                    try {
                        auto next = ui.player; next.assignMany(command.titles, command.personal.value(), command.source);
                        const auto position = ui.captureView(); savePlayer(next); ui.restoreView(position);
                        ui.bulkEditing = ui.quickAssignment = false; ui.screen = Screen::BulkMenu;
                        ui.featureNotice = ui.status = "Saved local changes for " + std::to_string(command.titles.size()) + " games";
                    } catch (const std::exception& e) { ui.localNotice = ui.featureNotice = std::string("Save failed: ") + e.what(); throw; }
                    break;
                }
                case CommandType::SaveSavedCollection:
                case CommandType::DeleteSavedCollection: {
                    auto next = ui.player;
                    if (command.type == CommandType::SaveSavedCollection) next.saveCollection(command.collection.value(), command.previous);
                    else next.deleteCollection(command.text);
                    savePlayer(next); ui.screen = Screen::SavedCollections;
                    ui.savedIndex = std::min(ui.savedIndex, ui.savedRows().size() - 1);
                    ui.featureNotice = ui.status = "Saved collections updated on this PS4"; break;
                }
                case CommandType::ScanBackups:
                    if (!demo) ui.backupDevices = scanBackupDevices();
                    ui.backupIndex = 0;
                    ui.featureNotice = ui.status = ui.backupDevices.empty() ? "Connect a writable FAT/exFAT USB drive, then press Triangle. Extended Storage is for games, not profile exports." : "USB drives checked";
                    break;
                case CommandType::ExportBackup: {
                    if (!playerWritable) throw std::runtime_error("Repair or restore the current player.json before exporting");
                    if (demo) throw std::runtime_error("Desktop preview does not export sample records to USB");
                    requireBackupDevice(command.text);
                    ui.featureNotice = ui.status = "Backup saved: " + exportPlayerBackup(command.text, ui.player, timestamp());
                    ui.backupDevices = scanBackupDevices(); break;
                }
                case CommandType::RestoreBackup: {
                    if (demo) throw std::runtime_error("Desktop preview does not restore USB records");
                    requireBackupDevice(command.text);
                    auto restored = readPlayerBackup(command.text, command.previous);
                    restorePlayerBackup(directory, restored, timestamp());
                    playerWritable = true; ui.player = std::move(restored); ui.filter = {}; ui.query.clear();
                    ui.markedTitles.clear(); ui.bulkMode = false; ui.rebuild(); ui.screen = Screen::Backups;
                    ui.featureNotice = ui.status = "Personal records restored. Previous player.json kept as player-before-restore-*.json in application storage.";
                    break;
                }
                case CommandType::TestSource: {
                    const auto source = command.candidate.value();
                    if (!worker->start([this, source] {
                        Result r; r.kind = ResultKind::SourceTest; SourceReport report; report.source = source;
                        try { report = inspectSource(source, httpGet(source.url, MaxCatalogBytes, &worker->cancelled)); }
                        catch (const std::exception& e) { report.message = sourceFailure(source, e.what()); }
                        r.message = report.message; r.sourceReport = std::move(report); return r;
                    })) { ui.sourceTesting = false; throw std::runtime_error("Wait for the current operation, then test this source again"); }
                    ui.loading = true; ui.featureNotice = ui.status = "Testing source URL and catalog format..."; break;
                }
                case CommandType::RetryQueue: {
                    if (cleaningStaging) throw std::runtime_error("Wait for storage cleanup before queuing packages");
                    auto* job = findJob(ui.downloads, command.text);
                    if (!job) throw std::runtime_error("Queue item no longer exists");
                    if (job->stage != QueueStage::Failed && job->stage != QueueStage::Cancelled) throw std::runtime_error("Only failed or cancelled packages can be retried");
                    if (tasks.count(command.text)) throw std::runtime_error("Cancel the existing owned PS4 task before retrying");
                    const auto required = queueRequirements(job->entry, ui);
                    auto next = ui.downloads; enqueueJob(next, job->entry, timestamp());
                    findJob(next, job->entry.url)->dependsOn = required.dependencyUrl;
                    if (!queueWritable) throw std::runtime_error("queue.json needs repair; original file was kept unchanged");
                    if (!demo) saveQueue(directory, next);
                    ui.downloads = std::move(next); syncQueued();
                    if (ui.failedInstall.url == command.text) ui.installError.clear();
                    ui.featureNotice = ui.status = "Package queued for retry"; break;
                }
                case CommandType::CancelQueue: {
                    auto* job = findJob(ui.downloads, command.text);
                    if (!job) throw std::runtime_error("Queue item no longer exists");
                    if (!queueActive(job->stage) && job->task < 0 && !tasks.count(command.text) && activeDownloadUrl != command.text) {
                        ui.featureNotice = ui.status = "No active tracked task remains. Use Remove entry to clear this history."; break;
                    }
                    if (command.text == activeDownloadUrl) {
                        pendingCancels.insert(command.text); job->stage = QueueStage::Cancelling; worker->cancelled = true;
                        job->message = "Cancelling direct download. Waiting for the transfer/registration worker.";
                    } else if (tasks.count(command.text)) {
                        pendingCancels.insert(command.text); job->stage = QueueStage::Cancelling; job->message = "Waiting for PS4 to release the owned task";
                    } else {
                        job->stage = QueueStage::Cancelled; job->message = "Queue entry cancelled; no tracked PS4 task remains. Complete staged package kept for retry.";
                    }
                    persistQueue(); syncQueued(); ui.featureNotice = ui.status = job->message; break;
                }
                case CommandType::RemoveQueue: {
                    auto* job = findJob(ui.downloads, command.text);
                    if (!job) { ui.featureNotice = "Queue entry is no longer present"; break; }
                    if (queueActive(job->stage) || job->task >= 0 || tasks.count(command.text) || activeDownloadUrl == command.text || pendingCancels.count(command.text))
                        throw std::runtime_error("This package has an active or tracked task. Use Cancel task first.");
                    auto next = ui.downloads;
                    next.erase(std::remove_if(next.begin(), next.end(), [&](const QueueItem& item) { return item.entry.url == command.text; }), next.end());
                    if (!queueWritable) throw std::runtime_error("queue.json needs repair; original file was kept unchanged");
                    if (!demo) saveQueue(directory, next);
                    ui.downloads = std::move(next);
                    ui.queueIndex = ui.downloads.empty() ? 0 : std::min(ui.queueIndex, ui.downloads.size() - 1);
                    if (ui.failedInstall.url == command.text) ui.installError.clear();
                    syncQueued(); ui.featureNotice = ui.status = "Queue history entry removed. Installed games and saved packages kept.";
                    break;
                }
                case CommandType::ClearQueue: {
                    auto previous = ui.downloads;
                    ui.downloads.erase(std::remove_if(ui.downloads.begin(), ui.downloads.end(), [&](const QueueItem& job) {
                        return !queueActive(job.stage) && !tasks.count(job.entry.url) && job.task < 0;
                    }), ui.downloads.end());
                    try { persistQueue(); } catch (...) { ui.downloads = std::move(previous); throw; }
                    ui.featureNotice = ui.status = "Finished queue history cleared; active tasks kept"; break;
                }
                case CommandType::RefreshQueue:
                    lastPoll = elapsed - 3; lastLocalScan = elapsed - 30;
                    ui.featureNotice = ui.status = "Queue status refresh requested"; break;
                case CommandType::CreateCollection:
                case CommandType::RenameCollection:
                case CommandType::DeleteCollection: {
                    try {
                        auto next = ui.player; const bool tag = command.source != 0;
                        auto position = ui.captureView();
                        if (command.type == CommandType::CreateCollection) next.createTerm(tag, command.text);
                        else if (command.type == CommandType::RenameCollection) next.renameTerm(tag, command.previous, command.text);
                        else next.deleteTerm(tag, command.text);
                        auto& active = tag ? position.filter.tag : position.filter.category;
                        const auto old = command.type == CommandType::RenameCollection ? command.previous : command.text;
                        if (command.type != CommandType::CreateCollection && lower(active) == lower(old)) {
                            active = command.type == CommandType::RenameCollection ? localName(command.text) : "";
                        }
                        if (command.type != CommandType::CreateCollection && ui.settings.grouping == (tag ? Grouping::Tag : Grouping::LocalCategory))
                            for (auto& cursor : position.cursor) if (lower(cursor.group) == lower(old))
                                cursor.group = command.type == CommandType::RenameCollection ? localName(command.text) : "";
                        savePlayer(next); ui.restoreView(position);
                        ui.collectionNotice = ui.status = command.type == CommandType::DeleteCollection ? "Local name and assignments removed; game records kept" : "Local category/tag saved";
                        const auto choices = ui.termChoices(tag); ui.collectionIndex = std::min(ui.collectionIndex, choices.size() + 1);
                        if (command.type != CommandType::DeleteCollection) for (size_t i = 0; i < choices.size(); ++i)
                            if (lower(choices[i]) == lower(command.text)) ui.collectionIndex = i + 2;
                    } catch (const std::exception& e) { ui.collectionNotice = std::string("Save failed: ") + e.what(); throw; }
                    break;
                }
                case CommandType::Uninstall: {
                    if (demo) { ui.status = "Desktop preview cannot uninstall PS4 games"; break; }
                    for (const auto& local : ui.installed) if (local.titleId == command.entry.titleId && local.queued)
                        throw std::runtime_error("Finish or cancel this title's queued download before uninstalling");
                    if (worker->busy()) throw std::runtime_error("Wait for the current operation before uninstalling");
                    savePosition(); auto next = ui.player; next.observeInstalled({command.entry}, timestamp()); savePlayer(next);
                    const auto entry = command.entry;
                    worker->start([entry] {
                        uninstallTitle(entry.titleId);
                        Result r; r.kind = ResultKind::Uninstall; r.entry = entry;
                        r.message = "Uninstalled: " + entry.name + ". Local rating and history kept.";
                        try { r.local = scanInstalled(); r.freeBytes = freeSpace("/user"); r.storage = scanInstallStorage(); }
                        catch (const std::exception& e) { r.message += " Refresh: " + std::string(e.what()); }
                        return r;
                    });
                    ui.status = "Uninstalling " + entry.name + "..."; ui.loading = true; log(ui.status); break;
                }
                case CommandType::OpenMedia:
                    savePosition(); openMediaUrl(command.text); ui.status = "Video opened in PS4 browser"; break;
                case CommandType::Launch: {
                    if (demo) ui.status = "Desktop preview cannot launch PS4 games";
                    else {
                        if (ui.loading) throw std::runtime_error("Wait for the current operation before launching");
                        savePosition(); // The PS4 may terminate us inside the launch call.
                        const auto previous = ui.player;
                        bool recorded = false;
                        if (playerWritable) {
                            try { auto next = previous; next.noteLaunch(command.entry.titleId, timestamp()); savePlayer(next); recorded = true; }
                            catch (const std::exception& e) { log(std::string("Save launch record: ") + e.what()); }
                        }
                        log("Launch requested: " + command.entry.titleId + "; library position saved");
                        try { launchTitle(command.entry.titleId); }
                        catch (...) {
                            if (recorded) { try { savePlayer(previous); } catch (const std::exception& e) { log(e.what()); } }
                            throw;
                        }
                        ui.status = "Launch requested: " + command.entry.name; lastLocalScan = -30;
                    }
                    break;
                }
                case CommandType::Install: {
                    if (cleaningStaging) throw std::runtime_error("Wait for storage cleanup before queuing packages");
                    if (demo) { ui.status = "Desktop preview cannot install PS4 packages"; break; }
                    if (!queueWritable) throw std::runtime_error("queue.json needs repair; the original file was kept unchanged");
                    if (tasks.count(command.entry.url)) throw std::runtime_error("This package is already queued");
                    savePosition();
                    const auto required = queueRequirements(command.entry, ui);
                    auto next = ui.downloads; enqueueJob(next, command.entry, timestamp());
                    findJob(next, command.entry.url)->dependsOn = required.dependencyUrl;
                    saveQueue(directory, next); ui.downloads = std::move(next); syncQueued();
                    ui.status = "Package added to the download queue";
                    log("Install queued: " + command.entry.titleId);
                    break;
                }
            }
        } catch (const std::exception& e) {
            if (command.type == CommandType::ArchiveSignIn || command.type == CommandType::ArchiveSignOut) {
                ui.archiveBusy = false; ui.archiveNotice = e.what();
            }
            ui.featureNotice = ui.status = e.what(); log(ui.status);
        }
        std::fill(command.archivePassword.begin(), command.archivePassword.end(), '\0'); command.archivePassword.clear();
    }
    // Keep installation failures visible even if a routine library scan finishes.
    // Wait for the user to leave IME/settings rather than interrupting an edit.
    if (!quitting && ui.screen == Screen::Dashboard && !ui.installError.empty()) ui.screen = Screen::InstallError;
    if (!demo && !quitting && bootReady) {
        if (!monitor->busy()) {
            auto cancel = std::find_if(pendingCancels.begin(), pendingCancels.end(), [&](const std::string& url) { return tasks.count(url); });
            if (cancel != pendingCancels.end()) {
                const auto url = *cancel; const int task = tasks.at(url);
                const auto* job = findJob(ui.downloads, url); const Entry entry = job ? job->entry : Entry{};
                monitor->start([entry, task] {
                    Result r; r.kind = ResultKind::Cancel; r.entry = entry;
                    try { cancelInstallTask(task); } catch (const std::exception& e) { r.message = e.what(); }
                    return r;
                });
            } else if (!tasks.empty() && elapsed - lastPoll >= 3) {
                const auto tracked = tasks; const bool rescan = elapsed - lastLocalScan >= 15;
                monitor->start([tracked, rescan] {
                    Result r; r.kind = ResultKind::Poll; bool completed = false;
                    for (const auto& item : tracked) {
                        auto progress = taskProgress(item.second);
                        if (progress.found && (progress.error || progress.completed)) progress.retired = retireInactiveTask(item.second);
                        completed = completed || !progress.found || progress.error || progress.completed;
                        r.progress[item.first] = progress;
                    }
                    if (rescan || completed) { r.local = scanInstalled(); r.freeBytes = freeSpace("/user"); }
                    return r;
                });
                lastPoll = elapsed;
            }
        }
        if (!worker->busy()) {
            try { startNextDownload(video.userId); } catch (const std::exception& e) { ui.status = e.what(); log(ui.status); }
            if (!worker->busy() && elapsed - lastLocalScan >= 30) refresh(false);
        }
    }
}
}
