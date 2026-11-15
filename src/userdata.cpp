#include "userdata.hpp"
#include "text.hpp"
#include "settings.hpp"
#include "sfo.hpp"
#include "json.hpp"
#include <cmath>
#include <algorithm>
#include <cerrno>
#include <memory>
#include <stdexcept>
#include <unistd.h>

namespace psgi {
namespace {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
Json parse(const std::string& text, unsigned maxSchema = 1) {
    if (text.size() > MaxCatalogBytes || text.find('\0') != std::string::npos) throw std::runtime_error("Invalid local data");
    Json root(cJSON_ParseWithLengthOpts(text.c_str(), text.size() + 1, nullptr, true), cJSON_Delete);
    if (!root || !cJSON_IsObject(root.get())) throw std::runtime_error("Invalid local data JSON");
    auto* schema = cJSON_GetObjectItemCaseSensitive(root.get(), "schema");
    if (!cJSON_IsNumber(schema) || schema->valuedouble < 1 || schema->valuedouble > maxSchema ||
        std::floor(schema->valuedouble) != schema->valuedouble) throw std::runtime_error("Unsupported local data schema");
    return root;
}
uint64_t number(const cJSON* object, const char* key, uint64_t limit = 9007199254740991ULL) {
    auto* field = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!field) return 0;
    if (!cJSON_IsNumber(field) || !std::isfinite(field->valuedouble) || field->valuedouble < 0 ||
        field->valuedouble > double(limit) || std::floor(field->valuedouble) != field->valuedouble)
        throw std::runtime_error(std::string("Invalid local data field: ") + key);
    return uint64_t(field->valuedouble);
}
bool boolean(const cJSON* object, const char* key) {
    auto* field = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!field) return false;
    if (!cJSON_IsBool(field)) throw std::runtime_error(std::string("Invalid local data field: ") + key);
    return cJSON_IsTrue(field);
}
std::string string(const cJSON* object, const char* key, size_t limit) {
    auto* field = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!field) return "";
    if (!cJSON_IsString(field) || std::string(field->valuestring).size() > limit)
        throw std::runtime_error(std::string("Invalid local data field: ") + key);
    return field->valuestring;
}
std::string print(const cJSON* root) {
    std::unique_ptr<char, decltype(&cJSON_free)> text(printJson(root), cJSON_free);
    if (!text) throw std::runtime_error("Cannot serialize local data");
    return text.get();
}
void requireId(const std::string& id) { if (!validTitleId(id)) throw std::runtime_error("This game needs a valid Title ID for local records"); }
bool exists(const std::string& path) {
    if (!access(path.c_str(), F_OK)) return true;
    if (errno == ENOENT) return false;
    throw std::runtime_error("Cannot inspect local data file: " + path);
}
std::string canonical(std::vector<std::string>& terms, const std::string& name) {
    for (const auto& term : terms) if (lower(term) == lower(name)) return term;
    if (terms.size() >= MaxLocalTerms) throw std::runtime_error("Maximum 256 local categories or tags");
    terms.push_back(name);
    std::sort(terms.begin(), terms.end(), [](const std::string& a, const std::string& b) { return lower(a) < lower(b); });
    return name;
}
std::vector<std::string> terms(const cJSON* object, const char* key, size_t maximum) {
    auto* field = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!field) return {};
    if (!cJSON_IsArray(field)) throw std::runtime_error(std::string("Invalid local list: ") + key);
    std::vector<std::string> out;
    for (auto* item = field->child; item; item = item->next) {
        if (!cJSON_IsString(item) || out.size() >= maximum) throw std::runtime_error("Invalid local category/tag list");
        auto name = localName(item->valuestring);
        if (name.empty()) throw std::runtime_error("Empty local category/tag name");
        for (const auto& existing : out) if (lower(existing) == lower(name)) throw std::runtime_error("Duplicate local category/tag name");
        out.push_back(name);
    }
    return out;
}
void array(cJSON* object, const char* key, const std::vector<std::string>& values) {
    auto* field = cJSON_AddArrayToObject(object, key);
    for (const auto& value : values) cJSON_AddItemToArray(field, cJSON_CreateString(value.c_str()));
}
LocalFilter readFilter(const cJSON* object) {
    if (!cJSON_IsObject(object)) throw std::runtime_error("Invalid local view filter");
    LocalFilter f;
    f.category = localName(string(object, "category", MaxLocalName)); f.tag = localName(string(object, "tag", MaxLocalName));
    f.uncategorized = boolean(object, "uncategorized"); f.untagged = boolean(object, "untagged");
    f.minRating = number(object, "min_rating", 5);
    f.maxRating = cJSON_GetObjectItemCaseSensitive(object, "max_rating") ? number(object, "max_rating", 5) : 5;
    f.launchState = number(object, "launch_state", 2); f.installState = number(object, "install_state", 3);
    if (f.minRating > f.maxRating || (f.uncategorized && !f.category.empty()) || (f.untagged && !f.tag.empty()))
        throw std::runtime_error("Collection filters conflict");
    return f;
}
void writeFilter(cJSON* object, const LocalFilter& f) {
    cJSON_AddStringToObject(object, "category", f.category.c_str()); cJSON_AddStringToObject(object, "tag", f.tag.c_str());
    cJSON_AddBoolToObject(object, "uncategorized", f.uncategorized); cJSON_AddBoolToObject(object, "untagged", f.untagged);
    cJSON_AddNumberToObject(object, "min_rating", f.minRating); cJSON_AddNumberToObject(object, "max_rating", f.maxRating);
    cJSON_AddNumberToObject(object, "launch_state", f.launchState); cJSON_AddNumberToObject(object, "install_state", f.installState);
    readFilter(object);
}
}
std::string localName(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n"), last = text.find_last_not_of(" \t\r\n");
    const auto name = first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
    if (name.size() > MaxLocalName) throw std::runtime_error("Local names must fit in 64 UTF-8 bytes");
    for (unsigned char c : name) if (c < 32 || c == 127) throw std::runtime_error("Local names cannot contain control characters");
    return name;
}
LocalInfo normalizeInfo(LocalInfo info) {
    info.category = localName(info.category);
    if (info.description.size() > MaxLocalDescription) throw std::runtime_error("Local description exceeds 2047 UTF-8 bytes");
    for (unsigned char c : info.description) if ((c < 32 && c != '\n' && c != '\r' && c != '\t') || c == 127)
        throw std::runtime_error("Invalid character in local description");
    std::vector<std::string> names;
    for (const auto& tag : info.tags) {
        auto name = localName(tag);
        if (name.empty()) throw std::runtime_error("Tag name cannot be empty");
        canonical(names, name);
    }
    if (names.size() > MaxGameTags) throw std::runtime_error("Maximum 16 tags per game");
    info.tags = std::move(names); return info;
}
bool sameInfo(const LocalInfo& a, const LocalInfo& b) {
    return a.category == b.category && a.description == b.description && a.tags == b.tags;
}
std::string joinedTags(const std::vector<std::string>& tags) {
    std::string text;
    for (const auto& tag : tags) { if (!text.empty()) text += ", "; text += tag; }
    return text;
}
bool PlayerData::observeInstalled(const std::vector<Entry>& entries, uint64_t now) {
    bool changed = false;
    for (const auto& entry : entries) if (validTitleId(entry.titleId)) {
        auto& record = games[entry.titleId];
        if (!record.wasInstalled) { record.wasInstalled = true; record.firstSeenInstalled = now; changed = true; }
    }
    return changed;
}
void PlayerData::rate(const std::string& id, unsigned stars) {
    requireId(id);
    if (stars > 5) throw std::runtime_error("Choose a rating from 1 to 5 stars");
    games[id].rating = stars;
}
void PlayerData::noteLaunch(const std::string& id, uint64_t now) {
    requireId(id); auto& record = games[id];
    if (record.launches >= 9007199254740991ULL) throw std::runtime_error("Launch counter is full");
    ++record.launches; record.lastLaunch = now;
}
void PlayerData::setInfo(const std::string& id, LocalInfo info) {
    requireId(id); info = normalizeInfo(std::move(info));
    auto nextCategories = categories, nextTags = tags;
    if (!info.category.empty()) info.category = canonical(nextCategories, info.category);
    for (auto& tag : info.tags) tag = canonical(nextTags, tag);
    games[id].info = std::move(info); categories = std::move(nextCategories); tags = std::move(nextTags);
}
void PlayerData::createTerm(bool tag, const std::string& name) {
    const auto value = localName(name);
    if (value.empty()) throw std::runtime_error("Enter a category or tag name");
    canonical(tag ? tags : categories, value);
}
void PlayerData::renameTerm(bool tag, const std::string& previous, const std::string& name) {
    const auto value = localName(name);
    if (value.empty()) throw std::runtime_error("Enter a category or tag name");
    auto& list = tag ? tags : categories;
    auto found = std::find_if(list.begin(), list.end(), [&](const std::string& term) { return lower(term) == lower(previous); });
    if (found == list.end()) throw std::runtime_error("That local category or tag no longer exists");
    for (const auto& term : list) if (lower(term) == lower(value) && lower(term) != lower(previous)) throw std::runtime_error("That local name already exists");
    for (auto& game : games) {
        auto& info = game.second.info;
        if (tag) { for (auto& term : info.tags) if (lower(term) == lower(previous)) term = value; }
        else if (lower(info.category) == lower(previous)) info.category = value;
    }
    *found = value;
    for (auto& collection : collections) {
        auto& term = tag ? collection.filter.tag : collection.filter.category;
        if (lower(term) == lower(previous)) term = value;
    }
    std::sort(list.begin(), list.end(), [](const std::string& a, const std::string& b) { return lower(a) < lower(b); });
}
void PlayerData::deleteTerm(bool tag, const std::string& name) {
    auto& list = tag ? tags : categories;
    list.erase(std::remove_if(list.begin(), list.end(), [&](const std::string& term) { return lower(term) == lower(name); }), list.end());
    for (auto& game : games) {
        auto& info = game.second.info;
        if (tag) info.tags.erase(std::remove_if(info.tags.begin(), info.tags.end(), [&](const std::string& term) { return lower(term) == lower(name); }), info.tags.end());
        else if (lower(info.category) == lower(name)) info.category.clear();
    }
    // Keep a deleted-name filter so an existing saved collection does not
    // unexpectedly expand to every game. It remains editable in its rule editor.
}
void PlayerData::saveCollection(SavedCollection collection, const std::string& previous) {
    collection.name = localName(collection.name);
    if (collection.name.empty()) throw std::runtime_error("Enter a collection name");
    if (collection.query.size() > 128 || collection.query.find('\0') != std::string::npos) throw std::runtime_error("Collection search is too long");
    Json filter(cJSON_CreateObject(), cJSON_Delete); writeFilter(filter.get(), collection.filter);
    size_t index = collections.size();
    for (size_t i = 0; i < collections.size(); ++i) {
        if (lower(collections[i].name) == lower(previous)) index = i;
        if (lower(collections[i].name) == lower(collection.name) && lower(collections[i].name) != lower(previous))
            throw std::runtime_error("That saved collection name already exists");
    }
    if (!previous.empty() && index == collections.size()) throw std::runtime_error("Saved collection no longer exists");
    if (index == collections.size()) {
        if (collections.size() >= 64) throw std::runtime_error("Maximum 64 saved collections");
        collections.push_back(std::move(collection));
    } else collections[index] = std::move(collection);
}
void PlayerData::deleteCollection(const std::string& name) {
    collections.erase(std::remove_if(collections.begin(), collections.end(), [&](const SavedCollection& c) { return lower(c.name) == lower(name); }), collections.end());
}
void PlayerData::assignMany(const std::set<std::string>& ids, const LocalInfo& change, unsigned operation) {
    if (ids.empty()) throw std::runtime_error("Select at least one game");
    if (operation > 3) throw std::runtime_error("Invalid bulk operation");
    // Validate every update on a copy: one invalid Title ID or full tag list
    // must not leave a half-applied batch in memory or on disk.
    auto next = *this;
    for (const auto& id : ids) {
        requireId(id); auto info = next.games[id].info;
        if (operation == 0) info.category = change.category;
        else if (operation == 1) info.tags.insert(info.tags.end(), change.tags.begin(), change.tags.end());
        else if (operation == 2) info.tags.erase(std::remove_if(info.tags.begin(), info.tags.end(), [&](const std::string& tag) {
            return std::any_of(change.tags.begin(), change.tags.end(), [&](const std::string& remove) { return lower(tag) == lower(remove); });
        }), info.tags.end());
        else info.tags.clear();
        next.setInfo(id, std::move(info));
    }
    *this = std::move(next);
}
void PlayerData::apply(Entry& entry) const {
    auto found = games.find(entry.titleId);
    const GameRecord empty{};
    const auto& record = found == games.end() ? empty : found->second;
    entry.wasInstalled = record.wasInstalled; entry.rating = record.rating;
    entry.launches = record.launches; entry.firstSeenInstalled = record.firstSeenInstalled; entry.lastLaunch = record.lastLaunch;
    entry.info = record.info;
}
void PlayerData::apply(std::vector<Entry>& entries) const { for (auto& entry : entries) apply(entry); }
PlayerData parsePlayerData(const std::string& text) {
    auto root = parse(text, 3); auto* games = cJSON_GetObjectItemCaseSensitive(root.get(), "games");
    if (!cJSON_IsObject(games)) throw std::runtime_error("Local game records are missing");
    PlayerData data;
    data.categories = terms(root.get(), "categories", MaxLocalTerms); data.tags = terms(root.get(), "tags", MaxLocalTerms);
    for (auto* item = games->child; item; item = item->next) {
        const std::string id = item->string; requireId(id);
        if (!cJSON_IsObject(item) || data.games.count(id) || data.games.size() >= MaxEntries) throw std::runtime_error("Invalid or duplicate local game record");
        data.games[id] = {boolean(item, "was_installed"), unsigned(number(item, "rating", 5)),
            number(item, "first_seen_installed"), number(item, "launches"), number(item, "last_launch")};
        LocalInfo info{string(item, "category", MaxLocalName),
            string(item, "description", MaxLocalDescription), terms(item, "tags", MaxGameTags)};
        data.setInfo(id, std::move(info));
    }
    if (auto* list = cJSON_GetObjectItemCaseSensitive(root.get(), "collections")) {
        if (!cJSON_IsArray(list)) throw std::runtime_error("Invalid saved collections");
        for (auto* item = list->child; item; item = item->next) {
            if (!cJSON_IsObject(item)) throw std::runtime_error("Invalid saved collection");
            SavedCollection c; c.name = string(item, "name", MaxLocalName); c.query = string(item, "query", 128);
            c.available = boolean(item, "available"); c.filter = readFilter(cJSON_GetObjectItemCaseSensitive(item, "filter"));
            data.saveCollection(std::move(c));
        }
    }
    return data;
}
std::string serializePlayerData(const PlayerData& data) {
    // Older releases reject schema 3 rather than erase saved collections.
    Json root(cJSON_CreateObject(), cJSON_Delete); cJSON_AddNumberToObject(root.get(), "schema", 3);
    auto* games = cJSON_AddObjectToObject(root.get(), "games");
    array(root.get(), "categories", data.categories); array(root.get(), "tags", data.tags);
    auto* collections = cJSON_AddArrayToObject(root.get(), "collections");
    for (const auto& c : data.collections) {
        auto* item = cJSON_CreateObject(); cJSON_AddItemToArray(collections, item);
        cJSON_AddStringToObject(item, "name", c.name.c_str()); cJSON_AddStringToObject(item, "query", c.query.c_str());
        cJSON_AddBoolToObject(item, "available", c.available); writeFilter(cJSON_AddObjectToObject(item, "filter"), c.filter);
    }
    for (const auto& game : data.games) {
        auto* record = cJSON_AddObjectToObject(games, game.first.c_str());
        cJSON_AddBoolToObject(record, "was_installed", game.second.wasInstalled);
        cJSON_AddNumberToObject(record, "rating", game.second.rating);
        cJSON_AddNumberToObject(record, "first_seen_installed", game.second.firstSeenInstalled);
        cJSON_AddNumberToObject(record, "launches", game.second.launches);
        cJSON_AddNumberToObject(record, "last_launch", game.second.lastLaunch);
        cJSON_AddStringToObject(record, "category", game.second.info.category.c_str());
        cJSON_AddStringToObject(record, "description", game.second.info.description.c_str());
        array(record, "tags", game.second.info.tags);
    }
    return print(root.get());
}
PlayerData loadPlayerData(const std::string& directory) {
    const auto path = directory + "/player.json";
    return exists(path) ? parsePlayerData(readFile(path)) : PlayerData{};
}
void savePlayerData(const std::string& directory, const PlayerData& data) {
    const auto text = serializePlayerData(data); parsePlayerData(text); atomicWrite(directory + "/player.json", text);
}
ViewState parseViewState(const std::string& text) {
    auto root = parse(text); ViewState state;
    state.available = boolean(root.get(), "available"); state.sidebar = boolean(root.get(), "sidebar");
    state.updatesOnly = boolean(root.get(), "updates_only");
    state.serverFavoritesOnly = boolean(root.get(), "server_favorites_only");
    state.sidebarIndex = cJSON_GetObjectItemCaseSensitive(root.get(), "sidebar_index") ? number(root.get(), "sidebar_index", 4) : (state.available ? 1 : 0);
    if (state.sidebarIndex > (state.serverFavoritesOnly ? 3u : 2u)) state.sidebarIndex = state.serverFavoritesOnly ? 3 : state.updatesOnly ? 2 : state.available ? 1 : 0;
    state.query = string(root.get(), "query", 512);
    state.genre = string(root.get(), "genre", 128);
    state.genresExpanded = boolean(root.get(), "genres_expanded");
    if (const auto* history=cJSON_GetObjectItemCaseSensitive(root.get(),"recent_searches")) {
        if (!cJSON_IsArray(history)) throw std::runtime_error("Invalid recent searches");
        for (const auto* item=history->child;item;item=item->next) {
            if (!cJSON_IsString(item) || state.recentSearches.size()>=12) throw std::runtime_error("Invalid recent searches");
            std::string term=item->valuestring;
            if (term.empty() || term.size()>128 || std::any_of(term.begin(),term.end(),[](unsigned char c){return c<32 || c==127;})) throw std::runtime_error("Invalid recent search text");
            toUtf16(term);
            if (std::any_of(state.recentSearches.begin(),state.recentSearches.end(),[&](const std::string& old){return lower(old)==lower(term);})) throw std::runtime_error("Duplicate recent search");
            state.recentSearches.push_back(term);
        }
    }
    if (auto* filter = cJSON_GetObjectItemCaseSensitive(root.get(), "local_filter")) {
        state.filter = readFilter(filter);
    }
    for (size_t i = 0; i < 2; ++i) {
        auto* item = cJSON_GetObjectItemCaseSensitive(root.get(), i ? "remote" : "installed");
        if (!cJSON_IsObject(item)) throw std::runtime_error("Saved library position is missing");
        auto& cursor = state.cursor[i]; cursor.titleId = string(item, "title_id", 16); cursor.url = string(item, "url", 2047);
        cursor.index = number(item, "index", MaxEntries * MaxGameTags);
        cursor.group = string(item, "group", MaxLocalName);
        if ((!cursor.titleId.empty() && !validTitleId(cursor.titleId)) || (!cursor.url.empty() && !isHttpUrl(cursor.url)))
            throw std::runtime_error("Invalid saved library position");
    }
    return state;
}
std::string serializeViewState(const ViewState& state) {
    Json root(cJSON_CreateObject(), cJSON_Delete); cJSON_AddNumberToObject(root.get(), "schema", 1);
    cJSON_AddBoolToObject(root.get(), "available", state.available); cJSON_AddBoolToObject(root.get(), "sidebar", state.sidebar);
    cJSON_AddBoolToObject(root.get(), "updates_only", state.updatesOnly);
    cJSON_AddBoolToObject(root.get(), "server_favorites_only", state.serverFavoritesOnly);
    cJSON_AddNumberToObject(root.get(), "sidebar_index", state.sidebarIndex);
    cJSON_AddStringToObject(root.get(), "query", state.query.c_str());
    cJSON_AddStringToObject(root.get(), "genre", state.genre.c_str());
    cJSON_AddBoolToObject(root.get(), "genres_expanded", state.genresExpanded);
    auto* history=cJSON_AddArrayToObject(root.get(),"recent_searches");
    for (const auto& term:state.recentSearches) cJSON_AddItemToArray(history,cJSON_CreateString(term.c_str()));
    auto* filter = cJSON_AddObjectToObject(root.get(), "local_filter");
    writeFilter(filter, state.filter);
    for (size_t i = 0; i < 2; ++i) {
        auto* item = cJSON_AddObjectToObject(root.get(), i ? "remote" : "installed");
        const auto& cursor = state.cursor[i];
        cJSON_AddStringToObject(item, "title_id", cursor.titleId.c_str()); cJSON_AddStringToObject(item, "url", cursor.url.c_str());
        cJSON_AddNumberToObject(item, "index", cursor.index);
        cJSON_AddStringToObject(item, "group", cursor.group.c_str());
    }
    return print(root.get());
}
ViewState loadViewState(const std::string& directory) {
    const auto path = directory + "/session.json";
    return exists(path) ? parseViewState(readFile(path)) : ViewState{};
}
void saveViewState(const std::string& directory, const ViewState& state) {
    const auto text = serializeViewState(state); parseViewState(text); atomicWrite(directory + "/session.json", text);
}
}
