#include "ui.hpp"
#include "sfo.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

using namespace psgi;
template<class F> void rejects(F fn) { bool failed = false; try { fn(); } catch (const std::exception&) { failed = true; } assert(failed); }
void drain(Ui& ui) { Command command; while (ui.take(command)) {} }
int main() {
    auto entries = parseCatalog(R"({"DATA":{"http://nas/base":{"name":"Game","title_id":"CUSA00001","version":"1.00","category":"games"},"http://nas/update":{"name":"Patch","title_id":"CUSA00001","version":"1.20","category":"updates"},"http://nas/dlc":{"name":"Expansion","title_id":"CUSA00001","category":"dlc"},"http://nas/other":{"name":"Other","title_id":"CUSA00002","category":"games"}}})", "");
    PlayerData data; data.observeInstalled({entries[0]}, 100); data.rate("CUSA00001", 5); data.noteLaunch("CUSA00001", 200);
    data.setInfo("CUSA00001", {"RPG", "Keep this note", {"Solo"}});
    data.setInfo("CUSA00002", {"Action", "Another note", {"Co-op"}});
    data.assignMany({"CUSA00001", "CUSA00002"}, {"Adventure", "ignored", {}}, 0);
    assert(data.games.at("CUSA00001").info.category == "Adventure" && data.games.at("CUSA00002").info.category == "Adventure");
    data.assignMany({"CUSA00001", "CUSA00002"}, {"", "", {"Weekend", "weekend"}}, 1);
    assert(data.games.at("CUSA00001").info.tags.size() == 2 && data.games.at("CUSA00002").info.tags.size() == 2);
    data.assignMany({"CUSA00001", "CUSA00002"}, {"", "", {"weekend"}}, 2);
    assert(data.games.at("CUSA00001").info.tags == std::vector<std::string>{"Solo"});
    assert(data.games.at("CUSA00001").rating == 5 && data.games.at("CUSA00001").launches == 1 && data.games.at("CUSA00001").wasInstalled);
    assert(data.games.at("CUSA00002").info.description == "Another note" && data.games.at("CUSA00001").info.description == "Keep this note");
    auto before = serializePlayerData(data);
    rejects([&] { data.assignMany({"CUSA00001", "INVALID"}, {"Broken", "", {}}, 0); });
    assert(serializePlayerData(data) == before);
    auto full = data; LocalInfo many;
    for (int i = 0; i < 16; ++i) many.tags.push_back("Tag " + std::to_string(i));
    full.setInfo("CUSA00002", many); before = serializePlayerData(full);
    rejects([&] { full.assignMany({"CUSA00001", "CUSA00002"}, {"", "", {"Overflow"}}, 1); });
    assert(serializePlayerData(full) == before);
    data.assignMany({"CUSA00002"}, {}, 3); assert(data.games.at("CUSA00002").info.tags.empty());

    SavedCollection saved; saved.name = "Highly rated adventures"; saved.available = true; saved.filter.category = "Adventure";
    saved.filter.minRating = 4; saved.filter.launchState = 2; data.saveCollection(saved);
    auto roundtrip = parsePlayerData(serializePlayerData(data)); assert(roundtrip.collections.size() == 1);
    roundtrip.apply(entries); entries[0].installed = true;
    assert(matchesLocal(entries[0], roundtrip.collections[0].filter) && !matchesLocal(entries[3], roundtrip.collections[0].filter));
    saved.name = "Renamed rules"; saved.filter.installState = 3; roundtrip.saveCollection(saved, "Highly rated adventures");
    assert(!matchesLocal(entries[0], roundtrip.collections[0].filter)); entries[0].installed = false;
    assert(matchesLocal(entries[0], roundtrip.collections[0].filter));
    roundtrip.renameTerm(false, "Adventure", "Story games"); assert(roundtrip.collections[0].filter.category == "Story games");
    roundtrip.deleteTerm(false, "Story games"); roundtrip.apply(entries);
    assert(!matchesLocal(entries[0], roundtrip.collections[0].filter)); // Deletion cannot broaden a saved rule.
    rejects([&] { roundtrip.saveCollection(saved); }); // Duplicate names are rejected.
    saved.name = "Bad rules"; saved.filter.minRating = 5; saved.filter.maxRating = 2;
    rejects([&] { data.saveCollection(saved); });
    assert(parsePlayerData(R"({"schema":2,"games":{}})").collections.empty());
    rejects([] { parsePlayerData(R"({"schema":3,"games":{},"collections":[{"name":"bad","filter":{"launch_state":3}}]})"); });
    ViewState view; view.filter = data.collections[0].filter;
    auto restoredView = parseViewState(serializeViewState(view)); assert(restoredView.filter.minRating == 4 && restoredView.filter.launchState == 2);

    assert(compareVersions("01.10", "1.2").value() > 0 && compareVersions("1.0.0", "01.00").value() == 0);
    assert(!compareVersions("unknown", "1.0") && !compareVersions("1..2", "1") && !compareVersions("9999999", "1"));
    Entry local = entries[0]; local.installed = true; local.version = "1.10";
    matchInstalled(entries, {local}, {});
    assert(entries[0].installed && !entries[1].installed && entries[1].installedVersion == "1.10");
    assert(!entries[2].installed && entries[2].baseInstalled && packageStatus(entries[2]).find("unknown") != std::string::npos);
    assert(packageStatus(entries[1]) == "Newer update available");
    const auto related = relatedPackages(entries, "CUSA00001"); assert(related == std::vector<size_t>({0, 1, 2}));
    Settings settings; settings.grouping = Grouping::Title;
    auto grouped = makeView(entries, settings, Library::Available); assert(grouped.size() == 2 && grouped[0].index == 0 && grouped[1].index == 3);
    grouped = makeView(entries,settings,Library::Available,"",{},true);
    assert(grouped.size() == 2 && grouped[0].group == grouped[1].group && grouped[0].index == 1 && grouped[1].index == 2);
    assert(parseSettings(serializeSettings(settings)).grouping == Grouping::Title);

    Ui diagnostics;
    QueueItem encodedUrl; encodedUrl.entry = entries[0];
    encodedUrl.entry.url = "http://nas/ARK%20Survival%20Evolved%20%5BEU%5D.pkg?token=a%2Bb%2520";
    diagnostics.downloads = {encodedUrl};
    assert(diagnostics.queueDetails().back() == "Package URL: http://nas/ARK Survival Evolved [EU].pkg?token=a+b%20");
    assert(parseQueue(serializeQueue(diagnostics.downloads))[0].entry.url == encodedUrl.entry.url);

    std::vector<QueueItem> queue; enqueueJob(queue, entries[0], 100); rejects([&] { enqueueJob(queue, entries[0], 101); });
    queue[0].stage = QueueStage::Downloading; sampleJob(queue[0], 100, 200, 2); assert(queue[0].bytesPerSecond == 50);
    queue[0].stage = QueueStage::Ready;
    assert(queueActive(queue[0].stage) && queueStageName(queue[0].stage) == "Waiting to install");
    assert(parseQueue(serializeQueue(queue))[0].stage == QueueStage::Ready);
    queue[0].stage = QueueStage::Downloading;
    const auto encoded = serializeQueue(queue); assert(parseQueue(encoded)[0].transferred == 100);
    queue[0].backgroundTransfer = true; assert(parseQueue(serializeQueue(queue))[0].backgroundTransfer);
    queue[0].stage = QueueStage::Verifying; queue[0].verificationStarted = 100; queue[0].systemCompleted = true;
    verifyJob(queue[0], {}, 101); assert(queue[0].stage == QueueStage::Verifying);
    verifyJob(queue[0], {}, 220); assert(queue[0].stage == QueueStage::Failed && queue[0].message.find("could not be confirmed") != std::string::npos);
    QueueItem mismatchedBase; mismatchedBase.entry = entries[0]; mismatchedBase.entry.version = "01.23";
    mismatchedBase.stage = QueueStage::Verifying; mismatchedBase.systemCompleted = true;
    Entry actualBase = entries[0]; actualBase.version = "01.00";
    verifyJob(mismatchedBase, {actualBase}, 230);
    assert(mismatchedBase.stage == QueueStage::Completed && mismatchedBase.message.find("catalog version 01.23") != std::string::npos);
    mismatchedBase.stage = QueueStage::Verifying; mismatchedBase.systemCompleted = false;
    verifyJob(mismatchedBase, {actualBase}, 231); assert(mismatchedBase.stage == QueueStage::Verifying);
    mismatchedBase.systemCompleted = true; actualBase.titleId = "CUSA99999";
    verifyJob(mismatchedBase, {actualBase}, 232); assert(mismatchedBase.stage == QueueStage::Verifying);
    QueueItem timeout = mismatchedBase; timeout.stage = QueueStage::Failed;
    timeout.message = "PS4 task finished but installation could not be confirmed. Check Downloads.";
    const auto timeoutMessage = timeout.message;
    verifyJob(timeout, {}, 400); assert(timeout.stage == QueueStage::Failed && timeout.message == timeoutMessage);
    actualBase.titleId = entries[0].titleId;
    verifyJob(timeout, {actualBase}, 401); assert(timeout.stage == QueueStage::Completed);
    timeout.stage = QueueStage::Failed; timeout.message = "Install error 0x80991404";
    verifyJob(timeout, {actualBase}, 402); assert(timeout.stage == QueueStage::Failed && timeout.message == "Install error 0x80991404");
    timeout.message = timeoutMessage; timeout.systemCompleted = false;
    verifyJob(timeout, {actualBase}, 403); assert(timeout.stage == QueueStage::Failed && timeout.message == timeoutMessage);
    enqueueJob(queue, entries[1], 300); auto& update = queue.back(); update.stage = QueueStage::Verifying;
    verifyJob(update, {local}, 301); assert(update.stage == QueueStage::Verifying);
    update.systemCompleted = true;
    verifyJob(update, {local}, 301); assert(update.stage == QueueStage::Verifying); // Native completion cannot prove a catalog update version.
    local.version = "1.20"; verifyJob(update, {local}, 302); assert(update.stage == QueueStage::Completed);
    QueueItem vanished; vanished.entry = entries[0]; vanished.previouslyPresent = true; vanished.previousVersion = local.version; vanished.stage = QueueStage::Verifying;
    verifyJob(vanished, {local}, 305); assert(vanished.stage == QueueStage::Verifying); // An old installed copy cannot prove a vanished task succeeded.
    enqueueJob(queue, entries[2], 400); auto& dlc = queue.back(); dlc.stage = QueueStage::Verifying;
    verifyJob(dlc, {local}, 401); assert(dlc.stage == QueueStage::Verifying);
    dlc.systemCompleted = true; verifyJob(dlc, {local}, 402); assert(dlc.stage == QueueStage::Completed && dlc.message.find("no separate game tile") != std::string::npos);
    auto fromTemporary = parseQueue(R"({"schema":1,"jobs":[{"url":"http://nas/r30","name":"r30 entry","stage":10}]})");
    assert(fromTemporary[0].stage == QueueStage::Verifying);
    assert(parseQueue(serializeQueue(fromTemporary))[0].stage == QueueStage::Verifying);
    rejects([] { parseQueue(R"({"schema":2,"jobs":[]})"); });
    rejects([] { parseQueue(R"({"schema":1,"jobs":[{"url":"http://nas/a","name":"x","task":-1}]})"); });

    char temporary[] = "/tmp/psgi-features-XXXXXX"; assert(mkdtemp(temporary)); const std::string root = temporary;
    std::filesystem::create_directory(root + "/usb0"); std::filesystem::create_directory(root + "/local");
    assert(scanBackupDevices(root, false).size() == 1 && scanBackupDevices(root, true).empty());
    const auto backup = exportPlayerBackup(root + "/usb0", data, 1000);
    assert(exportPlayerBackup(root + "/usb0", data, 1000) != backup);
    auto devices = scanBackupDevices(root, false); assert(devices[0].files.size() == 2);
    auto fromUsb = readPlayerBackup(devices[0].path, std::filesystem::path(backup).filename());
    assert(serializePlayerData(fromUsb) == serializePlayerData(data));
    rejects([&] { readPlayerBackup(devices[0].path, "../player.json"); });
    const auto link = root + "/usb0/PS4-Games-Installer-player-link.json";
    std::filesystem::create_symlink(backup, link); rejects([&] { readPlayerBackup(devices[0].path, std::filesystem::path(link).filename()); });
    assert(scanBackupDevices(root, false)[0].files.size() == 2);
    atomicWrite(root + "/local/player.json", "corrupt original profile");
    restorePlayerBackup(root + "/local", fromUsb, 1001);
    assert(readFile(root + "/local/player-before-restore-1001-0.json") == "corrupt original profile");
    assert(serializePlayerData(loadPlayerData(root + "/local")) == serializePlayerData(data));
    before = readFile(root + "/local/player.json"); std::filesystem::create_directory(root + "/local/player.json.tmp");
    rejects([&] { restorePlayerBackup(root + "/local", fromUsb, 1002); }); assert(readFile(root + "/local/player.json") == before);
    saveQueue(root + "/local", parseQueue(encoded)); assert(loadQueue(root + "/local")[0].stage == QueueStage::Failed);

    const auto report = inspectSource({"games", "http://nas/games.json"}, R"({"DATA":{"http://nas/a":{"name":"A","title_id":"CUSA00001"},"http://nas/b":{"name":"B"}}})");
    assert(report.success && report.games == 2 && report.missingIds == 1 && report.packages == 2);
    assert(inspectSource({"games", "http://nas/games"}, R"({"DATA":{}})").packages == 0);
    rejects([] { inspectSource({"games", "http://nas/"}, "<html>Not a catalog</html>"); });
    rejects([] { inspectSource({"games", "http://nas/"}, R"({"bad":{}})"); });
    assert(sourceFailure({"games", "http://nas/"}, "HTTP 404").find("JSON endpoint") != std::string::npos);

    // Controller paths for bulk selection, collection rules, source setup and related packages.
    Ui ui; ui.screen = Screen::Dashboard; ui.setLibraries(std::vector<Entry>{entries[0], entries[3]}, entries); ui.player = data; ui.rebuild();
    ui.handle(Button::Options); ui.menuIndex = 11; ui.handle(Button::Cross); assert(ui.bulkMode && ui.screen == Screen::Dashboard);
    ui.handle(Button::Cross); ui.handle(Button::Right); ui.handle(Button::Cross); assert(ui.markedTitles.size() == 2);
    ui.handle(Button::Triangle); ui.bulkIndex = 1; ui.handle(Button::Cross); assert(ui.screen == Screen::PickTags && ui.bulkEditing);
    ui.handle(Button::Triangle); drain(ui); ui.finishEdit("Weekend"); ui.handle(Button::Square);
    Command command; assert(ui.take(command) && command.type == CommandType::BulkAssign && command.source == 1 && command.titles.size() == 2);
    assert(command.personal->tags == std::vector<std::string>{"Weekend"}); ui.handle(Button::Circle); assert(ui.screen == Screen::BulkMenu);
    ui.bulkIndex = 3; ui.handle(Button::Cross); assert(ui.screen == Screen::ConfirmBulkClear);
    ui.handle(Button::Circle); assert(!ui.take(command)); ui.handle(Button::Circle); ui.handle(Button::Circle); assert(!ui.bulkMode);
    ui.handle(Button::Options); ui.menuIndex = 13; ui.handle(Button::Cross); ui.handle(Button::Triangle);
    ui.handle(Button::Cross); drain(ui); ui.finishEdit("New rules"); assert(ui.savedDraft.name == "New rules");
    ui.savedEditIndex = 5; ui.handle(Button::Left); assert(ui.savedDraft.filter.minRating == 5 && ui.savedDraft.filter.maxRating == 5);
    ui.savedEditIndex = 9; ui.handle(Button::Cross); assert(ui.take(command) && command.type == CommandType::SaveSavedCollection && command.collection->name == "New rules");
    ui.handle(Button::Circle); ui.savedIndex = 2; ui.handle(Button::Cross); drain(ui); assert(ui.filter.minRating == 4 && ui.screen == Screen::Dashboard);
    ui.handle(Button::Options); ui.menuIndex = 15; ui.handle(Button::Cross); assert(ui.screen == Screen::SourceSetup);
    ui.setupIndex = 2; ui.handle(Button::Cross); assert(ui.screen == Screen::SourceSetup);
    ui.handle(Button::Triangle); drain(ui); ui.finishEdit("http://nas/games.json");
    assert(ui.take(command) && command.type == CommandType::TestSource && command.candidate->url == "http://nas/games.json");
    ui.sourceReport = report; ui.setupIndex = 2; ui.handle(Button::Cross);
    assert(ui.screen == Screen::Settings && ui.draft.sources[0].url == "http://nas/games.json" && ui.settings.sources[0].url.empty());
    ui.filter = {}; ui.screen = Screen::Dashboard; ui.library = Library::Installed; ui.rebuild();
    ui.handle(Button::Triangle); const auto rows = ui.actionRows(); const auto packages = std::find(rows.begin(), rows.end(), "Game packages");
    assert(packages != rows.end()); ui.menuIndex = packages - rows.begin(); ui.handle(Button::Cross);
    assert(ui.screen == Screen::Packages && ui.packageRows().size() == 3);
    ui.handle(Button::Down); ui.handle(Button::Square); assert(ui.screen == Screen::Details && ui.pending.category == "updates");
    ui.handle(Button::Circle); assert(ui.screen == Screen::Packages);
    std::filesystem::remove_all(root);
    std::puts("Bulk atomicity, saved rules, USB recovery, queue verification, package versions, source validation and controller flows passed");
}
