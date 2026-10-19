#include "ui.hpp"
#include "metadata.hpp"
#include "sfo.hpp"
#include <algorithm>
#include <map>
#include <stdexcept>

namespace psgi {
void moveMenuSelection(Button button, size_t& selection, size_t count, size_t first) {
    if (!count) { selection = first; return; }
    const auto last = first + count - 1;
    selection = std::clamp(selection, first, last);
    if (button == Button::Up) selection = selection == first ? last : selection - 1;
    if (button == Button::Down) selection = selection == last ? first : selection + 1;
}
std::string layoutName(Layout layout) { return layout == Layout::Tiles ? "Tiles" : "Summary list"; }
std::string groupName(Grouping group) {
    const char* names[] = {"None", "Catalog category", "Region", "First letter", "Local category", "Local tag", "Game / Title ID"};
    return names[static_cast<int>(group)];
}
size_t keyboardMaximum(EditKind kind) {
    if (kind == EditKind::ArchiveEmail) return 254;
    if (kind == EditKind::ArchivePassword) return 1024;
    if (kind == EditKind::SourceUrl || kind == EditKind::SetupUrl) return 2047;
    if (kind == EditKind::SourceCategory) return 32;
    if (kind == EditKind::Search || kind == EditKind::SavedQuery) return 128;
    if (kind == EditKind::GameDescription) return MaxLocalDescription;
    return MaxLocalName;
}
std::string keyboardTitle(EditKind kind) {
    switch (kind) {
        case EditKind::ArchiveEmail: return "Archive.org email";
        case EditKind::ArchivePassword: return "Archive.org password";
        case EditKind::SourceUrl: return "Games JSON URL";
        case EditKind::SetupUrl: return "Test a games JSON URL";
        case EditKind::SavedName: return "Saved collection name";
        case EditKind::SavedQuery: return "Collection search words";
        case EditKind::SourceCategory: return "Catalog source category";
        case EditKind::Search: return "Search library";
        case EditKind::ProfileName: return "New server profile name";
        case EditKind::RenameProfile: return "Rename server profile";
        case EditKind::GameDescription: return "Local game description";
        case EditKind::GameTag: case EditKind::CreateTag: return "New local tag";
        case EditKind::RenameTag: return "Rename local tag";
        case EditKind::RenameCategory: return "Rename local category";
        default: return "New local category";
    }
}
size_t Ui::position() const { return selected[static_cast<int>(library)]; }
const std::vector<Entry>& Ui::entries() const { return library == Library::Installed ? installed : available; }
const Entry* Ui::current() const { return visible.empty() ? nullptr : &entries()[visible[position()].index]; }
std::string Ui::selectionKey() const {
    auto* e = current(); return e ? (e->url.empty() ? e->titleId : e->url) : "";
}
void Ui::emit(CommandType type, const Entry& entry) {
    Command command{type, entry, 0, ""};
    if (type == CommandType::Save) command.configuration = screen == Screen::Settings ? draft : settings;
    commands.push_back(std::move(command));
}
bool Ui::take(Command& out) {
    if (commands.empty()) return false;
    out = std::move(commands.front()); commands.pop_front(); return true;
}
void Ui::rebuild() {
    std::map<std::string,std::string> genreMap;
    for(const auto& entry:available)if(packageKind(entry)==0)for(const auto& name:entry.igdb.genres)genreMap.emplace(lower(name),name);
    genreNames.clear(); for(const auto& genre:genreMap)genreNames.push_back(genre.second);
    matchInstalled(available, installed, queued);
    std::set<std::string> queuedTitles;
    for (const auto& e : available) if (e.queued && !e.titleId.empty()) {
        const auto* job = findJob(downloads, e.url);
        // Verification after task release is bookkeeping, not a running installation.
        if (!job || job->stage != QueueStage::Verifying || job->task >= 0) queuedTitles.insert(e.titleId);
    }
    for (auto& e : installed) e.queued = queuedTitles.count(e.titleId) != 0;
    // Index once so large remote catalogs do not multiply installed-library work.
    std::map<std::string, const Entry*> baseTitles;
    std::map<std::string, std::string> latestUpdates;
    for (const auto& e : available) if (packageKind(e) == 1 && compareVersions(e.version, e.version)) {
        auto& latest = latestUpdates[e.titleId]; const auto newer = compareVersions(e.version, latest);
        if (latest.empty() || (newer && *newer > 0)) latest = e.version;
    }
    for (const auto& e : available) if (!e.titleId.empty() && packageKind(e) == 0) {
        auto found = baseTitles.find(e.titleId);
        if (found == baseTitles.end() || e.category == "games") baseTitles[e.titleId] = &e;
    }
    for (auto& local : installed) {
        local.installedVersion = local.version; local.baseInstalled = true; local.latestVersion = latestUpdates[local.titleId];
        auto found = baseTitles.find(local.titleId);
        if (found != baseTitles.end()) {
            const auto& remote = *found->second;
            local.coverUrl = remote.coverUrl; local.url = remote.url; local.size = remote.size;
            local.release = remote.release; local.minFirmware = remote.minFirmware;
            local.description = remote.description; local.igdb = remote.igdb; local.igdbJson = remote.igdbJson;
            if (!remote.category.empty()) local.category = remote.category;
            if (local.region.empty()) local.region = remote.region;
        }
    }
    player.apply(installed); player.apply(available); player.apply(pending);
    visible = makeView(entries(), settings, library, query, filter, updatesOnly && library == Library::Available, remoteGenre);
    uniqueShown = installable = 0; std::vector<bool> seen(entries().size());
    for (const auto& item : visible) if (!seen[item.index]) {
        seen[item.index] = true; ++uniqueShown;
        if (!entries()[item.index].installed && !entries()[item.index].queued) ++installable;
    }
    selected[static_cast<int>(library)] = visible.empty() ? 0 : std::min(position(), visible.size() - 1);
    grid.clear(); gridRowByPosition.clear();
    for (size_t i = 0; i < visible.size(); ++i) {
        if (grid.empty() || grid.back().items.size() == 4 || grid.back().group != visible[i].group)
            grid.push_back({visible[i].group, {}});
        grid.back().items.push_back(i); gridRowByPosition.push_back(grid.size() - 1);
    }
}
void Ui::setLibraries(std::optional<std::vector<Entry>> local, std::optional<std::vector<Entry>> remote) {
    auto state = captureView();
    if (local) installed = std::move(*local);
    if (remote) available = std::move(*remote);
    restoreView(state);
}
ViewState Ui::captureView() const {
    ViewState state; state.available = library == Library::Available; state.query = query; state.sidebar = sidebar;
    state.recentSearches = recentSearches;
    state.filter = filter; state.updatesOnly = updatesOnly; state.sidebarIndex = updatesOnly ? 2 : size_t(library);
    state.genre = remoteGenre; state.genresExpanded = genresExpanded;
    for (size_t i = 0; i < 2; ++i) {
        const auto which = i ? Library::Available : Library::Installed;
        const auto& list = i ? available : installed;
        const auto view = makeView(list, settings, which, query, filter, updatesOnly && i, remoteGenre);
        auto& cursor = state.cursor[i]; cursor.index = selected[i];
        if (!view.empty()) {
            const auto& entry = list[view[std::min(selected[i], view.size() - 1)].index];
            if (validTitleId(entry.titleId)) cursor.titleId = entry.titleId;
            if (i) cursor.url = entry.url;
            if (settings.grouping == Grouping::Tag || settings.grouping == Grouping::LocalCategory) cursor.group = view[std::min(selected[i], view.size() - 1)].group;
        }
    }
    return state;
}
void Ui::restoreView(const ViewState& state) {
    library = state.available ? Library::Available : Library::Installed; query = state.query; sidebar = state.sidebar;
    recentSearches = state.recentSearches;
    filter = state.filter; updatesOnly = state.updatesOnly; sidebarIndex = state.sidebarIndex < 3 ? state.sidebarIndex : updatesOnly ? 2 : size_t(library);
    remoteGenre = state.genre; genresExpanded = state.genresExpanded;
    rebuild();
    if (!remoteGenre.empty() && std::find(genreNames.begin(),genreNames.end(),remoteGenre)==genreNames.end()) { remoteGenre.clear(); rebuild(); }
    sidebarIndex = activeSidebarIndex();
    for (size_t i = 0; i < 2; ++i) {
        const auto& list = i ? available : installed;
        const auto view = makeView(list, settings, i ? Library::Available : Library::Installed, query, filter, updatesOnly && i, remoteGenre);
        const auto& cursor = state.cursor[i];
        selected[i] = view.empty() ? 0 : std::min(cursor.index, view.size() - 1);
        std::optional<size_t> fallback;
        for (size_t p = 0; p < view.size(); ++p) {
            const auto& entry = list[view[p].index];
            const bool title = !cursor.titleId.empty() && entry.titleId == cursor.titleId;
            const bool url = !cursor.url.empty() && entry.url == cursor.url;
            if (title && (!fallback || (!cursor.group.empty() && cursor.group == view[p].group))) fallback = p;
            if (((title && (!i || cursor.url.empty() || url)) || (cursor.titleId.empty() && url)) &&
                (cursor.group.empty() || cursor.group == view[p].group)) { fallback = p; break; }
        }
        if (fallback) selected[i] = *fallback;
    }
}
std::string Ui::librarySummary() const {
    if (library == Library::Installed)
        return std::to_string(installed.size()) + " installed games" +
            (uniqueShown != installed.size() ? " / " + std::to_string(uniqueShown) + " shown" : "");
    if (updatesOnly) return std::to_string(uniqueShown) + " updates / DLC";
    return std::to_string(installable) + " available to install / " + std::to_string(uniqueShown) + (remoteGenre.empty() ? " games" : " games in " + remoteGenre);
}
void Ui::beginEdit(EditKind kind, size_t index, const std::string& text) {
    settingsNotice.clear();
    editKind = kind; editSource = index; keyboardText = text;
    keyboardReturn = screen; screen = Screen::Keyboard;
    commands.push_back({CommandType::Keyboard, {}, index, text});
}
void Ui::finishEdit(const std::optional<std::string>& text) {
    screen = keyboardReturn;
    if (editKind == EditKind::ArchiveEmail || editKind == EditKind::ArchivePassword) {
        if (text) {
            if (text->size() > keyboardMaximum(editKind)) archiveNotice = "Account entry is too long. Please enter it again.";
            else {
                if (editKind == EditKind::ArchiveEmail) archiveEmail = *text;
                else archivePassword = *text;
                archiveNotice.clear();
            }
        } else archivePassword.clear();
        keyboardText.clear();
        return;
    }
    if (!text) return;
    if (editKind == EditKind::ProfileName || editKind == EditKind::RenameProfile) {
        try {
            if (editKind == EditKind::ProfileName) { addServerProfile(draft,*text); profileIndex=draft.profiles.size()-1; }
            else renameServerProfile(draft,editSource,*text);
            browseNotice = "Profile updated in draft. Return to Settings and Save settings to keep it.";
        } catch (const std::exception& e) { browseNotice=e.what(); }
        return;
    }
    if (editKind == EditKind::Search) { applySearch(*text); return; }
    if (editKind == EditKind::SavedName || editKind == EditKind::SavedQuery || editKind == EditKind::SetupUrl) {
        try {
            if (editKind == EditKind::SavedName) savedDraft.name = localName(*text);
            else if (editKind == EditKind::SavedQuery) savedDraft.query = text->substr(0, 128);
            else {
                if (!isHttpUrl(*text)) throw std::runtime_error("Use a full http:// or https:// games JSON URL");
                setupSource.url = *text; setupIndex = 1; testSetupSource();
            }
            featureNotice.clear();
        } catch (const std::exception& e) { featureNotice = e.what(); }
        return;
    }
    if (editKind >= EditKind::GameCategory) {
        try {
            if (editKind == EditKind::GameDescription) { auto next = localDraft; next.description = *text; localDraft = normalizeInfo(next); }
            else if (editKind == EditKind::GameCategory) localDraft.category = localName(*text);
            else if (editKind == EditKind::GameTag) { auto next = localDraft; next.tags.push_back(*text); localDraft = normalizeInfo(next); }
            else {
                auto name = localName(*text);
                if (name.empty()) throw std::runtime_error("Enter a category or tag name");
                Command command{}; command.type = editKind == EditKind::RenameCategory || editKind == EditKind::RenameTag ? CommandType::RenameCollection : CommandType::CreateCollection;
                command.source = editKind == EditKind::CreateTag || editKind == EditKind::RenameTag;
                command.text = name; command.previous = collectionOriginal; commands.push_back(std::move(command));
            }
            localNotice = quickAssignment ? "" : "Draft updated. Select Save local info to keep it.";
            if (quickAssignment && (editKind == EditKind::GameCategory || editKind == EditKind::GameTag)) {
                const bool tags = editKind == EditKind::GameTag;
                const auto choices = termChoices(tags, true);
                const auto name = localName(*text);
                const auto found = std::find_if(choices.begin(), choices.end(), [&](const std::string& term) { return lower(term) == lower(name); });
                pickerIndex = found == choices.end() ? 0 : 1 + (found - choices.begin());
            }
        } catch (const std::exception& e) { localNotice = collectionNotice = status = e.what(); }
        return;
    }
    if (editKind == EditKind::SourceUrl) {
        if (!text->empty() && !isHttpUrl(*text)) { settingsNotice = status = "Invalid URL. Use http:// or https:// with no spaces."; return; }
        draft.sources.at(editSource).url = *text;
        settingsNotice = "URL accepted. Select Save settings to write config.json.";
    } else {
        std::string category = lower(*text);
        if (category.empty() || category.size() > 32 || category == "ps5" || category.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_- ") != std::string::npos) {
            status = "Use a short PS4 category name (letters, numbers, spaces, - or _)."; return;
        }
        for (const auto& source : draft.sources) if (source.category == category) { status = "That category already exists."; return; }
        draft.sources.push_back({category, ""});
        sourceIndex = draft.sources.size() - 1;
        beginEdit(EditKind::SourceUrl, sourceIndex, "");
    }
}
std::vector<std::string> Ui::options() const {
    return {"Refresh library", "Search" + (query.empty() ? std::string() : ": " + query),
        "View: " + layoutName(settings.layout), "Group: " + groupName(settings.grouping),
        "Sort: " + sortName(settings.sort),
        settings.notInstalledOnly ? "Available: Not installed" : "Available: All packages", "Settings", "About", "Exit", "Browse local categories", "Browse local tags",
        bulkMode ? "Organize selected games" : "Select multiple games", "Downloads", "Collections", "USB backup / restore", "Connection setup helper", settings.descending ? "Order: Descending" : "Order: Ascending", "Storage cleanup", "Export diagnostics to USB", "Jump to letter", "Server profiles"};
}
std::vector<std::string> Ui::settingRows() const {
    return {"Games JSON URL", "Other catalog sources", "View: " + layoutName(draft.layout),
        "Grouping: " + groupName(draft.grouping), "Sort: " + sortName(draft.sort),
        draft.notInstalledOnly ? "Available: Not installed" : "Available: All packages",
        draft.animations ? "Background animation: On" : "Background animation: Off",
        draft.refreshOnStart ? "Refresh on startup: On" : "Refresh on startup: Off",
        "Test connection", "Clear artwork cache", "Save settings", "Installation destination", "Connection setup helper", draft.descending ? "Order: Descending" : "Order: Ascending", "Storage cleanup", "Export diagnostics to USB", "Server profiles", "Archive.org account"};
}
std::vector<std::string> Ui::archiveRows() const {
    return {"Email: " + (archiveEmail.empty() ? "Not entered" : archiveEmail),
        std::string("Password: ") + (archivePassword.empty() ? "Not entered" : "********"),
        archiveBusy ? "Please wait..." : "Sign in", "Sign out"};
}
std::string Ui::keyboardDisplayText() const {
    return editKind == EditKind::ArchivePassword ? std::string(keyboardText.size(), '*') : keyboardText;
}
std::vector<std::string> Ui::actionRows() const {
    std::vector<std::string> rows;
    if (library == Library::Installed || pending.installed)
        rows = pending.url.empty() ? std::vector<std::string>{"Launch game", "Details", "Refresh artwork"} :
            std::vector<std::string>{"Launch game", "Install remote package", "Details", "Refresh artwork"};
    else rows = {"Install package", "Details", "Refresh artwork"};
    if(!pending.igdb.screenshots.empty())
        rows.insert(std::find(rows.begin(),rows.end(),"Details")+1,"Show screenshots");
    if (validTitleId(pending.titleId)) {
        rows.push_back("Assign category");
        rows.push_back("Assign tags");
        rows.push_back("Rate game" + (pending.rating ? " (" + std::to_string(pending.rating) + "/5)" : std::string()));
        rows.push_back("Edit local info");
    }
    if (pending.installed) rows.push_back("Uninstall");
    if (validTitleId(pending.titleId) && !relatedPackages(available, pending.titleId).empty()) rows.push_back("Game packages");
    return rows;
}
std::vector<std::string> Ui::termChoices(bool tags, bool draft) const {
    auto terms = tags ? player.tags : player.categories;
    auto add = [&](const std::string& value) {
        if (!value.empty() && std::none_of(terms.begin(), terms.end(), [&](const std::string& term) { return lower(term) == lower(value); })) terms.push_back(value);
    };
    if (draft) { if (tags) { for (const auto& tag : localDraft.tags) add(tag); } else add(localDraft.category); }
    return terms;
}
std::vector<std::string> Ui::localRows() const {
    return {"Category: " + (localDraft.category.empty() ? "Uncategorized" : localDraft.category),
        "Tags: " + (localDraft.tags.empty() ? "Untagged" : joinedTags(localDraft.tags)),
        "Description: " + (localDraft.description.empty() ? "Not set" : localDraft.description), "Save local info"};
}
std::string Ui::filterDescription() const {
    std::string value;
    if (!filter.category.empty() || filter.uncategorized) value = "Category: " + (filter.uncategorized ? std::string("Uncategorized") : filter.category);
    if (!filter.tag.empty() || filter.untagged) { if (!value.empty()) value += " / "; value += "Tag: " + (filter.untagged ? std::string("Untagged") : filter.tag); }
    auto add = [&](const std::string& text) { if (!value.empty()) value += " / "; value += text; };
    if (filter.minRating || filter.maxRating != 5) add(std::to_string(filter.minRating) + "-" + std::to_string(filter.maxRating) + " stars");
    if (filter.launchState) add(filter.launchState == 1 ? "No recorded launches" : "Launched here");
    if (filter.installState) { const char* names[] = {"", "Installed", "Not installed", "Was installed"}; add(names[filter.installState]); }
    return value;
}
std::vector<std::string> Ui::collectionRows() const {
    std::map<std::string, size_t> counts; size_t empty = 0, total = 0;
    for (const auto& entry : entries()) {
        if (library == Library::Available && settings.notInstalledOnly && entry.installed) continue;
        ++total;
        if (collectionTags) {
            if (entry.info.tags.empty()) ++empty;
            for (const auto& tag : entry.info.tags) ++counts[lower(tag)];
        } else if (entry.info.category.empty()) ++empty;
        else ++counts[lower(entry.info.category)];
    }
    std::vector<std::string> rows{"All games / clear local filters (" + std::to_string(total) + ")",
        std::string(collectionTags ? "Untagged" : "Uncategorized") + " (" + std::to_string(empty) + ")"};
    for (const auto& name : termChoices(collectionTags)) rows.push_back(name + " (" + std::to_string(counts[lower(name)]) + ")");
    return rows;
}
void Ui::quickSave() { rebuild(); emit(CommandType::Save); }
void Ui::saveLocalDraft() {
    Command command{}; command.type = bulkEditing ? CommandType::BulkAssign : CommandType::SaveLocalInfo;
    if (bulkEditing) { command.titles = markedTitles; command.source = bulkOperation; }
    command.entry = pending; command.personal = localDraft; command.returnToActions = quickAssignment;
    commands.push_back(std::move(command));
}
void Ui::handle(Button b) {
    if (handleDetails(b)) return;
    // Circle and dialogs are handled before dashboard shortcuts.
    if (screen == Screen::Keyboard) return;
    if (screen == Screen::ArchiveAccount) {
        if (b == Button::Circle) {
            archivePassword.clear(); keyboardText.clear(); screen = Screen::Settings; return;
        }
        moveMenuSelection(b, archiveIndex, archiveRows().size());
        if (b != Button::Cross || archiveBusy) return;
        if (archiveIndex == 0) beginEdit(EditKind::ArchiveEmail, 0, archiveEmail);
        else if (archiveIndex == 1) {
            archivePassword.clear(); beginEdit(EditKind::ArchivePassword, 0, "");
        } else if (archiveIndex == 2) {
            if (archiveEmail.empty() || archivePassword.empty()) {
                archiveNotice = "Enter your Archive.org email and password first."; return;
            }
            Command command{}; command.type = CommandType::ArchiveSignIn;
            command.archiveEmail = archiveEmail; command.archivePassword = std::move(archivePassword);
            commands.push_back(std::move(command));
            archivePassword.clear(); keyboardText.clear(); archiveBusy = true; archiveNotice = "Signing in...";
        } else {
            archivePassword.clear(); keyboardText.clear(); archiveBusy = true; archiveNotice = "Signing out...";
            emit(CommandType::ArchiveSignOut);
        }
        return;
    }
    if (handleBrowsing(b)) return;
    if (handleFeatures(b)) return;
    if (screen == Screen::DiscardLocal) {
        if (b == Button::Circle) { localDraft = pending.info; screen = Screen::Actions; }
        else if (b == Button::Cross) screen = Screen::LocalInfo;
        return;
    }
    if (screen == Screen::ConfirmDeleteCollection) {
        if (b == Button::Circle) screen = Screen::Collections;
        else if (b == Button::Cross) {
            Command command{}; command.type = CommandType::DeleteCollection; command.source = collectionTags; command.text = collectionOriginal;
            commands.push_back(std::move(command)); screen = Screen::Collections;
        }
        return;
    }
    if (screen == Screen::LocalInfo) {
        if (b == Button::Circle) { screen = localDirty() ? Screen::DiscardLocal : Screen::Actions; return; }
        moveMenuSelection(b, localIndex, localRows().size());
        if (b == Button::Cross) switch (localIndex) {
            case 0: pickerIndex = 0; screen = Screen::PickCategory; break;
            case 1: pickerIndex = 0; screen = Screen::PickTags; break;
            case 2: beginEdit(EditKind::GameDescription, 0, localDraft.description); break;
            case 3: saveLocalDraft(); break;
        }
        return;
    }
    if (screen == Screen::PickCategory || screen == Screen::PickTags) {
        const bool tags = screen == Screen::PickTags;
        const auto choices = termChoices(tags, true);
        pickerIndex = std::min(pickerIndex, choices.size());
        if (b == Button::Circle) {
            if (quickAssignment) { localDraft = pending.info; quickAssignment = false; screen = bulkEditing ? Screen::BulkMenu : Screen::Actions; bulkEditing = false; }
            else screen = Screen::LocalInfo;
            return;
        }
        if (quickAssignment && tags && b == Button::Square) { saveLocalDraft(); return; }
        if (b == Button::Triangle) { beginEdit(tags ? EditKind::GameTag : EditKind::GameCategory, 0, ""); return; }
        moveMenuSelection(b, pickerIndex, choices.size() + 1);
        if (b == Button::Cross) {
            localNotice.clear();
            if (!pickerIndex) { if (tags) localDraft.tags.clear(); else localDraft.category.clear(); }
            else {
                const auto& value = choices.at(pickerIndex - 1);
                if (!tags) localDraft.category = value;
                else {
                    auto found = std::find_if(localDraft.tags.begin(), localDraft.tags.end(), [&](const std::string& tag) { return lower(tag) == lower(value); });
                    if (found != localDraft.tags.end()) localDraft.tags.erase(found);
                    else if (localDraft.tags.size() < MaxGameTags) localDraft.tags.push_back(value);
                    else localNotice = status = "Maximum 16 tags per game";
                }
            }
            pickerIndex = std::min(pickerIndex, termChoices(tags, true).size());
            if (!tags) { if (quickAssignment) saveLocalDraft(); else screen = Screen::LocalInfo; }
        }
        return;
    }
    if (screen == Screen::Collections) {
        if (b == Button::Circle) { screen = Screen::Options; return; }
        if (b == Button::L1 || b == Button::R1) { collectionTags = !collectionTags; collectionIndex = 0; collectionNotice.clear(); return; }
        const auto choices = termChoices(collectionTags);
        collectionIndex = std::min(collectionIndex, choices.size() + 1);
        moveMenuSelection(b, collectionIndex, choices.size() + 2);
        if (b == Button::Triangle) { beginEdit(collectionTags ? EditKind::CreateTag : EditKind::CreateCategory, 0, ""); return; }
        if (collectionIndex >= 2) {
            collectionOriginal = choices.at(collectionIndex - 2);
            if (b == Button::Square) { beginEdit(collectionTags ? EditKind::RenameTag : EditKind::RenameCategory, 0, collectionOriginal); return; }
            if (b == Button::Options) { screen = Screen::ConfirmDeleteCollection; return; }
        }
        if (b == Button::Cross) {
            if (!collectionIndex) filter = {};
            else if (collectionTags) { filter.tag = collectionIndex == 1 ? "" : choices.at(collectionIndex - 2); filter.untagged = collectionIndex == 1; }
            else { filter.category = collectionIndex == 1 ? "" : choices.at(collectionIndex - 2); filter.uncategorized = collectionIndex == 1; }
            settings.grouping = collectionTags ? Grouping::Tag : Grouping::LocalCategory;
            selected[0] = selected[1] = 0; sidebar = false; screen = Screen::Dashboard; quickSave();
        }
        return;
    }
    if (screen == Screen::ConfirmExit) {
        if (b == Button::Circle) emit(CommandType::Quit);
        else if (b == Button::Cross || b == Button::Options) screen = Screen::Dashboard;
        return;
    }
    if (screen == Screen::Discard) {
        if (b == Button::Circle) { draft = settings; screen = Screen::Options; }
        else if (b == Button::Cross) screen = Screen::Settings;
        return;
    }
    if (screen == Screen::ConfirmInstall) {
        if (b == Button::Circle) screen = installReturn;
        else if (b == Button::Triangle) { packageTitle = pending.titleId; packageIndex = 0; screen = Screen::Packages; }
        else if (b == Button::Cross) {
            const auto required = requirements();
            if (!required.blockers.empty()) status = required.blockers.front();
            else { emit(CommandType::Install, pending); screen = installReturn; }
        }
        return;
    }
    if (screen == Screen::InstallError) {
        if (b == Button::Circle) { installError.clear(); screen = Screen::Dashboard; }
        else if (b == Button::Cross) { pending = failedInstall; installError.clear(); installReturn = Screen::Dashboard; screen = Screen::ConfirmInstall; }
        return;
    }
    if (screen == Screen::ConfirmUninstall) {
        if (b == Button::Circle) screen = Screen::Actions;
        else if (b == Button::Cross) { emit(CommandType::Uninstall, pending); screen = Screen::Dashboard; }
        return;
    }
    if (screen == Screen::Rating) {
        if (b == Button::Circle) { screen = Screen::Actions; return; }
        moveMenuSelection(b, ratingIndex, 6);
        if (b == Button::Cross) {
            commands.push_back({CommandType::Rate, pending, ratingIndex < 5 ? ratingIndex + 1 : 0, ""});
            screen = Screen::Actions;
        }
        return;
    }
    if (b == Button::Circle) {
        if (screen == Screen::Splash || screen == Screen::Dashboard) screen = Screen::ConfirmExit;
        else if (screen == Screen::Settings) screen = sameSettings(settings, draft) ? Screen::Options : Screen::Discard;
        else if (screen == Screen::Sources) screen = Screen::Settings;
        else if (screen == Screen::Storage) screen = Screen::Settings;
        else if (screen == Screen::Details) screen = detailReturn;
        else if (screen == Screen::About) screen = Screen::Options;
        else if (screen == Screen::Actions) screen = actionReturn;
        else screen = Screen::Dashboard;
        return;
    }
    if (screen == Screen::Splash) return;
    if (screen == Screen::About) return;
    if (screen == Screen::Storage) {
        if (b == Button::Triangle) emit(CommandType::Storage);
        moveMenuSelection(b, storageIndex, 2);
        if (b == Button::Cross) {
            if (!storageChecked || storage.destination == InstallDestination::Unknown) {
                status = "Check storage before changing the destination"; return;
            }
            if (!(storageIndex ? storage.external : storage.internal).available) {
                status = storageIndex ? "Connect a PS4 Extended Storage drive first" : "Internal storage is unavailable"; return;
            }
            commands.push_back({CommandType::SetStorage, {}, storageIndex, ""});
        }
        return;
    }

    if (screen == Screen::Options) {
        moveMenuSelection(b, menuIndex, options().size());
        if (b != Button::Cross) return;
        switch (menuIndex) {
            case 0: emit(CommandType::Refresh); screen = Screen::Dashboard; break;
            case 1: searchIndex=0; browseNotice.clear(); screen=Screen::Search; break;
            case 2: settings.layout = settings.layout == Layout::Tiles ? Layout::Summary : Layout::Tiles; quickSave(); break;
            case 3: settings.grouping = static_cast<Grouping>((static_cast<int>(settings.grouping) + 1) % 7); quickSave(); break;
            case 4: settings.sort = static_cast<SortField>((int(settings.sort) + 1) % 5); settings.descending = settings.sort != SortField::Name; quickSave(); break;
            case 5: settings.notInstalledOnly = !settings.notInstalledOnly; quickSave(); break;
            case 6: draft = settings; settingsNotice.clear(); settingsIndex = 0; screen = Screen::Settings; break;
            case 7: screen = Screen::About; break;
            case 8: screen = Screen::ConfirmExit; break;
            case 9: case 10: collectionTags = menuIndex == 10; collectionIndex = 0; collectionNotice.clear(); screen = Screen::Collections; break;
            case 11: bulkMode = true; bulkIndex = 0; featureNotice.clear(); screen = markedTitles.empty() ? Screen::Dashboard : Screen::BulkMenu; break;
            case 12: featureReturn = Screen::Options; screen = Screen::Queue; queueIndex = 0; featureNotice.clear(); break;
            case 13: featureReturn = Screen::Options; screen = Screen::SavedCollections; savedIndex = 0; featureNotice.clear(); break;
            case 14: screen = Screen::Backups; featureNotice.clear(); emit(CommandType::ScanBackups); break;
            case 15: draft = settings; setupSourceIndex = 0; setupSource = draft.sources[0]; setupReturn = Screen::Options; sourceReport = {}; featureNotice.clear(); setupIndex = 0; screen = Screen::SourceSetup; break;
            case 16: settings.descending = !settings.descending; quickSave(); break;
            case 17: maintenanceReturn = Screen::Options; screen = Screen::Cleanup; featureNotice.clear(); emit(CommandType::ScanUsage); break;
            case 19: jumpIndex=0; jumpReturn=Screen::Options; screen=Screen::JumpLetter; break;
            case 20: draft=settings; profileIndex=0; profileReturn=Screen::Options; browseNotice.clear(); screen=Screen::ServerProfiles; break;
            case 18: maintenanceReturn = Screen::Options; screen = Screen::Diagnostics; featureNotice.clear(); emit(CommandType::ScanBackups); break;
        }
        return;
    }
    if (screen == Screen::Settings) {
        moveMenuSelection(b, settingsIndex, settingRows().size());
        if (b != Button::Cross && b != Button::Left && b != Button::Right) return;
        if (b != Button::Cross && (settingsIndex < 2 || (settingsIndex > 7 && settingsIndex != 13))) return;
        settingsNotice.clear();
        switch (settingsIndex) {
            case 0: beginEdit(EditKind::SourceUrl, 0, draft.sources[0].url); break;
            case 1: sourceIndex = 1; screen = Screen::Sources; break;
            case 2: draft.layout = draft.layout == Layout::Tiles ? Layout::Summary : Layout::Tiles; break;
            case 3: draft.grouping = static_cast<Grouping>((static_cast<int>(draft.grouping) + 1) % 7); break;
            case 4: draft.sort = static_cast<SortField>((int(draft.sort) + (b == Button::Left ? 4 : 1)) % 5); draft.descending = draft.sort != SortField::Name; break;
            case 5: draft.notInstalledOnly = !draft.notInstalledOnly; break;
            case 6: draft.animations = !draft.animations; break;
            case 7: draft.refreshOnStart = !draft.refreshOnStart; break;
            case 8: emit(CommandType::Test); break;
            case 9: emit(CommandType::ClearArtwork); break;
            case 10: emit(CommandType::Save); break;
            case 11: screen = Screen::Storage; storageIndex = storage.destination == InstallDestination::External ? 1 : 0; emit(CommandType::Storage); break;
            case 12: setupSourceIndex = 0; setupSource = draft.sources[0]; setupReturn = Screen::Settings; sourceReport = {}; featureNotice.clear(); setupIndex = 0; screen = Screen::SourceSetup; break;
            case 13: draft.descending = !draft.descending; break;
            case 14: maintenanceReturn = Screen::Settings; screen = Screen::Cleanup; featureNotice.clear(); emit(CommandType::ScanUsage); break;
            case 16: profileIndex=0; profileReturn=Screen::Settings; browseNotice.clear(); screen=Screen::ServerProfiles; break;
            case 15: maintenanceReturn = Screen::Settings; screen = Screen::Diagnostics; featureNotice.clear(); emit(CommandType::ScanBackups); break;
            case 17: archiveIndex = 0; archivePassword.clear(); keyboardText.clear(); screen = Screen::ArchiveAccount; break;
        }
        return;
    }
    if (screen == Screen::Sources) {
        moveMenuSelection(b, sourceIndex, draft.sources.empty() ? 1 : draft.sources.size(), draft.sources.empty() ? 0 : 1);
        if (b == Button::Square && sourceIndex < draft.sources.size()) {
            setupSourceIndex = sourceIndex; setupSource = draft.sources[sourceIndex]; setupReturn = Screen::Sources;
            sourceReport = {}; featureNotice.clear(); setupIndex = 0; screen = Screen::SourceSetup; return;
        }
        if (b == Button::Triangle && sourceIndex < draft.sources.size()) {
            draft.sources.erase(draft.sources.begin() + sourceIndex); return;
        }
        if (b == Button::Cross) {
            if (sourceIndex == draft.sources.size()) {
                if (draft.sources.size() >= 32) { status = "Maximum 32 catalog sources"; return; }
                beginEdit(EditKind::SourceCategory, sourceIndex, "");
            } else beginEdit(EditKind::SourceUrl, sourceIndex, draft.sources[sourceIndex].url);
        }
        return;
    }
    if (screen == Screen::Actions) {
        auto actions = actionRows();
        moveMenuSelection(b, menuIndex, actions.size());
        if (b == Button::Cross) {
            auto action = actions[menuIndex];
            if (action == "Launch game") { emit(CommandType::Launch, pending); screen = Screen::Dashboard; }
            else if (action.find("Install") == 0) { installReturn = actionReturn == Screen::Packages ? Screen::Packages : Screen::Dashboard; screen = Screen::ConfirmInstall; }
            else if (action == "Uninstall") {
                if (pending.queued) status = "Finish or cancel this title's queued download before uninstalling";
                else screen = Screen::ConfirmUninstall;
            }
            else if (action.find("Rate game") == 0) { ratingIndex = pending.rating ? pending.rating - 1 : 0; screen = Screen::Rating; }
            else if (action == "Assign category" || action == "Assign tags") {
                beginAssignment(action == "Assign tags");
            }
            else if (action == "Game packages") { packageTitle = pending.titleId; packageIndex = 0; screen = Screen::Packages; }
            else if (action == "Edit local info") { localDraft = pending.info; quickAssignment = false; localIndex = 0; localNotice.clear(); screen = Screen::LocalInfo; }
            else if (action == "Details") { openDetails(Screen::Actions); }
            else if (action == "Show screenshots") { mediaIndex=0; mediaReturn=Screen::Actions; screen=Screen::Screenshots; }
            else { emit(CommandType::RefreshArtwork, pending); screen = Screen::Dashboard; }
        }
        return;
    }
    if (b == Button::Options) { menuIndex = 0; screen = Screen::Options; return; }
    if (b == Button::L1 || b == Button::R1) {
        library = library == Library::Installed ? Library::Available : Library::Installed; updatesOnly = false; remoteGenre.clear(); sidebarIndex = int(library); rebuild(); return;
    }
    if (sidebar) {
        if (b == Button::Left && (sidebarIndex == 1 || !remoteGenre.empty())) {
            genresExpanded = false; remoteGenre.clear(); sidebarIndex = 1;
        } else if (b == Button::Right && sidebarIndex == 1 && !genresExpanded && !genres().empty()) {
            genresExpanded = true;
        } else {
            const auto rows=sidebarRows();
            moveMenuSelection(b,sidebarIndex,rows.size());
            if (b == Button::Cross && sidebarIndex == 1 && !genres().empty()) genresExpanded = true;
            if (b == Button::Right || b == Button::Cross) sidebar = false;
        }
        const auto rows=sidebarRows(); sidebarIndex=std::min(sidebarIndex,rows.size()-1);
        const auto row=rows[sidebarIndex];
        library = row.kind ? Library::Available : Library::Installed; updatesOnly = row.kind == 2; remoteGenre = row.genre;
        selected[int(library)]=0; rebuild(); return;
    }
    if (auto* e = current()) {
        if (b == Button::Cross) {
            pending = *e;
            if (library == Library::Installed) {
                if (pending.queued) status = "This title is still queued. Check PS4 Downloads before launching.";
                else emit(CommandType::Launch, pending);
            }
            else if (pending.installed) { menuIndex = 0; screen = Screen::Actions; }
            else if (pending.queued) status = "This package is already queued. See PS4 Notifications > Downloads.";
            else { installReturn = Screen::Dashboard; screen = Screen::ConfirmInstall; }
        } else if (b == Button::Triangle) { pending = *e; menuIndex = 0; actionReturn = Screen::Dashboard; screen = Screen::Actions; }
        else if (b == Button::Square) { pending = *e; openDetails(Screen::Dashboard); }
        else if (settings.layout == Layout::Summary) {
            moveMenuSelection(b, selected[static_cast<int>(library)], visible.size());
            if (b == Button::Left) { sidebar = true; sidebarIndex = activeSidebarIndex(); }
        } else {
            const auto& rows = tileRows();
            size_t row = tileRowIndex(), col = 0;
            for (size_t c = 0; c < rows[row].items.size(); ++c) if (rows[row].items[c] == position()) col = c;
            if (b == Button::Left) {
                if (col == 0) { sidebar = true; sidebarIndex = activeSidebarIndex(); } else --selected[static_cast<int>(library)];
            }
            if (b == Button::Right && col + 1 < rows[row].items.size()) ++selected[static_cast<int>(library)];
            if (b == Button::Up || b == Button::Down) {
                moveMenuSelection(b, row, rows.size());
                selected[static_cast<int>(library)] = rows[row].items[std::min(col, rows[row].items.size()-1)];
            }
        }
    } else if (b == Button::Left) { sidebar = true; sidebarIndex = activeSidebarIndex(); }
}
}
