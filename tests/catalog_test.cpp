#include "catalog.hpp"
#include <cassert>
#include <cstdio>
#include <functional>
#include <stdexcept>

using namespace psgi;
void rejects(const std::function<void()>& f) {
    bool threw = false;
    try { f(); } catch (const std::exception&) { threw = true; }
    assert(threw);
}

int main() {
    auto sources = parseConfig(R"({"preferences":{"CONTENT_URLS":{"Games":"http://nas:8080/GAMES.json","homebrew":null,"PS5":"http://nas/PS5.json"}}})");
    assert(sources.size() == 1 && sources[0].category == "games");
    assert(parseConfig(R"({"content_urls":{}})").empty());
    rejects([] { parseConfig(R"({"content_urls":{"games":"file:///etc/passwd"}})"); });
    rejects([] { parseConfig(R"({"content_urls":{"games":42}})"); });
    rejects([] { parseConfig(R"({"content_urls":{"games":"http://nas/a","Games":"http://nas/b"}})"); });
    rejects([] { parseConfig("{}"); });
    rejects([] { parseConfig("{} garbage"); });
    rejects([] { parseConfig(std::string("{}\0extra", 8)); });
    assert(isHttpUrl("http://192.168.1.10:8080/a%20b.pkg"));
    assert(isHttpUrl("https://nas.local/g.pkg?token=abc"));
    for (auto url : {"ftp://nas/a", "http://", "http:///a", "http://nas/a b", "http://nas/\r\n", "http://u:p@nas/a", "http://nas/a#part", "http://:80/a", "http://nas\\evil/a"})
        assert(!isHttpUrl(url));

    const std::string catalog = R"({"DATA":{
      "http://nas/z.pkg":{"name":"Zulu","size":8589934592,"region":null,"version":"1.00"},
      "https://nas/a.pkg":{"name":"Alpha \u263a","size":"4294967297","title_id":"CUSA00001","min_fw":null},
      "http://nas/b.pkg":{"name":"Beta","size":null}
    }})";
    auto entries = parseCatalog(catalog, "Games");
    assert(entries.size() == 3 && entries[0].size == 8589934592ULL);
    assert(entries[0].region.empty() && entries[1].size == 4294967297ULL);
    assert(entries[1].name == "Alpha \xe2\x98\xba" && entries[2].size == 0);
    auto visible = filterCatalog(entries, "games", "");
    assert(visible == (std::vector<size_t>{1, 2, 0}));
    assert(filterCatalog(entries, "games", "CUSA00001") == (std::vector<size_t>{1}));
    assert(filterCatalog(entries, "dlc", "").empty());
    assert(filterCatalog(entries, "", "ZULU") == (std::vector<size_t>{0}));
    assert(filterCatalog(entries, "", "", true) == (std::vector<size_t>{0, 1, 2}));
    mergeCatalog(entries, entries);
    assert(entries.size() == 3);
    auto flagged = parseCatalog(R"({"DATA":{
      "http://nas/favorite.pkg":{"name":"Favorite","is_favorite":true,"is_excluded":false},
      "http://nas/ordinary.pkg":{"name":"Ordinary","is_favorite":false},
      "http://nas/hidden.pkg":{"name":"Hidden","is_favorite":true,"is_excluded":true},
      "http://nas/old-hidden.pkg":{"name":"Old hidden","is_excluded":"true"},
      "http://nas/old-favorite.pkg":{"name":"Old favorite","is_favorite":"true"}
    }})", "games");
    assert(flagged.size() == 3 && flagged[0].serverFavorite && !flagged[1].serverFavorite && flagged[2].serverFavorite);
    auto cachedFlags = parseCatalog(serializeCatalog(flagged), "");
    assert(cachedFlags.size() == 3 && cachedFlags[0].serverFavorite && !cachedFlags[1].serverFavorite && cachedFlags[2].serverFavorite);
    for (const auto& entry : cachedFlags) for (const auto& field : entry.metadata)
        assert(field.first != "is_favorite" && field.first != "is_excluded");
    rejects([] { parseCatalog(R"({"DATA":[]})", "games"); });
    rejects([] { parseCatalog(R"({"DATA":{"file:///x":{"name":"x"}}})", "games"); });
    rejects([] { parseCatalog(R"({"DATA":{"http://nas/a":{"size":2}}})", "games"); });
    rejects([] { parseCatalog(R"({"DATA":{"http://nas/a":{"name":"A","size":-1}}})", "games"); });
    rejects([] { parseCatalog(R"({"DATA":{"http://nas/a":{"name":"A","size":"18446744073709551616"}}})", "games"); });
    rejects([] { parseCatalog(R"({"DATA":{"http://nas/a":{"name":"A","size":"-1"}}})", "games"); });
    rejects([] { parseCatalog(R"({"DATA":{"http://nas/a":{"name":"A","size":1.5}}})", "games"); });
    rejects([] { parseCatalog(R"({"DATA":{"http://nas/a":{"name":"A","size":true}}})", "games"); });
    rejects([] { parseCatalog(R"({"DATA":{"http://nas/a":{"name":"A"},"http://nas/a":{"name":"B"}}})", "games"); });
    rejects([] { parseCatalog(std::string(MaxCatalogBytes + 1, ' '), "games"); });
    assert(formatSize(8589934592ULL) == "8.6 GB");
    assert(formatSize(0) == "Unknown size");
    std::puts("Catalog tests passed");
}
