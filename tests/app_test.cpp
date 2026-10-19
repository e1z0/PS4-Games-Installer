#include "app.hpp"
#include "platform.hpp"
#include "download.hpp"
#include "video.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
std::mutex requestMutex;
std::string lastUrl;
std::atomic<bool> allowInstalls{false}, allowRetirement{false};
std::atomic<bool> rejectInstall{false}, retainFailedStart{false};
std::atomic<int> submissions{0}, progressMode{0};
std::atomic<bool> blockProbe{false}, uniqueTasks{false}, failCancellation{false};
std::atomic<int> cancellations{0};
std::atomic<bool> blockPoll{false}, pollEntered{false};
std::atomic<bool> failProfileRemote{false}, blockProfileHome{false}, profileHomeBlocked{false};
std::string sourceResponse;
std::vector<psgi::Entry> localGames;
std::string launchDirectory;
bool allowLaunch = false, failLaunch = false, allowUninstall = false;
int launchCalls = 0, uninstallCalls = 0;
std::atomic<bool> failArchiveLogin{false}, expectArchiveDownload{false};
std::atomic<int> archiveLoginCalls{0}, authenticatedTransfers{0};
}
namespace psgi {
Video::Video(bool) {}
Video::~Video() {}
void Video::present(const Canvas&) {}
void Video::startKeyboard(Ui&) {}
void Video::pollKeyboard(Ui&) {}
void Video::flushInput() {}
std::vector<Button> Video::input(Ui&) { return {}; }
void prepareConsole() {}
int finishApplication(int status) { return status; }
std::vector<Entry> scanInstalled() { std::lock_guard<std::mutex> lock(requestMutex); return localGames; }
std::vector<Entry> demoLibrary() { return {}; }
uint64_t freeSpace(const std::string&) { return 0; }
void launchTitle(const std::string& id) {
    if (!allowLaunch) throw std::runtime_error("PS4 required");
    const auto saved = loadViewState(launchDirectory);
    assert(saved.cursor[saved.available ? 1 : 0].titleId == id); // Must exist before shell handoff.
    assert(loadPlayerData(launchDirectory).games.at(id).launches == (failLaunch ? 2 : 1));
    ++launchCalls;
    if (failLaunch) throw std::runtime_error("Launch fixture failed");
}
void uninstallTitle(const std::string& id) {
    assert(allowUninstall && loadPlayerData(launchDirectory).games.at(id).wasInstalled);
    std::lock_guard<std::mutex> lock(requestMutex); ++uninstallCalls;
    localGames.erase(std::remove_if(localGames.begin(), localGames.end(), [&](const Entry& entry) { return entry.titleId == id; }), localGames.end());
}
int queueInstall(const Entry&, int) {
    if (!allowInstalls) throw std::runtime_error("PS4 required");
    if (rejectInstall) throw std::runtime_error("PS4 refused installation (0x80990086). No matching task could be confirmed. Downloaded package kept.");
    const int count = ++submissions; const int task = uniqueTasks ? 100 + count : 45;
    if (retainFailedStart) throw SubmissionFailure("Start task failed; owned registration retained", false, task);
    return task;
}
int queueLocalInstall(const Entry&, const std::string&, int) { throw std::runtime_error("PS4 required"); }
bool retireInactiveTask(int task) { assert(task >= 45); return allowRetirement; }
void cancelInstallTask(int task) { assert(task >= 45); ++cancellations; if (failCancellation) throw std::runtime_error("Cancellation fixture failed; task retained"); }
HttpTransferInfo httpTransfer(const std::string& url, const HttpSink& sink, bool range, const std::atomic<bool>* cancelled) {
    while (blockProbe && !(cancelled && *cancelled)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
    assert(allowInstalls && range); sink("\x7f" "CNT", 4, 4); return {206, 4, url};
}
HttpTransferInfo httpTransferAuthenticated(const std::string& url, const HttpSink& sink, bool range,
                                         const ArchiveSession& session, const std::atomic<bool>* cancelled) {
    if (expectArchiveDownload) {
        assert(session.user == "fixture%40example.test" && session.sig == "fixture-private-signature");
        ++authenticatedTransfers;
    }
    return httpTransfer(url, sink, range, cancelled);
}
ArchiveSession archiveLogin(const std::string& email, const std::string& password, const std::atomic<bool>*) {
    assert(email == "fixture@example.test" && password == "fixture-private-password");
    ++archiveLoginCalls;
    if (failArchiveLogin) throw std::runtime_error("Archive.org sign-in failed. Check your email and password.");
    return {"fixture%40example.test", "fixture-private-signature"};
}
TaskProgress taskProgress(int task) {
    assert(task >= 45);
    if (blockPoll) {
        pollEntered = true;
        while (blockPoll) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    TaskProgress result{100, 100, progressMode == 1 ? int32_t(0x80991404u) : 0, true};
    if (progressMode == 3) result.transferred = 50;
    result.completed = progressMode == 2;
    return result;
}
std::string httpGet(const std::string& url, size_t, const std::atomic<bool>* cancelled) {
    if (url=="http://profiles-home/games" && blockProfileHome) {
        profileHomeBlocked=true;
        while (blockProfileHome && !(cancelled && *cancelled)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::lock_guard<std::mutex> lock(requestMutex); lastUrl = url;
    if (url=="http://profiles-home/games") return R"({"DATA":{"http://profiles-home/game.pkg":{"name":"Home game","title_id":"CUSA00001"}}})";
    if (url=="http://profiles-remote/games") {
        if (failProfileRemote) throw std::runtime_error("HTTP 503 profile fixture");
        return R"({"DATA":{"http://profiles-remote/game.pkg":{"name":"Remote online","title_id":"CUSA00002"}}})";
    }
    if (!sourceResponse.empty()) return sourceResponse;
    return R"({"DATA":{"http://fixture/game.pkg":{"name":"Saved source game","title_id":"CUSA00001"}}})";
}
std::string httpGetArtwork(const std::string& url, size_t limit, const std::atomic<bool>* cancelled) { return httpGet(url,limit,cancelled); }
}

using namespace psgi;
void settleAt(App& app, Video& video, int line) {
    for (int i = 0; i < 200; ++i) {
        app.tick(0, video);
        if (!app.ui.loading && app.ui.screen != Screen::Splash) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::fprintf(stderr, "Worker timeout at test line %d: screen=%d loading=%d status=%s\n", line, int(app.ui.screen), int(app.ui.loading), app.ui.status.c_str());
    assert(false && "Application worker did not finish");
}
#define settle(app, video) settleAt(app, video, __LINE__)
void editUrl(App& app, Video& video, const std::string& url) {
    auto& ui = app.ui;
    ui.settingsIndex = 0; ui.handle(Button::Cross);
    assert(ui.screen == Screen::Keyboard);
    app.tick(0, video); // Process the real keyboard command.
    ui.finishEdit(url);
    assert(ui.screen == Screen::Settings && ui.draft.sources[0].url == url);
    assert(ui.settingsNotice.find("Select Save settings") != std::string::npos);
}
void waitForStatus(App& app, Video& video, double elapsed, const char* text) {
    for (int i = 0; i < 200; ++i) {
        app.tick(elapsed, video);
        if (app.ui.status.find(text) != std::string::npos) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    assert(false && "Expected installation state was not observed");
}
void action(Ui& ui, const std::string& label) {
    ui.screen = Screen::Dashboard; ui.handle(Button::Triangle);
    const auto rows = ui.actionRows();
    const auto found = std::find_if(rows.begin(), rows.end(), [&](const std::string& row) { return row.find(label) == 0; });
    assert(found != rows.end()); ui.menuIndex = found - rows.begin(); ui.handle(Button::Cross);
}
int main() {
    char temporary[] = "/tmp/psgi-save-XXXXXX";
    assert(mkdtemp(temporary));
    const std::string directory = temporary, url = "http://fixture/games";
    Video video;
    {
        App app(directory); settle(app, video);
        auto& ui = app.ui;
        ui.handle(Button::Options);
        for (int i = 0; i < 6; ++i) ui.handle(Button::Down);
        ui.handle(Button::Cross); assert(ui.screen == Screen::Settings);
        editUrl(app, video, url);
        assert(loadSettings(directory).sources[0].url.empty()); // Accepting text is still a draft.
        ui.settingsIndex = 10; ui.handle(Button::Cross); app.tick(0, video);
        assert(ui.screen == Screen::Settings && ui.settingsNotice == "Saved to " + directory + "/config.json");
        assert(loadSettings(directory).sources[0].url == url);
        settle(app, video);
        assert(ui.available.size() == 1 && ui.available[0].name == "Saved source game");
        { std::lock_guard<std::mutex> lock(requestMutex); assert(lastUrl == url); }

        // Save the draft captured when Save was pressed, even if the screen changes.
        const std::string next = "http://fixture/updated";
        editUrl(app, video, next);
        ui.settingsIndex = 10; ui.handle(Button::Cross); ui.screen = Screen::Options;
        app.tick(0, video); settle(app, video);
        assert(loadSettings(directory).sources[0].url == next);

        ui.screen = Screen::Settings;
        editUrl(app, video, url);
        std::filesystem::create_directory(directory + "/config.json.tmp"); // Deterministic write failure, including as root.
        ui.settingsIndex = 10; ui.handle(Button::Cross); app.tick(0, video);
        assert(ui.screen == Screen::Settings && ui.settingsNotice.find("Save failed:") == 0);
        assert(ui.settings.sources[0].url == next && ui.draft.sources[0].url == url);
        assert(loadSettings(directory).sources[0].url == next);
        std::filesystem::remove(directory + "/config.json.tmp");
        ui.handle(Button::Cross); app.tick(0, video); settle(app, video);
        assert(loadSettings(directory).sources[0].url == url);
        assert(ui.settingsNotice == "Saved to " + directory + "/config.json");
    }
    {
        App restarted(directory);
        assert(restarted.ui.settings.sources[0].url == url);
    }
    {
        auto settings = loadSettings(directory); settings.refreshOnStart = false; saveSettings(directory, settings);
        App app(directory); settle(app, video);
        allowInstalls = true;
        app.ui.handle(Button::R1); app.ui.handle(Button::Cross); app.ui.handle(Button::Cross);
        app.tick(0, video); settle(app, video);
        assert(submissions == 1 && app.ui.queued.size() == 1);
        const auto packageUrl = app.ui.available[0].url;
        assert(readFile(directory + "/tasks.json").find(packageUrl) != std::string::npos);
        std::filesystem::create_directory(directory + "/downloads");
        const auto staged = stagedPackagePath(directory, packageUrl); atomicWrite(staged, "staging fixture");

        // 100% transferred alone must not release a package still being installed.
        waitForStatus(app, video, 4, "installation is still running");
        assert(app.ui.queued.size() == 1 && std::filesystem::exists(staged));
        progressMode = 1; allowRetirement = false;
        waitForStatus(app, video, 8, "cancel the failed task");
        assert(app.ui.queued.size() == 1 && std::filesystem::exists(staged));
        assert(readFile(directory + "/tasks.json").find(packageUrl) != std::string::npos);

        assert(app.ui.screen == Screen::InstallError);
        app.ui.handle(Button::Circle); app.tick(8, video);
        assert(app.ui.screen == Screen::Dashboard && app.ui.installError.empty());
        app.ui.status = "Awaiting repeated task failure";
        waitForStatus(app, video, 11, "cancel the failed task");
        assert(app.ui.screen == Screen::Dashboard && app.ui.installError.empty()); // Acknowledged errors do not reopen on every poll.
        allowRetirement = true;
        waitForStatus(app, video, 15, "failed task removed");
        assert(app.ui.queued.empty() && !std::filesystem::exists(staged));
        assert(readFile(directory + "/tasks.json") == "{}");

        // Retry becomes possible only after the owned task was actually retired.
        app.ui.handle(Button::Cross); app.ui.handle(Button::Cross);
        app.tick(15, video); settle(app, video); assert(submissions == 2);
        atomicWrite(staged, "staging fixture"); progressMode = 2;
        waitForStatus(app, video, 19, "installation is not confirmed");
        assert(app.ui.downloads[0].stage == QueueStage::Verifying && !std::filesystem::exists(staged));
        assert(readFile(directory + "/tasks.json") == "{}");
        { std::lock_guard<std::mutex> lock(requestMutex); Entry installed = app.ui.available[0]; installed.installed = true; localGames = {installed}; }
        app.ui.screen = Screen::Queue; app.ui.handle(Button::Triangle); app.tick(19, video);
        waitForStatus(app, video, 19, "confirmed by a fresh");
        assert(app.ui.downloads[0].stage == QueueStage::Completed && app.ui.queued.empty());
        { std::lock_guard<std::mutex> lock(requestMutex); localGames.clear(); }
    }
    const auto history = directory + "/queue-history"; std::filesystem::create_directory(history);
    Settings historySettings; historySettings.refreshOnStart = false; saveSettings(history, historySettings);
    QueueItem historical; historical.entry.name = "Historical result"; historical.entry.url = "http://fixture/old.pkg";
    historical.entry.titleId = "CUSA00001"; historical.stage = QueueStage::Failed; historical.systemCompleted = true;
    historical.verificationStarted = 100; historical.message = "PS4 task finished but installation could not be confirmed. Old timeout.";
    QueueItem other = historical; other.entry.url = "http://fixture/other.pkg"; other.stage = QueueStage::Completed;
    saveQueue(history, {historical, other});
    std::filesystem::create_directory(history + "/downloads");
    const auto savedPackage = stagedPackagePath(history, historical.entry.url); atomicWrite(savedPackage, "kept package");
    {
        App app(history); settle(app, video);
        assert(app.ui.downloads[0].stage == QueueStage::Failed); // Reporting rollback does not reclassify old failures.
        const int cancelledBefore = cancellations, submittedBefore = submissions, uninstalledBefore = uninstallCalls;
        app.ui.screen = Screen::QueueDetails; app.ui.queueIndex = 0;
        app.ui.handle(Button::Triangle); app.ui.handle(Button::Circle); app.tick(0, video);
        assert(app.ui.downloads.size() == 2 && loadQueue(history).size() == 2);
        app.ui.handle(Button::Triangle); app.ui.handle(Button::Cross); app.tick(0, video);
        assert(app.ui.downloads.size() == 1 && app.ui.downloads[0].entry.url == other.entry.url);
        assert(loadQueue(history).size() == 1 && std::filesystem::exists(savedPackage));
        assert(cancellations == cancelledBefore && submissions == submittedBefore);
        // Failed writes preserve the selected history entry for retry.
        std::filesystem::create_directory(history + "/queue.json.tmp");
        app.ui.screen = Screen::QueueDetails; app.ui.handle(Button::Triangle); app.ui.handle(Button::Cross); app.tick(0, video);
        assert(app.ui.downloads.size() == 1 && loadQueue(history).size() == 1);
        std::filesystem::remove(history + "/queue.json.tmp");
        app.ui.screen = Screen::QueueDetails; app.ui.handle(Button::Triangle); app.ui.handle(Button::Cross); app.tick(0, video);
        assert(app.ui.downloads.empty() && loadQueue(history).empty() && app.ui.queueIndex == 0);
        assert(uninstallCalls == uninstalledBefore && std::filesystem::exists(savedPackage));
    }
    // A completion poll already in flight may release a task after Cancel was requested.
    const auto cancelRace = directory + "/cancel-race"; std::filesystem::create_directory(cancelRace);
    saveSettings(cancelRace, historySettings);
    QueueItem finishing = historical; finishing.stage = QueueStage::Installing; finishing.task = 175;
    finishing.systemCompleted = false; finishing.verificationStarted = 0;
    saveQueue(cancelRace, {finishing});
    { std::lock_guard<std::mutex> lock(requestMutex); localGames = {finishing.entry}; }
    progressMode = 2; allowRetirement = true;
    {
        App app(cancelRace); settle(app, video);
        pollEntered = false; blockPoll = true; app.tick(4, video);
        for (unsigned i = 0; i < 200 && !pollEntered; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        assert(pollEntered);
        app.ui.screen = Screen::QueueDetails; app.ui.handle(Button::Triangle); app.ui.handle(Button::Cross); app.tick(4, video);
        assert(app.ui.downloads[0].stage == QueueStage::Cancelling);
        blockPoll = false;
        waitForStatus(app, video, 4, "confirmed by a fresh");
        assert(app.ui.downloads[0].stage == QueueStage::Completed && app.ui.downloads[0].task < 0);
        app.ui.handle(Button::Triangle); app.ui.handle(Button::Cross); app.tick(4, video);
        assert(app.ui.downloads.empty() && loadQueue(cancelRace).empty());
    }
    progressMode = 0;
    { std::lock_guard<std::mutex> lock(requestMutex); localGames.clear(); }
    // Hardware regression: catalog 01.23, installed base 01.00, completed/released task.
    const auto versionMismatch = directory + "/version-mismatch"; std::filesystem::create_directory(versionMismatch);
    saveSettings(versionMismatch, historySettings);
    QueueItem bonfire; bonfire.entry.name = "Bonfire Peaks"; bonfire.entry.titleId = "CUSA28188";
    bonfire.entry.url = "http://fixture/bonfire.pkg"; bonfire.entry.category = "games"; bonfire.entry.version = "01.23";
    bonfire.stage = QueueStage::Verifying; bonfire.systemCompleted = true;
    saveQueue(versionMismatch, {bonfire}); atomicWrite(versionMismatch + "/catalog.json", serializeCatalog({bonfire.entry}));
    Entry actualBonfire = bonfire.entry; actualBonfire.version = "01.00"; actualBonfire.url.clear(); actualBonfire.installed = true;
    { std::lock_guard<std::mutex> lock(requestMutex); localGames = {actualBonfire}; }
    const int beforeSubmissions = submissions, beforeCancellations = cancellations;
    {
        App app(versionMismatch); settle(app, video);
        assert(app.ui.downloads[0].stage == QueueStage::Completed && app.ui.queued.empty());
        assert(app.ui.installed[0].installed && !app.ui.installed[0].queued);
        assert(app.ui.available[0].installed && !app.ui.available[0].queued);
        assert(loadQueue(versionMismatch)[0].stage == QueueStage::Completed);
    }
    {
        App app(versionMismatch); settle(app, video);
        assert(app.ui.downloads[0].stage == QueueStage::Completed && app.ui.queued.empty());
        assert(submissions == beforeSubmissions && cancellations == beforeCancellations);
    }
    // The same queue entry may have timed out before the fixed build was installed.
    bonfire.stage = QueueStage::Failed; bonfire.verificationStarted = 100;
    bonfire.message = "PS4 task finished but installation could not be confirmed. Check Downloads.";
    saveQueue(versionMismatch, {bonfire});
    {
        App app(versionMismatch); settle(app, video);
        assert(app.ui.downloads[0].stage == QueueStage::Completed && loadQueue(versionMismatch)[0].stage == QueueStage::Completed);
        assert(!app.ui.installed[0].queued && submissions == beforeSubmissions && cancellations == beforeCancellations);
    }
    { std::lock_guard<std::mutex> lock(requestMutex); localGames.clear(); }
    const auto refusal = directory + "/refusal"; std::filesystem::create_directory(refusal);
    Settings offline; offline.refreshOnStart = false; saveSettings(refusal, offline);
    atomicWrite(refusal + "/catalog.json", R"({"DATA":{"http://fixture/game.pkg":{"name":"Rejected package","title_id":"CUSA00001"}}})");
    {
        App app(refusal); settle(app, video); app.ui.handle(Button::R1); rejectInstall = true;
        app.ui.handle(Button::Cross); app.ui.handle(Button::Cross); app.tick(0, video);
        app.ui.screen = Screen::Keyboard; settle(app, video);
        assert(app.ui.screen == Screen::Keyboard && app.ui.installError.find("0x80990086") != std::string::npos);
        app.ui.screen = Screen::Dashboard; app.tick(0, video);
        assert(app.ui.screen == Screen::InstallError && app.ui.failedInstall.name == "Rejected package");
        assert(app.ui.queued.empty() && !std::filesystem::exists(refusal + "/tasks.json"));
        // Routine discovery must not dismiss the installation error.
        app.tick(31, video); settle(app, video);
        assert(app.ui.screen == Screen::InstallError && app.ui.installError.find("0x80990086") != std::string::npos);
        app.ui.handle(Button::Cross); assert(app.ui.screen == Screen::ConfirmInstall);
        rejectInstall = false; app.ui.handle(Button::Cross); app.tick(0, video); settle(app, video);
        assert(app.ui.screen == Screen::Dashboard && app.ui.installError.empty() && !app.ui.queued.empty());
    }
    const auto personal = directory + "/personal"; std::filesystem::create_directory(personal);
    Settings personalSettings; personalSettings.refreshOnStart = false; saveSettings(personal, personalSettings);
    auto games = parseCatalog(R"({"DATA":{"http://fixture/a.pkg":{"name":"Alpha","title_id":"CUSA00001"},"http://fixture/b.pkg":{"name":"Bravo","title_id":"CUSA00002"}}})", "games");
    atomicWrite(personal + "/catalog.json", serializeCatalog(games));
    { std::lock_guard<std::mutex> lock(requestMutex); localGames = games; for (auto& entry : localGames) { entry.installed = true; entry.url.clear(); } }
    launchDirectory = personal; allowLaunch = true;
    {
        App app(personal); settle(app, video); app.ui.selected[0] = 1;
        assert(app.ui.current()->titleId == "CUSA00002");
        app.ui.handle(Button::Cross); app.tick(0, video);
        assert(launchCalls == 1 && loadPlayerData(personal).games.at("CUSA00002").launches == 1);
    }
    { std::lock_guard<std::mutex> lock(requestMutex); localGames[1].name = "Aardvark"; }
    {
        App app(personal); settle(app, video);
        assert(app.ui.library == Library::Installed && app.ui.position() == 0 && app.ui.current()->titleId == "CUSA00002");
        // Rating goes through the action menu and is durable independently of the catalog.
        action(app.ui, "Rate game"); assert(app.ui.screen == Screen::Rating);
        app.ui.ratingIndex = 3; app.ui.handle(Button::Cross); app.tick(0, video);
        assert(app.ui.current()->rating == 4 && loadPlayerData(personal).games.at("CUSA00002").rating == 4);
        action(app.ui, "Edit local info"); app.ui.handle(Button::Cross); app.ui.handle(Button::Triangle);
        app.tick(0, video); app.ui.finishEdit("Adventure"); app.ui.handle(Button::Circle);
        app.ui.localIndex = 1; app.ui.handle(Button::Cross); app.ui.handle(Button::Triangle);
        app.tick(0, video); app.ui.finishEdit("Weekend"); app.ui.handle(Button::Circle);
        app.ui.localIndex = 2; app.ui.handle(Button::Cross); app.tick(0, video); app.ui.finishEdit("My own description");
        assert(loadPlayerData(personal).games.at("CUSA00002").info.category.empty());
        std::filesystem::create_directory(personal + "/player.json.tmp");
        app.ui.localIndex = 3; app.ui.handle(Button::Cross); app.tick(0, video);
        assert(app.ui.screen == Screen::LocalInfo && app.ui.localDirty() && app.ui.localNotice.find("Save failed") == 0);
        assert(loadPlayerData(personal).games.at("CUSA00002").info.category.empty());
        std::filesystem::remove(personal + "/player.json.tmp");
        app.ui.handle(Button::Cross); app.tick(0, video);
        assert(!app.ui.localDirty() && app.ui.current()->info.description == "My own description");
        assert(loadPlayerData(personal).games.at("CUSA00002").info.tags == std::vector<std::string>{"Weekend"});
        // Quick category assignment saves immediately; failure keeps the picker for retry.
        action(app.ui, "Assign category"); assert(app.ui.screen == Screen::PickCategory);
        std::filesystem::create_directory(personal + "/player.json.tmp");
        app.ui.pickerIndex = 0; app.ui.handle(Button::Cross); app.tick(0, video);
        assert(app.ui.screen == Screen::PickCategory && app.ui.quickAssignment && app.ui.localNotice.find("Save failed") == 0);
        assert(loadPlayerData(personal).games.at("CUSA00002").info.category == "Adventure");
        std::filesystem::remove(personal + "/player.json.tmp");
        app.ui.handle(Button::Cross); app.tick(0, video);
        assert(app.ui.screen == Screen::Actions && !app.ui.quickAssignment);
        assert(loadPlayerData(personal).games.at("CUSA00002").info.category.empty());
        action(app.ui, "Assign category"); assert(app.ui.pickerIndex == 1);
        app.ui.handle(Button::Cross); app.tick(0, video);
        assert(app.ui.screen == Screen::Actions && loadPlayerData(personal).games.at("CUSA00002").info.category == "Adventure");
        // Edit through Installed, then browse/filter the same record in Available.
        app.ui.handle(Button::Circle); app.ui.handle(Button::R1);
        const auto tagged = std::find_if(app.ui.available.begin(), app.ui.available.end(), [](const Entry& e) { return e.titleId == "CUSA00002"; });
        assert(tagged != app.ui.available.end() && tagged->info.description == "My own description");
        const auto taggedPosition = std::find_if(app.ui.visible.begin(), app.ui.visible.end(), [&](const VisibleEntry& e) { return app.ui.available[e.index].titleId == "CUSA00002"; });
        assert(taggedPosition != app.ui.visible.end()); app.ui.selected[1] = taggedPosition - app.ui.visible.begin();
        action(app.ui, "Assign tags"); app.ui.handle(Button::Cross); app.ui.handle(Button::Circle); app.tick(0, video);
        assert(app.ui.screen == Screen::Actions && loadPlayerData(personal).games.at("CUSA00002").info.tags == std::vector<std::string>{"Weekend"});
        action(app.ui, "Assign tags"); app.ui.handle(Button::Triangle); app.tick(0, video); app.ui.finishEdit("Co-op");
        std::filesystem::create_directory(personal + "/player.json.tmp");
        app.ui.handle(Button::Square); app.tick(0, video);
        assert(app.ui.screen == Screen::PickTags && app.ui.localDraft.tags.size() == 2 && app.ui.localNotice.find("Save failed") == 0);
        assert(loadPlayerData(personal).games.at("CUSA00002").info.tags.size() == 1);
        std::filesystem::remove(personal + "/player.json.tmp");
        app.ui.handle(Button::Square); app.tick(0, video);
        assert(app.ui.screen == Screen::Actions && !app.ui.quickAssignment);
        const auto quickSaved = loadPlayerData(personal).games.at("CUSA00002");
        assert(quickSaved.info.tags.size() == 2 && quickSaved.info.category == "Adventure");
        assert(quickSaved.info.description == "My own description" && quickSaved.rating == 4 && quickSaved.launches == 1 && quickSaved.wasInstalled);
        const auto localSaved = std::find_if(app.ui.installed.begin(), app.ui.installed.end(), [](const Entry& e) { return e.titleId == "CUSA00002"; });
        assert(localSaved != app.ui.installed.end() && localSaved->info.tags.size() == 2);
        action(app.ui, "Assign tags"); app.ui.pickerIndex = 1; app.ui.handle(Button::Cross); app.ui.handle(Button::Square); app.tick(0, video);
        assert(loadPlayerData(personal).games.at("CUSA00002").info.tags == std::vector<std::string>{"Weekend"});
        app.ui.handle(Button::Circle);
        app.ui.handle(Button::Options); app.ui.menuIndex = 9; app.ui.handle(Button::Cross);
        app.ui.collectionIndex = 2; app.ui.handle(Button::Cross); app.tick(0, video);
        assert(app.ui.current()->titleId == "CUSA00002" && app.ui.filter.category == "Adventure");
        app.ui.handle(Button::Options); app.ui.menuIndex = 9; app.ui.handle(Button::Cross); app.ui.collectionIndex = 2;
        app.ui.handle(Button::Square); app.tick(0, video); app.ui.finishEdit("Story games"); app.tick(0, video);
        assert(app.ui.filter.category == "Story games" && loadPlayerData(personal).games.at("CUSA00002").info.category == "Story games");
        app.ui.handle(Button::Options); app.ui.handle(Button::Circle); // Cancel deletion first.
        assert(loadPlayerData(personal).categories == std::vector<std::string>{"Story games"});
        app.ui.handle(Button::Options); app.ui.handle(Button::Cross); app.tick(0, video);
        assert(app.ui.filter.category.empty() && loadPlayerData(personal).categories.empty());
        assert(loadPlayerData(personal).games.at("CUSA00002").info.description == "My own description");
        app.ui.screen = Screen::Dashboard; app.ui.handle(Button::L1);
        failLaunch = true;
        app.ui.screen = Screen::Dashboard; app.ui.handle(Button::Cross); app.tick(0, video);
        assert(launchCalls == 2 && app.ui.status == "Launch fixture failed");
        assert(loadPlayerData(personal).games.at("CUSA00002").launches == 1);
        failLaunch = false;
        std::filesystem::create_directory(personal + "/session.json.tmp");
        app.ui.screen = Screen::Dashboard; app.ui.handle(Button::Cross); app.tick(0, video);
        assert(launchCalls == 2 && app.ui.status.find("Cannot write") != std::string::npos);
        std::filesystem::remove(personal + "/session.json.tmp");

        action(app.ui, "Uninstall"); assert(app.ui.screen == Screen::ConfirmUninstall);
        app.ui.handle(Button::Circle); app.tick(0, video); assert(uninstallCalls == 0);
        action(app.ui, "Uninstall"); allowUninstall = true;
        app.ui.handle(Button::Cross); app.tick(0, video); settle(app, video);
        assert(uninstallCalls == 1 && app.ui.installed.size() == 1);
        const auto removed = std::find_if(app.ui.available.begin(), app.ui.available.end(), [](const Entry& entry) { return entry.titleId == "CUSA00002"; });
        assert(removed != app.ui.available.end() && !removed->installed && removed->wasInstalled && removed->rating == 4);
        const auto records = loadPlayerData(personal);
        assert(records.games.at("CUSA00002").wasInstalled && records.games.at("CUSA00002").rating == 4);
        assert(records.games.at("CUSA00002").info.description == "My own description");
        app.ui.handle(Button::R1); assert(app.ui.librarySummary().find("available to install") != std::string::npos);
    }
    {
        App app(personal); settle(app, video);
        assert(app.ui.player.games.at("CUSA00002").rating == 4 && app.ui.player.games.at("CUSA00002").wasInstalled);
        assert(app.ui.player.games.at("CUSA00002").info.tags == std::vector<std::string>{"Weekend"});
    }
    const auto corrupt = directory + "/corrupt"; std::filesystem::create_directory(corrupt);
    saveSettings(corrupt, personalSettings); atomicWrite(corrupt + "/player.json", "broken personal records");
    {
        App app(corrupt); settle(app, video); action(app.ui, "Rate game");
        app.ui.ratingIndex = 2; app.ui.handle(Button::Cross); app.tick(0, video);
        assert(app.ui.status.find("original file was kept") != std::string::npos);
        assert(readFile(corrupt + "/player.json") == "broken personal records");
    }
    const auto extras = directory + "/extras"; std::filesystem::create_directory(extras);
    saveSettings(extras, personalSettings); atomicWrite(extras + "/catalog.json", serializeCatalog(games));
    PlayerData seed; seed.setInfo("CUSA00001", {"Old category", "Persistent note", {"Solo"}}); seed.rate("CUSA00001", 5);
    savePlayerData(extras, seed);
    { std::lock_guard<std::mutex> lock(requestMutex); localGames = games; for (auto& e : localGames) { e.installed = true; e.version = "1.00"; } }
    {
        App app(extras); settle(app, video);
        auto& ui = app.ui;
        ui.handle(Button::Options); ui.menuIndex = 11; ui.handle(Button::Cross);
        ui.handle(Button::Cross); ui.handle(Button::Right); ui.handle(Button::Cross); ui.handle(Button::Triangle);
        ui.bulkIndex = 0; ui.handle(Button::Cross); ui.handle(Button::Triangle); app.tick(0, video); ui.finishEdit("Bulk category");
        std::filesystem::create_directory(extras + "/player.json.tmp"); ui.handle(Button::Cross); app.tick(0, video);
        assert(ui.screen == Screen::PickCategory && ui.bulkEditing && ui.localNotice.find("Save failed") == 0);
        assert(loadPlayerData(extras).games.at("CUSA00001").info.category == "Old category");
        std::filesystem::remove(extras + "/player.json.tmp"); ui.handle(Button::Cross); app.tick(0, video);
        assert(ui.screen == Screen::BulkMenu && loadPlayerData(extras).games.at("CUSA00002").info.category == "Bulk category");
        assert(loadPlayerData(extras).games.at("CUSA00001").rating == 5 && loadPlayerData(extras).games.at("CUSA00001").info.description == "Persistent note");
        ui.bulkIndex = 3; ui.handle(Button::Cross); ui.handle(Button::Circle); app.tick(0, video);
        assert(!loadPlayerData(extras).games.at("CUSA00001").info.tags.empty());
        ui.handle(Button::Cross); ui.handle(Button::Cross); app.tick(0, video);
        assert(ui.screen == Screen::BulkMenu && loadPlayerData(extras).games.at("CUSA00001").info.tags.empty());
        ui.handle(Button::Circle); ui.handle(Button::Circle); assert(!ui.bulkMode);
        ui.handle(Button::Options); ui.menuIndex = 13; ui.handle(Button::Cross); ui.handle(Button::Triangle);
        ui.handle(Button::Cross); app.tick(0, video); ui.finishEdit("Five-star collection");
        ui.savedEditIndex = 5; ui.handle(Button::Left); ui.savedEditIndex = 9;
        std::filesystem::create_directory(extras + "/player.json.tmp"); ui.handle(Button::Cross); app.tick(0, video);
        assert(ui.screen == Screen::EditSaved && loadPlayerData(extras).collections.empty());
        std::filesystem::remove(extras + "/player.json.tmp"); ui.handle(Button::Cross); app.tick(0, video);
        assert(ui.screen == Screen::SavedCollections && loadPlayerData(extras).collections[0].filter.minRating == 5);
        ui.savedIndex = 4; ui.handle(Button::Cross); app.tick(0, video);
        assert(ui.visible.size() == 1 && ui.current()->titleId == "CUSA00001");
        ui.handle(Button::Options); ui.menuIndex = 15; ui.handle(Button::Cross);
        ui.handle(Button::Triangle); app.tick(0, video); ui.finishEdit("http://new-server/games"); app.tick(0, video); settle(app, video);
        assert(ui.sourceReport.success && ui.sourceReport.packages == 1 && loadSettings(extras).sources[0].url.empty());
        ui.setupIndex = 2; ui.handle(Button::Cross); assert(ui.screen == Screen::Settings && ui.draft.sources[0].url == "http://new-server/games");
        ui.handle(Button::Cross); app.tick(0, video); settle(app, video);
        assert(loadSettings(extras).sources[0].url == "http://new-server/games");
        ui.settingsIndex = 12; ui.handle(Button::Cross);
        { std::lock_guard<std::mutex> lock(requestMutex); sourceResponse = "<html>Wrong endpoint</html>"; }
        ui.handle(Button::Square); app.tick(0, video); settle(app, video);
        assert(!ui.sourceReport.success && ui.sourceReport.message.find("web page") != std::string::npos);
        ui.setupIndex = 2; ui.handle(Button::Cross); assert(ui.screen == Screen::SourceSetup);
        { std::lock_guard<std::mutex> lock(requestMutex); sourceResponse.clear(); }
        ui.backupDevices = {{"/mnt/usb7/disconnected-fixture", {}}}; ui.backupIndex = 0; ui.screen = Screen::Backups;
        ui.handle(Button::Cross); app.tick(0, video);
        assert(ui.featureNotice.find("disconnected") != std::string::npos);
    }
    { App app(extras); settle(app, video); assert(app.ui.player.collections[0].name == "Five-star collection"); }

    const auto managed = directory + "/managed"; std::filesystem::create_directory(managed); saveSettings(managed, offline);
    atomicWrite(managed + "/catalog.json", serializeCatalog(games));
    { std::lock_guard<std::mutex> lock(requestMutex); localGames.clear(); }
    uniqueTasks = true; blockProbe = true; progressMode = 0; allowRetirement = false;
    {
        App app(managed); settle(app, video);
        auto enqueue = [&](const Entry& entry) { app.ui.pending = entry; app.ui.screen = Screen::ConfirmInstall; app.ui.handle(Button::Cross); app.tick(0, video); };
        enqueue(games[0]); enqueue(games[1]);
        assert(app.ui.downloads.size() == 2 && app.ui.downloads[1].stage == QueueStage::Waiting && app.ui.queued.size() == 2);
        const int beforeCancel = cancellations;
        app.ui.screen = Screen::QueueDetails; app.ui.queueIndex = 1; app.ui.handle(Button::Triangle); app.ui.handle(Button::Cross); app.tick(0, video);
        assert(app.ui.downloads[1].stage == QueueStage::Cancelled && cancellations == beforeCancel);
        app.ui.queueIndex = 0; app.ui.handle(Button::Triangle); app.ui.handle(Button::Cross); app.tick(0, video); settle(app, video);
        assert(app.ui.downloads[0].stage == QueueStage::Cancelled && cancellations == beforeCancel && app.ui.queued.empty());
        blockProbe = false;
        app.ui.handle(Button::Cross); app.tick(0, video); settle(app, video);
        assert(app.ui.downloads[0].stage == QueueStage::Installing && app.ui.downloads[0].task >= 100);
        assert(app.ui.downloads[0].backgroundTransfer);
        {
            // Simulate a historical confirmation racing with a new active task.
            auto& job = app.ui.downloads[0]; const auto savedStage = job.stage; const auto savedTask = job.task;
            job.stage = QueueStage::Failed; job.task = -1;
            app.ui.screen = Screen::QueueDetails; app.ui.handle(Button::Triangle);
            job.stage = savedStage; job.task = savedTask;
            app.ui.handle(Button::Cross); app.tick(0, video);
            assert(app.ui.downloads.size() == 2 && app.ui.downloads[0].task == savedTask);
            assert(app.ui.featureNotice.find("Use Cancel task") != std::string::npos);
            app.ui.screen = Screen::QueueDetails; app.ui.queueIndex = 0;
        }
        progressMode = 3;
        waitForStatus(app, video, 4, "PS4 background download");
        assert(app.ui.downloads[0].stage == QueueStage::Downloading && loadQueue(managed)[0].backgroundTransfer);
        progressMode = 0;
        waitForStatus(app, video, 8, "installation is still running");
        assert(app.ui.downloads[0].stage == QueueStage::Installing);
        failCancellation = true;
        app.ui.handle(Button::Triangle); app.ui.handle(Button::Cross); app.tick(0, video);
        waitForStatus(app, video, 0, "Cancellation fixture failed");
        assert(app.ui.downloads[0].task >= 100 && app.ui.queued.size() == 1);
        failCancellation = false;
        app.ui.handle(Button::Triangle); app.ui.handle(Button::Cross); app.tick(0, video);
        waitForStatus(app, video, 0, "Owned task cancelled");
        assert(app.ui.downloads[0].stage == QueueStage::Cancelled && app.ui.downloads[0].task < 0 && app.ui.queued.empty());
    }
    atomicWrite(managed + "/tasks.json", "{\"http://fixture/a.pkg\":199}"); // Stale compatibility write must not resurrect a cancelled task.
    {
        App app(managed); settle(app, video);
        assert(app.ui.downloads.size() == 2 && app.ui.downloads[0].stage == QueueStage::Cancelled && app.ui.queued.empty());
        app.ui.screen = Screen::Queue; app.ui.handle(Button::Options); app.tick(0, video); assert(app.ui.downloads.empty());
    }
    // Missing-base guard, persistent dependency ordering and a second check of
    // real installed state immediately before network/registration work.
    const auto dependencies = directory + "/dependencies"; std::filesystem::create_directory(dependencies); saveSettings(dependencies, offline);
    Entry base = games[0]; base.version = "1.00";
    Entry patch = base; patch.url += "?patch=1"; patch.category = "updates"; patch.version = "1.20";
    Entry dlc = base; dlc.url += "?dlc=1"; dlc.category = "dlc";
    { std::lock_guard<std::mutex> lock(requestMutex); localGames.clear(); }
    progressMode = 0; blockProbe = true;
    {
        App app(dependencies); settle(app, video);
        auto enqueue = [&](const Entry& entry) { app.ui.pending = entry; app.ui.screen = Screen::ConfirmInstall; app.ui.handle(Button::Cross); app.tick(0, video); };
        const auto before = submissions.load(); enqueue(patch); assert(app.ui.downloads.empty() && submissions == before && app.ui.screen == Screen::ConfirmInstall);
        enqueue(base); enqueue(patch); enqueue(dlc);
        assert(app.ui.downloads.size() == 3 && app.ui.downloads[1].dependsOn == base.url && app.ui.downloads[2].dependsOn == base.url);
        assert(loadQueue(dependencies)[1].dependsOn == base.url);
        app.ui.screen = Screen::QueueDetails; app.ui.queueIndex = 0; app.ui.handle(Button::Triangle); app.ui.handle(Button::Cross); app.tick(0, video); settle(app, video);
        for (unsigned i=0;i<10;++i) app.tick(0,video);
        assert(app.ui.downloads[0].stage == QueueStage::Cancelled && app.ui.downloads[1].stage == QueueStage::Failed && app.ui.downloads[2].stage == QueueStage::Failed && submissions == before);
        // UI scan is stale, but the worker must check the current console scan.
        blockProbe = false; app.ui.setLibraries(std::vector<Entry>{base},std::vector<Entry>{patch});
        enqueue(patch); settle(app, video);
        assert(submissions == before && app.ui.downloads[1].stage == QueueStage::Failed && app.ui.downloads[1].message.find("missing") != std::string::npos);
        // Maintenance re-checks protection when the confirmation is submitted.
        const auto staged = stagedPackagePath(dependencies, base.url); std::filesystem::create_directory(dependencies + "/downloads");
        atomicWrite(staged, "package"); atomicWrite(staged + ".lease", "released\n");
        app.ui.screen = Screen::Options; app.ui.menuIndex = 17; app.ui.handle(Button::Cross); app.tick(0,video); settle(app,video);
        assert(app.ui.usage.reclaimable == 7);
        app.ui.handle(Button::Cross); app.ui.handle(Button::Cross); app.tick(0,video); settle(app,video);
        assert(!std::filesystem::exists(staged) && app.ui.usage.staging == 0);
        app.ui.screen = Screen::Diagnostics; app.ui.backupDevices = {{"/mnt/usb7/disconnected-fixture",{}}}; app.ui.backupIndex=0;
        app.ui.handle(Button::Cross); app.tick(0,video); settle(app,video); assert(app.ui.featureNotice.find("disconnected") != std::string::npos);
    }
    const auto ordering = directory + "/dependency-ordering"; std::filesystem::create_directory(ordering); saveSettings(ordering, offline);
    QueueItem queuedPatch; queuedPatch.entry = patch; queuedPatch.dependsOn = base.url;
    QueueItem runningBase; runningBase.entry = base; runningBase.stage = QueueStage::Installing; runningBase.task = 170;
    QueueItem unrelated; unrelated.entry = games[1];
    saveQueue(ordering, {queuedPatch, runningBase, unrelated});
    progressMode = 0;
    {
        const auto before = submissions.load(); App app(ordering); settle(app,video);
        assert(submissions == before + 1 && app.ui.downloads[0].stage == QueueStage::Waiting && app.ui.downloads[2].task >= 100);
        { std::lock_guard<std::mutex> lock(requestMutex); localGames = {base}; }
        progressMode = 2; allowRetirement = true;
        for (unsigned i=0; i<200 && app.ui.downloads[0].task < 0; ++i) { app.tick(4,video); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
        assert(app.ui.downloads[1].stage == QueueStage::Completed && app.ui.downloads[0].task >= 100 && submissions == before + 2);
        assert(loadQueue(ordering)[0].dependsOn == base.url);
    }
    const auto retained = directory + "/retained-start"; std::filesystem::create_directory(retained); saveSettings(retained,offline);
    progressMode=0; retainFailedStart=true;
    {
        App app(retained); settle(app,video);
        app.ui.pending=games[1];app.ui.screen=Screen::ConfirmInstall;app.ui.handle(Button::Cross);app.tick(0,video);settle(app,video);
        assert(app.ui.downloads[0].stage==QueueStage::Failed && app.ui.downloads[0].task>=100);
        app.ui.screen=Screen::QueueDetails;app.ui.queueIndex=0;
        for (unsigned i=0;i<20;++i) { app.tick(4,video);std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
        assert(app.ui.downloads[0].stage==QueueStage::Failed && app.ui.downloads[0].message.find("owned registration retained")!=std::string::npos);
        app.ui.handle(Button::Triangle);app.ui.handle(Button::Cross);app.tick(4,video);waitForStatus(app,video,4,"Owned task cancelled");
        assert(app.ui.downloads[0].task<0 && app.ui.downloads[0].stage==QueueStage::Cancelled);
    }
    retainFailedStart=false;
    uniqueTasks = false;
    const auto profiles=directory+"/profiles";std::filesystem::create_directory(profiles);
    Settings profileSettings;profileSettings.sources={{"games","http://profiles-home/games"}};addServerProfile(profileSettings,"Home");
    profileSettings.sources={{"games","http://profiles-remote/games"}};addServerProfile(profileSettings,"Remote");useServerProfile(profileSettings,0);saveSettings(profiles,profileSettings);
    auto homeCache=games[0];homeCache.name="Home cached";homeCache.url="http://profiles-home/game.pkg";
    auto remoteCache=games[1];remoteCache.name="Remote cached";remoteCache.url="http://profiles-remote/game.pkg";
    atomicWrite(catalogCachePath(profiles,profileSettings),serializeCatalog({homeCache}));
    auto remoteSettings=profileSettings;useServerProfile(remoteSettings,1);atomicWrite(catalogCachePath(profiles,remoteSettings),serializeCatalog({remoteCache}));
    atomicWrite(profiles+"/catalog.json",serializeCatalog({games[0]})); // Must never override a named profile cache.
    {std::lock_guard<std::mutex> lock(requestMutex);localGames={games[0]};}
    {
        App app(profiles);settle(app,video);assert(app.ui.available[0].name=="Home game");auto& ui=app.ui;
        ui.handle(Button::Options);ui.menuIndex=1;ui.handle(Button::Cross);ui.handle(Button::Cross);app.tick(0,video);ui.finishEdit("Home");app.tick(0,video);
        assert(loadViewState(profiles).recentSearches==std::vector<std::string>{"Home"});
        ui.screen=Screen::Options;ui.menuIndex=20;ui.handle(Button::Cross);ui.profileIndex=1;ui.handle(Button::Cross);assert(ui.screen==Screen::Settings && ui.draft.activeProfile=="Remote" && ui.settings.activeProfile=="Home");
        std::filesystem::create_directory(profiles+"/config.json.tmp");ui.handle(Button::Cross);app.tick(0,video);
        assert(loadSettings(profiles).activeProfile=="Home" && ui.settings.activeProfile=="Home" && ui.draft.activeProfile=="Remote" && ui.available[0].name=="Home game");
        std::filesystem::remove(profiles+"/config.json.tmp");failProfileRemote=true;ui.handle(Button::Cross);app.tick(0,video);
        assert(ui.settings.activeProfile=="Remote" && ui.available.size()==1 && ui.available[0].name=="Remote cached" && ui.installed[0].url.empty());
        settle(app,video);assert(ui.available[0].name=="Remote cached" && ui.status.find("503")!=std::string::npos);
        failProfileRemote=false;blockProfileHome=true;
        ui.settingsIndex=16;ui.handle(Button::Cross);ui.profileIndex=0;ui.handle(Button::Cross);ui.handle(Button::Cross);app.tick(0,video);
        for(unsigned i=0;i<200 && !profileHomeBlocked;++i) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
        assert(profileHomeBlocked);
        assert(ui.available[0].name=="Home game"); // Existing Home cache is selected while its fetch waits.
        ui.settingsIndex=16;ui.handle(Button::Cross);ui.profileIndex=1;ui.handle(Button::Cross);ui.handle(Button::Cross);app.tick(0,video);
        assert(ui.settings.activeProfile=="Remote" && ui.available[0].name=="Remote cached");
        blockProfileHome=false;
        for(unsigned i=0;i<200;++i){app.tick(0,video);assert(ui.available.size()==1 && ui.available[0].titleId=="CUSA00002");if(!ui.loading)break;std::this_thread::sleep_for(std::chrono::milliseconds(5));}
        settle(app,video);assert(ui.available[0].name=="Remote online" && loadSettings(profiles).activeProfile=="Remote");
    }
    {App app(profiles);settle(app,video);assert(app.ui.settings.activeProfile=="Remote" && app.ui.available[0].name=="Remote online" && app.ui.recentSearches==std::vector<std::string>{"Home"});}
    const auto legacyCache=directory+"/legacy-cache";std::filesystem::create_directory(legacyCache);saveSettings(legacyCache,offline);
    atomicWrite(legacyCache+"/catalog.json",serializeCatalog({homeCache}));
    {App app(legacyCache);settle(app,video);assert(app.ui.available.size()==1 && app.ui.available[0].name=="Home cached");}
    auto externallyEdited=offline;externallyEdited.sources={{"games","http://different-server/games"}};saveSettings(legacyCache,externallyEdited);
    {App app(legacyCache);settle(app,video);assert(app.ui.available.empty());} // Legacy adoption must not repeat after an external URL change.
    const auto archiveDirectory = directory + "/archive-account";
    std::filesystem::create_directory(archiveDirectory);
    saveSettings(archiveDirectory, offline);
    auto signIn = [&](App& app) {
        auto& ui = app.ui; ui.screen = Screen::ArchiveAccount; ui.archiveIndex = 2;
        ui.archiveEmail = "fixture@example.test"; ui.archivePassword = "fixture-private-password";
        ui.handle(Button::Cross);
        assert(ui.archiveBusy && ui.archivePassword.empty() && ui.keyboardText.empty());
        app.tick(0, video); settle(app, video);
        assert(!ui.archiveBusy);
    };
    {
        App app(archiveDirectory); settle(app, video);
        assert(!app.ui.archiveSignedIn);
        failArchiveLogin = true; signIn(app);
        assert(!app.ui.archiveSignedIn && loadArchiveSession(archiveDirectory).empty());
        assert(app.ui.archiveNotice.find("sign-in failed") != std::string::npos);
        failArchiveLogin = false;
        std::filesystem::create_directory(archiveDirectory + "/archive-session.json.tmp");
        signIn(app);
        assert(!app.ui.archiveSignedIn && loadArchiveSession(archiveDirectory).empty());
        std::filesystem::remove(archiveDirectory + "/archive-session.json.tmp");
        signIn(app);
        assert(app.ui.archiveSignedIn && !loadArchiveSession(archiveDirectory).empty());
        assert(archiveLoginCalls == 3);
        for (const auto& file : {"config.json", "app.log"}) {
            const auto contents = readFile(archiveDirectory + "/" + file);
            assert(contents.find("fixture-private") == std::string::npos);
            assert(contents.find("fixture@example.test") == std::string::npos);
        }
    }
    {
        App app(archiveDirectory); settle(app, video);
        assert(app.ui.archiveSignedIn && app.ui.archivePassword.empty());
        // A failed replacement login keeps the previous saved session usable.
        failArchiveLogin = true; signIn(app);
        assert(app.ui.archiveSignedIn && !loadArchiveSession(archiveDirectory).empty());
        failArchiveLogin = false;
        allowInstalls = true; rejectInstall = false; retainFailedStart = false; progressMode = 0;
        blockProbe = true; uniqueTasks = false; expectArchiveDownload = true;
        Entry authenticated; authenticated.titleId = "CUSA98765"; authenticated.name = "Authenticated fixture";
        authenticated.url = "http://fixture/auth-game.pkg";
        app.ui.setLibraries(std::vector<Entry>{}, std::vector<Entry>{authenticated});
        app.ui.library = Library::Available; app.ui.rebuild(); app.ui.screen = Screen::Dashboard;
        app.ui.handle(Button::Cross); app.ui.handle(Button::Cross); app.tick(0, video);
        app.ui.screen = Screen::ArchiveAccount; app.ui.archiveIndex = 3; app.ui.handle(Button::Cross);
        app.tick(0, video);
        assert(app.ui.archiveSignedIn && !app.ui.archiveBusy && !loadArchiveSession(archiveDirectory).empty());
        assert(app.ui.archiveNotice.find("Finish or cancel") != std::string::npos);
        blockProbe = false; settle(app, video);
        assert(authenticatedTransfers > 0);
        expectArchiveDownload = false;
        assert(readFile(archiveDirectory + "/queue.json").find("fixture-private") == std::string::npos);
        app.ui.screen = Screen::ArchiveAccount; app.ui.archiveIndex = 3; app.ui.handle(Button::Cross);
        app.tick(0, video);
        assert(!app.ui.archiveSignedIn && !app.ui.archiveBusy && loadArchiveSession(archiveDirectory).empty());
    }
    { App app(archiveDirectory); assert(!app.ui.archiveSignedIn); }
    std::filesystem::remove_all(directory);
    std::puts("Settings persistence and tracked installation progress, failed-task recovery, retry and staging cleanup passed");
    std::puts("Launch handoff/session restart, rating persistence, uninstall confirmation/history and corrupt-record protection passed");
    std::puts("Registration refusal, persistent error/retry, IME deferral and acknowledged-task-error suppression passed");
    std::puts("Bulk rollback/retry, saved collection restart, source setup, managed queue cancellation/retry and durable history passed");
    std::puts("Archive.org sign-in failure, durable session restart, private storage and local sign-out passed");
}
