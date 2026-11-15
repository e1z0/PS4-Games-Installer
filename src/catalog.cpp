#include "catalog.hpp"
#include "metadata.hpp"
#include "json.hpp"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <set>
#include <stdexcept>

namespace psgi {
std::string lower(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return s;
}
namespace {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
Json parse(const std::string& s, size_t limit = MaxCatalogBytes) {
    if (s.size() > limit) throw std::runtime_error("JSON exceeds 8 MiB limit");
    if (s.find('\0') != std::string::npos) throw std::runtime_error("JSON contains NUL bytes");
    Json result(cJSON_ParseWithLengthOpts(s.c_str(), s.size() + 1, nullptr, true), cJSON_Delete);
    if (!result || !cJSON_IsObject(result.get())) throw std::runtime_error("Expected a JSON object");
    return result;
}
std::string field(const cJSON* object, const char* key) {
    const cJSON* value = cJSON_GetObjectItem(object, key);
    return cJSON_IsString(value) ? value->valuestring : "";
}
bool serverFlag(const cJSON* object, const char* key) {
    const cJSON* value = cJSON_GetObjectItemCaseSensitive(object, key);
    // Older offline caches serialized boolean metadata as the string "true".
    return cJSON_IsTrue(value) || (cJSON_IsString(value) && std::string(value->valuestring) == "true");
}
uint64_t sizeField(const cJSON* object) {
    const cJSON* value = cJSON_GetObjectItem(object, "size");
    if (cJSON_IsNumber(value)) {
        double d = value->valuedouble;
        if (!std::isfinite(d) || d < 0 || d > 9007199254740991.0 || std::floor(d) != d)
            throw std::runtime_error("Invalid package size");
        return static_cast<uint64_t>(d);
    }
    if (cJSON_IsString(value)) {
        std::string s = value->valuestring;
        if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos)
            throw std::runtime_error("Invalid package size string");
        errno = 0;
        unsigned long long n = std::strtoull(s.c_str(), nullptr, 10);
        if (errno == ERANGE) throw std::runtime_error("Package size overflows uint64");
        return n;
    }
    if (value && !cJSON_IsNull(value)) throw std::runtime_error("Invalid package size type");
    return 0;
}
}

bool isHttpUrl(const std::string& url) {
    size_t begin = url.compare(0, 7, "http://") == 0 ? 7 :
                   url.compare(0, 8, "https://") == 0 ? 8 : 0;
    if (!begin || url.size() >= 2048 || url.size() == begin) return false;
    for (unsigned char c : url) if (c <= 32 || c == 127 || c == '\\') return false;
    size_t end = url.find_first_of("/?#", begin);
    std::string authority = url.substr(begin, end - begin);
    return !authority.empty() && authority.find('@') == std::string::npos &&
           authority[0] != ':' && url.find('#') == std::string::npos;
}

std::vector<Source> parseConfig(const std::string& text) {
    auto root = parse(text);
    const cJSON* object = root.get();
    // Accept nested preferences.content_urls, including uppercase keys.
    const cJSON* prefs = cJSON_GetObjectItem(object, "preferences");
    if (prefs) object = prefs;
    const cJSON* urls = cJSON_GetObjectItem(object, "content_urls");
    if (!cJSON_IsObject(urls)) throw std::runtime_error("Config needs a content_urls object");
    std::vector<Source> sources;
    std::set<std::string> categories;
    for (const cJSON* item = urls->child; item; item = item->next) {
        if (cJSON_IsNull(item)) continue;
        if (!cJSON_IsString(item) || !isHttpUrl(item->valuestring))
            throw std::runtime_error("Catalog URLs must be direct http:// or https:// URLs");
        std::string category = lower(item->string);
        if (category == "ps5") continue;
        if (!categories.insert(category).second) throw std::runtime_error("Duplicate category in config");
        sources.push_back({category, item->valuestring});
        if (sources.size() > 32) throw std::runtime_error("Too many catalog sources (max 32)");
    }
    return sources;
}

std::vector<Entry> parseCatalog(const std::string& text, const std::string& category, size_t limit) {
    auto root = parse(text, limit);
    const cJSON* data = cJSON_GetObjectItem(root.get(), "DATA");
    if (!cJSON_IsObject(data)) throw std::runtime_error("Catalog needs a DATA object");
    std::vector<Entry> result;
    std::set<std::string> urls;
    for (const cJSON* item = data->child; item; item = item->next) {
        if (!cJSON_IsObject(item))
            throw std::runtime_error("Catalog DATA must map direct package URLs to objects");
        if (serverFlag(item, "is_excluded")) continue;
        if (!isHttpUrl(item->string))
            throw std::runtime_error("Catalog DATA must map direct package URLs to objects");
        Entry entry;
        entry.url = item->string;
        entry.serverFavorite = serverFlag(item, "is_favorite");
        entry.name = field(item, "name");
        entry.titleId = field(item, "title_id");
        entry.region = field(item, "region");
        entry.version = field(item, "version");
        entry.minFirmware = field(item, "min_fw");
        entry.coverUrl = field(item, "cover_url");
        if (!entry.coverUrl.empty() && !isHttpUrl(entry.coverUrl)) entry.coverUrl.clear();
        entry.release = field(item, "release");
        entry.contentId = field(item, "content_id");
        entry.description = field(item, "description");
        entry.packageType = lower(field(item, "package_type"));
        if (const auto* igdb = cJSON_GetObjectItemCaseSensitive(item, "igdb"); cJSON_IsObject(igdb)) {
            std::unique_ptr<char, decltype(&cJSON_free)> json(printJson(igdb), cJSON_free);
            if (json) readIgdb(entry, json.get());
        }
        entry.size = sizeField(item);
        entry.category = lower(category.empty() ? field(item, "category") : category);
        for (const cJSON* m = item->child; m && entry.metadata.size() < 64; m = m->next) {
            if (std::string(m->string) == "igdb" || std::string(m->string) == "is_favorite" || std::string(m->string) == "is_excluded") continue;
            if (cJSON_IsString(m)) entry.metadata.emplace_back(m->string, std::string(m->valuestring).substr(0, 4096));
            else if (cJSON_IsNumber(m) || cJSON_IsBool(m)) {
                std::unique_ptr<char, decltype(&cJSON_free)> value(printJson(m), cJSON_free);
                if (value) entry.metadata.emplace_back(m->string, value.get());
            }
        }
        if (entry.name.empty() || entry.name.size() >= 600)
            throw std::runtime_error("Package name is missing or too long");
        if (!urls.insert(entry.url).second) throw std::runtime_error("Duplicate package URL in catalog");
        result.push_back(std::move(entry));
        if (result.size() > MaxEntries) throw std::runtime_error("Too many catalog entries");
    }
    return result;
}

void mergeCatalog(std::vector<Entry>& into, const std::vector<Entry>& entries) {
    std::set<std::string> urls;
    for (const auto& e : into) urls.insert(e.url);
    for (const auto& e : entries) if (urls.insert(e.url).second) {
        if (into.size() >= MaxEntries) throw std::runtime_error("Too many combined catalog entries");
        into.push_back(e);
    }
}

std::vector<size_t> filterCatalog(const std::vector<Entry>& entries, const std::string& category,
                                const std::string& query, bool sortBySize) {
    std::vector<size_t> result;
    std::string q = lower(query);
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        if ((!category.empty() && e.category != lower(category)) ||
            (!q.empty() && lower(e.name + " " + e.titleId).find(q) == std::string::npos)) continue;
        result.push_back(i);
    }
    std::sort(result.begin(), result.end(), [&](size_t a, size_t b) {
        if (sortBySize && entries[a].size != entries[b].size) return entries[a].size > entries[b].size;
        return lower(entries[a].name) < lower(entries[b].name);
    });
    return result;
}

std::string formatSize(uint64_t bytes) {
    if (!bytes) return "Unknown size";
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    double n = static_cast<double>(bytes);
    size_t i = 0;
    while (n >= 1000 && i < 6) { n /= 1000; ++i; }
    char out[64];
    std::snprintf(out, sizeof(out), "%.1f %s", n, units[i]);
    return out;
}

std::string serializeCatalog(const std::vector<Entry>& entries) {
    Json root(cJSON_CreateObject(), cJSON_Delete);
    auto* data = cJSON_AddObjectToObject(root.get(), "DATA");
    for (const auto& e : entries) {
        auto* item = cJSON_AddObjectToObject(data, e.url.c_str());
        for (const auto& m : e.metadata) cJSON_AddStringToObject(item, m.first.c_str(), m.second.c_str());
        auto set = [&](const char* key, const std::string& value) {
            cJSON_DeleteItemFromObjectCaseSensitive(item, key);
            cJSON_AddStringToObject(item, key, value.c_str());
        };
        set("name", e.name); set("title_id", e.titleId); set("region", e.region);
        set("version", e.version); set("min_fw", e.minFirmware); set("category", e.category);
        set("cover_url", e.coverUrl); set("release", e.release); set("content_id", e.contentId);
        set("description", e.description); set("size", std::to_string(e.size));
        cJSON_DeleteItemFromObjectCaseSensitive(item, "is_favorite");
        cJSON_DeleteItemFromObjectCaseSensitive(item, "is_excluded");
        cJSON_AddBoolToObject(item, "is_favorite", e.serverFavorite);
        if (!e.packageType.empty()) set("package_type",e.packageType);
        if (!e.igdbJson.empty()) {
            auto* igdb=cJSON_Parse(e.igdbJson.c_str());
            if(cJSON_IsObject(igdb))cJSON_AddItemToObject(item,"igdb",igdb);else cJSON_Delete(igdb);
        }
    }
    std::unique_ptr<char, decltype(&cJSON_free)> text(printJson(root.get()), cJSON_free);
    if (!text) throw std::runtime_error("Cannot serialize library");
    return text.get();
}

std::string readFile(const std::string& path, size_t limit) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) throw std::runtime_error("Cannot open " + path);
    auto closeFile = [](FILE* stream) { std::fclose(stream); };
    std::unique_ptr<FILE, decltype(closeFile)> file(f, closeFile);
    std::string text;
    char block[8192];
    size_t n;
    while ((n = std::fread(block, 1, sizeof(block), f))) {
        if (text.size() + n > limit) throw std::runtime_error("File exceeds 8 MiB limit");
        text.append(block, n);
    }
    if (std::ferror(f)) throw std::runtime_error("Failed reading " + path);
    return text;
}
}
