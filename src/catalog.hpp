#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace psgi {
constexpr size_t MaxCatalogBytes = 8 * 1024 * 1024;
constexpr size_t MaxEntries = 50000;

struct Source { std::string category, url; };
struct LocalInfo {
    std::string category, description;
    std::vector<std::string> tags;
};
struct LocalFilter {
    std::string category, tag;
    bool uncategorized = false, untagged = false;
    unsigned minRating = 0, maxRating = 5;
    // 0 any, 1 no launches recorded in this app, 2 launched through this app.
    unsigned launchState = 0;
    // 0 any, 1 installed, 2 not installed, 3 previously installed but absent.
    unsigned installState = 0;
};
struct MediaItem { std::string name, url; };
struct PlayerMode {
    bool supportedKnown = false, supported = false;
    bool splitScreenKnown = false, splitScreen = false;
    unsigned maximum = 0;
};
struct RichMetadata {
    std::vector<std::string> genres, themes, publishers, developers, gameModes;
    std::string summary, storyline, releaseDate;
    PlayerMode offline, online;
    double userRating = 0;
    bool userRatingKnown = false;
    uint64_t ratingCount = 0;
    std::vector<MediaItem> screenshots, videos;
    std::vector<std::pair<std::string, std::string>> additional;
};
struct Entry {
    std::string url, name, titleId, region, version, minFirmware, category;
    std::string coverUrl, localIcon, release, contentId, description;
    std::string installedVersion, latestVersion;
    uint64_t size = 0, installedSize = 0;
    bool installed = false, queued = false, serverFavorite = false;
    bool baseInstalled = false;
    // Local records are applied by Title ID after catalog parsing/refresh.
    bool wasInstalled = false;
    unsigned rating = 0;
    uint64_t firstSeenInstalled = 0, launches = 0, lastLaunch = 0;
    LocalInfo info;
    std::vector<std::pair<std::string, std::string>> metadata;
    std::string packageType, igdbJson;
    RichMetadata igdb;
};

bool isHttpUrl(const std::string& url);
std::vector<Source> parseConfig(const std::string& text);
std::vector<Entry> parseCatalog(const std::string& text, const std::string& category, size_t limit = MaxCatalogBytes);
void mergeCatalog(std::vector<Entry>& into, const std::vector<Entry>& entries);
std::vector<size_t> filterCatalog(const std::vector<Entry>& entries,
                                const std::string& category, const std::string& query,
                                bool sortBySize = false);
std::string formatSize(uint64_t bytes);
std::string readFile(const std::string& path, size_t limit = MaxCatalogBytes);
std::string serializeCatalog(const std::vector<Entry>& entries);
std::string lower(std::string value);
}
