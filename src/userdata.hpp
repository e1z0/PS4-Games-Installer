#pragma once
#include "catalog.hpp"
#include <map>
#include <set>

namespace psgi {
constexpr size_t MaxLocalTerms = 256, MaxGameTags = 16, MaxLocalName = 64, MaxLocalDescription = 2047;
std::string localName(const std::string& text);
LocalInfo normalizeInfo(LocalInfo info);
bool sameInfo(const LocalInfo& a, const LocalInfo& b);
std::string joinedTags(const std::vector<std::string>& tags);
struct GameRecord {
    bool wasInstalled = false;
    unsigned rating = 0;
    uint64_t firstSeenInstalled = 0, launches = 0, lastLaunch = 0;
    LocalInfo info{};
};
struct SavedCollection {
    std::string name, query;
    LocalFilter filter;
    bool available = false;
};
struct PlayerData {
    std::map<std::string, GameRecord> games;
    std::vector<std::string> categories, tags;
    std::vector<SavedCollection> collections;
    bool observeInstalled(const std::vector<Entry>& entries, uint64_t now);
    void rate(const std::string& titleId, unsigned stars);
    void noteLaunch(const std::string& titleId, uint64_t now);
    void setInfo(const std::string& titleId, LocalInfo info);
    void createTerm(bool tag, const std::string& name);
    void renameTerm(bool tag, const std::string& previous, const std::string& name);
    void deleteTerm(bool tag, const std::string& name);
    void saveCollection(SavedCollection collection, const std::string& previous = "");
    void deleteCollection(const std::string& name);
    // Tags are added/removed without replacing other tags or any other local field.
    void assignMany(const std::set<std::string>& ids, const LocalInfo& change, unsigned operation);
    void apply(Entry& entry) const;
    void apply(std::vector<Entry>& entries) const;
};
struct LibraryCursor { std::string titleId, url; size_t index = 0; std::string group; };
struct ViewState {
    bool available = false, sidebar = false, updatesOnly = false, serverFavoritesOnly = false;
    size_t sidebarIndex = 0;
    std::string query, genre;
    bool genresExpanded = false;
    std::vector<std::string> recentSearches;
    LocalFilter filter;
    LibraryCursor cursor[2];
};
PlayerData parsePlayerData(const std::string& text);
std::string serializePlayerData(const PlayerData& data);
PlayerData loadPlayerData(const std::string& directory);
void savePlayerData(const std::string& directory, const PlayerData& data);
ViewState parseViewState(const std::string& text);
std::string serializeViewState(const ViewState& state);
ViewState loadViewState(const std::string& directory);
void saveViewState(const std::string& directory, const ViewState& state);
}
