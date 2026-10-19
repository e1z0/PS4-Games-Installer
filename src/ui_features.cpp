#include "ui.hpp"
#include "metadata.hpp"
#include "sfo.hpp"
#include <algorithm>

namespace psgi {
void Ui::beginAssignment(bool tags, bool bulk) {
    bulkEditing = bulk; quickAssignment = true; localNotice.clear();
    if (bulk) { pending = {}; pending.name = std::to_string(markedTitles.size()) + " selected games"; localDraft = {}; }
    else localDraft = pending.info;
    const auto choices = termChoices(tags, true);
    const auto active = tags ? (localDraft.tags.empty() ? std::string() : localDraft.tags.front()) : localDraft.category;
    const auto found = std::find_if(choices.begin(), choices.end(), [&](const std::string& term) { return lower(term) == lower(active); });
    pickerIndex = found == choices.end() ? (choices.empty() ? 0 : 1) : 1 + (found - choices.begin());
    screen = tags ? Screen::PickTags : Screen::PickCategory;
}
void Ui::testSetupSource() {
    sourceReport = {}; sourceReport.source = setupSource; sourceTesting = true;
    Command command{}; command.type = CommandType::TestSource; command.candidate = setupSource; commands.push_back(std::move(command));
}
std::vector<std::string> Ui::bulkRows() const {
    return {"Assign category", "Add tags (keep other tags)", "Remove tags", "Clear all tags", "Select all visible games", "Clear selection", "Finish selecting"};
}
std::vector<std::string> Ui::savedRows() const {
    std::vector<std::string> rows{"All games / clear filters", "No launch requests recorded", "Rated 4-5 stars", "Previously installed"};
    for (const auto& c : player.collections) rows.push_back(c.name + (c.available ? "  [Available]" : "  [Installed]"));
    return rows;
}
std::vector<std::string> Ui::savedEditRows() const {
    const auto& f = savedDraft.filter;
    const char* launch[] = {"Any", "No launches recorded", "Launched through this app"};
    const char* installed[] = {"Any", "Installed", "Not installed", "Previously installed, now absent"};
    return {"Name: " + (savedDraft.name.empty() ? "Enter a name" : savedDraft.name),
        "Library: " + std::string(savedDraft.available ? "Available" : "Installed"), "Search: " + (savedDraft.query.empty() ? "Any" : savedDraft.query),
        "Category: " + (f.uncategorized ? "Uncategorized" : f.category.empty() ? "Any" : f.category),
        "Tag: " + (f.untagged ? "Untagged" : f.tag.empty() ? "Any" : f.tag),
        "Minimum stars: " + std::to_string(f.minRating), "Maximum stars: " + std::to_string(f.maxRating),
        "Launch history: " + std::string(launch[f.launchState]), "Installation: " + std::string(installed[f.installState]), "Save collection"};
}
std::vector<std::string> Ui::packageRows() const {
    std::vector<std::string> rows;
    for (const auto index : relatedPackages(available, packageTitle)) {
        const auto& e = available[index];
        rows.push_back(packageKindName(e) + (e.version.empty() ? "" : " v" + e.version) + " / " + e.name + " / " + formatSize(e.size) + " / " + packageStatus(e));
    }
    return rows;
}
std::vector<std::string> Ui::queueRows() const {
    std::vector<std::string> rows;
    for (const auto& job : downloads) rows.push_back(job.entry.name + " / " + queueProgress(job));
    return rows;
}
std::vector<std::string> Ui::queueDetails() const {
    if (downloads.empty()) return {};
    const auto& job = downloads[std::min(queueIndex, downloads.size() - 1)];
    return {job.entry.name, job.entry.titleId + " / " + packageKindName(job.entry) + " / v" + (job.entry.version.empty() ? "?" : job.entry.version),
        queueProgress(job), "PS4 task: " + (job.task < 0 ? std::string("not registered / released") : std::to_string(job.task)),
        job.message, "Transferred bytes only measure download progress. Installation needs PS4 confirmation.",
        "A failed or cancelled package can be retried with X. Triangle cancels an owned active task.", "Package URL: " + readableUrl(job.entry.url)};
}
InstallRequirements Ui::requirements() const {
    std::vector<Entry> bases;
    for (const auto& job : downloads) if (queueActive(job.stage) && packageKind(job.entry) == 0) bases.push_back(job.entry);
    return installationRequirements(pending, installed, bases);
}
bool Ui::handleFeatures(Button b) {
    if (screen == Screen::Cleanup || screen == Screen::ConfirmCleanup) {
        if (b == Button::Circle) { screen = screen == Screen::Cleanup ? maintenanceReturn : Screen::Cleanup; return true; }
        if (screen == Screen::ConfirmCleanup) {
            if (b == Button::Cross) { emit(cleanupArtwork ? CommandType::ClearArtwork : CommandType::CleanStaging); screen = Screen::Cleanup; }
        } else if (b == Button::Triangle) emit(CommandType::ScanUsage);
        else if (b == Button::Cross || b == Button::Square) { cleanupArtwork = b == Button::Square; screen = Screen::ConfirmCleanup; }
        return true;
    }
    if (screen == Screen::Diagnostics) {
        if (b == Button::Circle) screen = maintenanceReturn;
        if (b == Button::Triangle) emit(CommandType::ScanBackups);
        moveMenuSelection(b, backupIndex, backupDevices.size());
        if (b == Button::Cross && !backupDevices.empty()) {
            backupIndex = std::min(backupIndex, backupDevices.size() - 1);
            Command c{}; c.type = CommandType::ExportDiagnostics; c.text = backupDevices[backupIndex].path; commands.push_back(std::move(c));
        }
        return true;
    }
    if (screen == Screen::Dashboard && bulkMode) {
        if (b == Button::Circle) { bulkMode = false; markedTitles.clear(); return true; }
        if (b == Button::Triangle) { bulkIndex = 0; featureNotice.clear(); screen = Screen::BulkMenu; return true; }
        if (b == Button::Cross && !sidebar) {
            if (const auto* e = current()) {
                if (!validTitleId(e->titleId)) status = "A valid Title ID is required for local organization";
                else if (markedTitles.count(e->titleId)) markedTitles.erase(e->titleId);
                else markedTitles.insert(e->titleId);
            }
            return true;
        }
    }
    if (screen == Screen::BulkMenu || screen == Screen::ConfirmBulkClear) {
        if (screen == Screen::ConfirmBulkClear) {
            if (b == Button::Circle) screen = Screen::BulkMenu;
            else if (b == Button::Cross) { bulkOperation = 3; bulkEditing = true; localDraft = {}; saveLocalDraft(); }
            return true;
        }
        if (b == Button::Circle) { screen = Screen::Dashboard; return true; }
        moveMenuSelection(b, bulkIndex, bulkRows().size());
        if (b == Button::Cross) {
            if (bulkIndex <= 3 && markedTitles.empty()) { featureNotice = "Select at least one game with X first"; return true; }
            if (bulkIndex <= 2) { bulkOperation = bulkIndex; beginAssignment(bulkIndex != 0, true); }
            else if (bulkIndex == 3) screen = Screen::ConfirmBulkClear;
            else if (bulkIndex == 4) { for (const auto& item : visible) if (validTitleId(entries()[item.index].titleId)) markedTitles.insert(entries()[item.index].titleId); }
            else if (bulkIndex == 5) markedTitles.clear();
            else { bulkMode = false; markedTitles.clear(); screen = Screen::Dashboard; }
        }
        return true;
    }
    if (screen == Screen::SavedCollections) {
        if (b == Button::Circle) { screen = featureReturn; return true; }
        savedIndex = std::min(savedIndex, savedRows().size() - 1);
        moveMenuSelection(b, savedIndex, savedRows().size());
        if (b == Button::Triangle || (b == Button::Square && savedIndex >= 4)) {
            savedOriginal.clear(); featureNotice.clear(); savedEditIndex = 0;
            if (b == Button::Square) { savedDraft = player.collections.at(savedIndex - 4); savedOriginal = savedDraft.name; }
            else { savedDraft = {}; savedDraft.available = library == Library::Available; savedDraft.query = query; savedDraft.filter = filter; }
            screen = Screen::EditSaved;
        } else if (b == Button::Options && savedIndex >= 4) { savedOriginal = player.collections.at(savedIndex - 4).name; screen = Screen::ConfirmDeleteSaved; }
        else if (b == Button::Cross) {
            if (savedIndex >= 4) {
                const auto& c = player.collections.at(savedIndex - 4);
                filter = c.filter; query = c.query; library = c.available ? Library::Available : Library::Installed;
            } else {
                filter = {}; query.clear();
                if (savedIndex == 1) filter.launchState = 1;
                if (savedIndex == 2) filter.minRating = 4;
                if (savedIndex == 3) { filter.installState = 3; library = Library::Available; }
            }
            updatesOnly = false; remoteGenre.clear(); sidebarIndex = size_t(library); selected[static_cast<int>(library)] = 0; sidebar = false; screen = Screen::Dashboard; quickSave();
        }
        return true;
    }
    if (screen == Screen::ConfirmDeleteSaved) {
        if (b == Button::Circle) screen = Screen::SavedCollections;
        if (b == Button::Cross) { Command c{}; c.type = CommandType::DeleteSavedCollection; c.text = savedOriginal; commands.push_back(std::move(c)); }
        return true;
    }
    if (screen == Screen::EditSaved) {
        if (b == Button::Circle) { screen = Screen::SavedCollections; featureNotice.clear(); return true; }
        moveMenuSelection(b, savedEditIndex, savedEditRows().size());
        if (b != Button::Cross && b != Button::Left && b != Button::Right) return true;
        const int step = b == Button::Left ? -1 : 1;
        auto cycle = [&](unsigned& value, unsigned count) { value = (int(value) + int(count) + step) % count; };
        auto& f = savedDraft.filter;
        switch (savedEditIndex) {
            case 0: if (b == Button::Cross) beginEdit(EditKind::SavedName, 0, savedDraft.name); break;
            case 1: savedDraft.available = !savedDraft.available; break;
            case 2: if (b == Button::Cross) beginEdit(EditKind::SavedQuery, 0, savedDraft.query); break;
            case 3: case 4: {
                const bool tags = savedEditIndex == 4; auto terms = termChoices(tags);
                auto& name = tags ? f.tag : f.category; auto& empty = tags ? f.untagged : f.uncategorized;
                // Any, unassigned, and every locally registered name.
                unsigned index = empty ? 1 : 0;
                for (size_t i = 0; i < terms.size(); ++i) if (lower(terms[i]) == lower(name) && !name.empty()) index = i + 2;
                cycle(index, terms.size() + 2); empty = index == 1; name = index >= 2 ? terms.at(index - 2) : ""; break;
            }
            case 5: cycle(f.minRating, 6); f.maxRating = std::max(f.maxRating, f.minRating); break;
            case 6: cycle(f.maxRating, 6); f.minRating = std::min(f.minRating, f.maxRating); break;
            case 7: cycle(f.launchState, 3); break;
            case 8: cycle(f.installState, 4); break;
            case 9: if (b == Button::Cross) { Command c{}; c.type = CommandType::SaveSavedCollection; c.collection = savedDraft; c.previous = savedOriginal; commands.push_back(std::move(c)); } break;
        }
        return true;
    }
    if (screen == Screen::Queue || screen == Screen::QueueDetails || screen == Screen::ConfirmCancelQueue) {
        if (!downloads.empty()) queueIndex = std::min(queueIndex, downloads.size() - 1);
        if (screen == Screen::ConfirmCancelQueue) {
            if (b == Button::Circle) screen = Screen::QueueDetails;
            if (b == Button::Cross) {
                if (findJob(downloads, queueActionUrl)) {
                    Command c{}; c.type = removingQueueEntry ? CommandType::RemoveQueue : CommandType::CancelQueue;
                    c.text = queueActionUrl; commands.push_back(std::move(c));
                    screen = removingQueueEntry ? Screen::Queue : Screen::QueueDetails;
                } else { featureNotice = "Queue entry is no longer present"; screen = Screen::Queue; }
            }
            return true;
        }
        if (b == Button::Circle) { screen = screen == Screen::Queue ? featureReturn : Screen::Queue; return true; }
        if (screen == Screen::Queue) {
            moveMenuSelection(b, queueIndex, downloads.size());
            if (b == Button::Options) emit(CommandType::ClearQueue);
            if (b == Button::Triangle) emit(CommandType::RefreshQueue);
            if ((b == Button::Cross || b == Button::Square) && !downloads.empty()) { detailScroll = 0; screen = Screen::QueueDetails; }
        } else if (!downloads.empty()) {
            if (b == Button::Up && detailScroll) --detailScroll;
            if (b == Button::Down) ++detailScroll;
            const auto& job = downloads[queueIndex];
            if (b == Button::Cross && (job.stage == QueueStage::Failed || job.stage == QueueStage::Cancelled)) {
                Command c{}; c.type = CommandType::RetryQueue; c.text = job.entry.url; commands.push_back(std::move(c));
            }
            if (b == Button::Triangle) {
                queueActionUrl = job.entry.url;
                removingQueueEntry = !queueActive(job.stage) && job.task < 0;
                screen = Screen::ConfirmCancelQueue;
            }
        }
        return true;
    }
    if (screen == Screen::Backups || screen == Screen::BackupFiles || screen == Screen::ConfirmRestore) {
        if (b == Button::Circle) { screen = screen == Screen::Backups ? Screen::Options : screen == Screen::BackupFiles ? Screen::Backups : Screen::BackupFiles; return true; }
        if (screen == Screen::ConfirmRestore) {
            if (b == Button::Cross) { Command c{}; c.type = CommandType::RestoreBackup; c.text = restoreDevice; c.previous = restoreFilename; commands.push_back(std::move(c)); }
            return true;
        }
        if (b == Button::Triangle) { emit(CommandType::ScanBackups); return true; }
        if (backupDevices.empty()) return true;
        backupIndex = std::min(backupIndex, backupDevices.size() - 1);
        if (screen == Screen::Backups) {
            moveMenuSelection(b, backupIndex, backupDevices.size());
            if (b == Button::Cross) { Command c{}; c.type = CommandType::ExportBackup; c.text = backupDevices[backupIndex].path; commands.push_back(std::move(c)); }
            if (b == Button::Square) { backupFileIndex = 0; screen = Screen::BackupFiles; }
        } else {
            const auto& files = backupDevices[backupIndex].files;
            if (files.empty()) return true;
            backupFileIndex = std::min(backupFileIndex, files.size() - 1);
            moveMenuSelection(b, backupFileIndex, files.size());
            if (b == Button::Cross) { restoreDevice = backupDevices[backupIndex].path; restoreFilename = files[backupFileIndex]; screen = Screen::ConfirmRestore; }
        }
        return true;
    }
    if (screen == Screen::SourceSetup) {
        if (b == Button::Circle) { screen = setupReturn; return true; }
        moveMenuSelection(b, setupIndex, 3);
        if (b == Button::Triangle || (b == Button::Cross && setupIndex == 0)) beginEdit(EditKind::SetupUrl, 0, setupSource.url);
        else if (b == Button::Square || (b == Button::Cross && setupIndex == 1)) { if (!sourceTesting) testSetupSource(); }
        else if (b == Button::Cross && setupIndex == 2) {
            if (!sourceReport.success || sourceReport.source.url != setupSource.url || sourceReport.source.category != setupSource.category) { featureNotice = "Test this URL successfully before using it"; return true; }
            if (setupSourceIndex < draft.sources.size()) draft.sources[setupSourceIndex] = setupSource;
            settingsNotice = "Source tested. Select Save settings to write config.json.";
            screen = Screen::Settings; settingsIndex = 10;
        }
        return true;
    }
    if (screen == Screen::Packages) {
        if (b == Button::Circle) { actionReturn = Screen::Dashboard; screen = Screen::Dashboard; return true; }
        const auto related = relatedPackages(available, packageTitle);
        if (related.empty()) return true;
        packageIndex = std::min(packageIndex, related.size() - 1);
        moveMenuSelection(b, packageIndex, related.size());
        if (b == Button::Cross || b == Button::Square || b == Button::Triangle) {
            pending = available[related[packageIndex]];
            if (b == Button::Cross) {
                if (pending.queued) { for (size_t i = 0; i < downloads.size(); ++i) if (downloads[i].entry.url == pending.url) queueIndex = i; featureReturn = Screen::Packages; screen = Screen::Queue; }
                else { installReturn = Screen::Packages; screen = Screen::ConfirmInstall; }
            } else if (b == Button::Square) { openDetails(Screen::Packages); }
            else { actionReturn = Screen::Packages; menuIndex = 0; screen = Screen::Actions; }
        }
        return true;
    }
    return false;
}
}
