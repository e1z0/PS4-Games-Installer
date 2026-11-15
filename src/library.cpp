#include "library.hpp"
#include "metadata.hpp"
#include <algorithm>
#include <map>
#include <ctime>
#include <limits>
#include "sfo.hpp"

namespace psgi {
bool newerUpdate(const Entry& e) {
    const auto newer = compareVersions(e.version, e.installedVersion);
    return packageKind(e) == 1 && e.baseInstalled && newer && *newer > 0;
}
std::string requiredBaseVersion(const Entry& entry) {
    for (const auto& field : entry.metadata) if (lower(field.first) == "required_base_version") return field.second;
    return "";
}
InstallRequirements installationRequirements(const Entry& e, const std::vector<Entry>& installed, const std::vector<Entry>& queuedBases) {
    InstallRequirements out;
    if (!e.minFirmware.empty()) out.notes.push_back("Catalog minimum firmware: " + e.minFirmware.substr(0, 64) + ". Firmware compatibility has not been verified.");
    if (packageKind(e) != 1 && packageKind(e) != 2) return out;
    if (!validTitleId(e.titleId)) { out.blockers.push_back("A valid Title ID is required to check this update/DLC's base game."); return out; }
    const auto base = std::find_if(installed.begin(), installed.end(), [&](const Entry& local) { return local.titleId == e.titleId; });
    const auto waiting = std::find_if(queuedBases.begin(), queuedBases.end(), [&](const Entry& queued) { return queued.titleId == e.titleId && packageKind(queued) == 0 && queued.url != e.url; });
    if (waiting != queuedBases.end()) {
        out.dependencyUrl = waiting->url; out.notes.push_back("Waits for the queued base game " + e.titleId + " to finish installation.");
    } else if (base == installed.end()) out.blockers.push_back("Base game " + e.titleId + " is missing. Install its base package first, then retry this update/DLC.");
    const auto required = requiredBaseVersion(e);
    if (!required.empty()) {
        out.notes.push_back("Required base version: " + required.substr(0, 64));
        if (!compareVersions(required, required)) out.blockers.push_back("The catalog's required_base_version is invalid; correct its dotted numeric version.");
        else if (base != installed.end() && out.dependencyUrl.empty()) {
            auto version = compareVersions(base->version, required);
            if (!version || *version < 0) out.blockers.push_back("Installed base version " + (base->version.empty() ? std::string("unknown") : base->version) + " does not meet required version " + required + ".");
        }
    }
    out.notes.push_back("Base game and add-on/update must share the exact Title ID. Check catalog version and region information.");
    return out;
}
std::optional<int> compareVersions(const std::string& a, const std::string& b) {
    auto parts = [](const std::string& text) -> std::vector<unsigned> {
        std::vector<unsigned> out; unsigned value = 0, digits = 0;
        if (text.empty() || text.size() > 64) return {};
        for (char c : text + ".") {
            if (c == '.') {
                if (!digits || out.size() >= 8) return {};
                out.push_back(value); value = digits = 0;
            } else if (c >= '0' && c <= '9' && ++digits <= 6) value = value * 10 + unsigned(c - '0');
            else return {};
        }
        while (out.size() > 1 && out.back() == 0) out.pop_back();
        return out;
    };
    auto x = parts(a), y = parts(b);
    if (x.empty() || y.empty()) return {};
    x.resize(std::max(x.size(), y.size())); y.resize(x.size());
    return x == y ? 0 : x < y ? -1 : 1;
}
unsigned packageKind(const Entry& e) {
    const auto type = lower(e.packageType);
    if (type == "game") return 0;
    if (type == "update" || type == "patch") return 1;
    if (type == "dlc") return 2;
    const auto category = lower(e.category);
    return category == "updates" || category == "update" || category == "patch" ? 1 :
        category == "dlc" || category == "addons" ? 2 : category == "games" || category == "homebrew" || category.empty() ? 0 : 3;
}
std::string packageKindName(const Entry& e) {
    const char* names[] = {"Base game", "Update", "DLC", "Other"}; return names[packageKind(e)];
}
std::string packageStatus(const Entry& e) {
    if (e.queued) return "Queued";
    if (packageKind(e) == 2) return e.baseInstalled ? "Base installed; DLC status unknown" : "DLC; base game required";
    if (e.installed) return "Installed";
    if (e.baseInstalled && packageKind(e) == 1) {
        auto version = compareVersions(e.version, e.installedVersion);
        return version && *version > 0 ? "Newer update available" : "Base installed; version unconfirmed";
    }
    if (e.wasInstalled) return "Was installed";
    return "Available";
}
std::vector<size_t> relatedPackages(const std::vector<Entry>& entries, const std::string& id) {
    std::vector<size_t> out;
    if (id.empty()) return out;
    for (size_t i = 0; i < entries.size(); ++i) if (entries[i].titleId == id) out.push_back(i);
    std::stable_sort(out.begin(), out.end(), [&](size_t a, size_t b) {
        const auto& x = entries[a]; const auto& y = entries[b];
        if (packageKind(x) != packageKind(y)) return packageKind(x) < packageKind(y);
        const bool knownX = compareVersions(x.version, x.version).has_value(), knownY = compareVersions(y.version, y.version).has_value();
        if (knownX != knownY) return knownX;
        auto version = compareVersions(x.version, y.version);
        if (version && *version) return *version > 0;
        return lower(x.name) < lower(y.name);
    });
    return out;
}
void matchInstalled(std::vector<Entry>& remote, const std::vector<Entry>& installed,
                    const std::set<std::string>& queuedUrls) {
    std::map<std::string, const Entry*> titles;
    for (const auto& e : installed) if (!e.titleId.empty()) titles[e.titleId] = &e;
    for (auto& e : remote) {
        const bool previouslyPresent = e.installed;
        auto match = titles.find(e.titleId);
        e.baseInstalled = !e.titleId.empty() && match != titles.end();
        e.installedVersion = e.baseInstalled ? match->second->version : "";
        e.installed = e.baseInstalled && packageKind(e) == 0;
        if (e.baseInstalled && packageKind(e) == 1) {
            auto version = compareVersions(e.installedVersion, e.version); e.installed = version && *version >= 0;
        }
        e.installedSize = e.baseInstalled ? match->second->installedSize : 0;
        if (e.baseInstalled && !match->second->localIcon.empty()) e.localIcon = match->second->localIcon;
        else if (!e.baseInstalled && previouslyPresent) e.localIcon.clear();
        e.queued = queuedUrls.count(e.url) != 0;
    }
}
bool matchesLocal(const Entry& entry, const LocalFilter& filter) {
    if (entry.rating < filter.minRating || entry.rating > filter.maxRating) return false;
    if ((filter.launchState == 1 && entry.launches) || (filter.launchState == 2 && !entry.launches)) return false;
    if ((filter.installState == 1 && !entry.installed) || (filter.installState == 2 && entry.installed) ||
        (filter.installState == 3 && (entry.installed || !entry.wasInstalled))) return false;
    if (filter.uncategorized && !entry.info.category.empty()) return false;
    if (filter.untagged && !entry.info.tags.empty()) return false;
    if (!filter.category.empty() && lower(filter.category) != lower(entry.info.category)) return false;
    if (!filter.tag.empty() && std::none_of(entry.info.tags.begin(), entry.info.tags.end(), [&](const std::string& tag) { return lower(tag) == lower(filter.tag); })) return false;
    return true;
}
std::vector<VisibleEntry> makeView(const std::vector<Entry>& entries, const Settings& s,
                                 Library library, const std::string& query, const LocalFilter& filter, bool updatesOnly, const std::string& genre, bool serverFavoritesOnly) {
    std::vector<VisibleEntry> out;
    auto q = lower(query);
    std::map<std::string, std::string> titleNames;
    if (s.grouping == Grouping::Title) for (const auto& e : entries) if (!e.titleId.empty() && packageKind(e) == 0) titleNames[e.titleId] = e.name;
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        if (library == Library::Available) {
            const bool addon = packageKind(e) == 1 || packageKind(e) == 2;
            if (addon != updatesOnly) continue;
            if (serverFavoritesOnly && !e.serverFavorite) continue;
            if (!updatesOnly && !genre.empty() && std::none_of(e.igdb.genres.begin(),e.igdb.genres.end(),[&](const std::string& name){ return lower(name)==lower(genre); })) continue;
        }
        if (!matchesLocal(e, filter)) continue;
        if (library == Library::Available && s.notInstalledOnly && e.installed) continue;
        std::string searchable = e.name + " " + e.titleId + " " + e.info.category + " " + e.info.description;
        for (const auto& tag : displayTags(e)) searchable += " " + tag;
        searchable += " " + joinedNames(displayCategories(e)) + " " + joinedNames(e.igdb.publishers) + " " + joinedNames(e.igdb.themes);
        if (!q.empty() && lower(searchable).find(q) == std::string::npos) continue;
        std::string group;
        if (s.grouping == Grouping::Category) group = e.category.empty() ? "Unknown" : e.category;
        else if (s.grouping == Grouping::Region) group = e.region.empty() || e.region == "UNK" ? "Unknown" : e.region;
        else if (s.grouping == Grouping::Letter) {
            char c = lower(e.name).empty() ? '#' : lower(e.name)[0];
            group = c >= 'a' && c <= 'z' ? std::string(1, c - 'a' + 'A') : "#";
        }
        else if (s.grouping == Grouping::LocalCategory) group = displayCategories(e).empty() ? "Uncategorized" : joinedNames(displayCategories(e));
        else if (s.grouping == Grouping::Title) group = e.titleId.empty() ? "No Title ID: " + e.name : e.titleId + (titleNames.count(e.titleId) ? " / " + titleNames.at(e.titleId) : "");
        else if (s.grouping == Grouping::Tag) {
            if (displayTags(e).empty()) group = "Untagged";
            else { for (const auto& tag : displayTags(e)) if (filter.tag.empty() || lower(tag) == lower(filter.tag)) out.push_back({i, tag}); continue; }
        }
        out.push_back({i, group});
    }
    std::stable_sort(out.begin(), out.end(), [&](const VisibleEntry& a, const VisibleEntry& b) {
        if (a.group != b.group) return a.group < b.group;
        const auto& x = entries[a.index]; const auto& y = entries[b.index];
        if (s.grouping == Grouping::Title) {
            if (packageKind(x) != packageKind(y)) return packageKind(x) < packageKind(y);
            const bool knownX = compareVersions(x.version, x.version).has_value(), knownY = compareVersions(y.version, y.version).has_value();
            if (knownX != knownY) return knownX;
            auto version = compareVersions(x.version, y.version);
            if (version && *version) return *version > 0;
        }
        auto key = [&](const Entry& e) -> uint64_t {
            switch (s.sort) {
                case SortField::Size: return library == Library::Installed ? e.installedSize : e.size;
                case SortField::Rating: return uint64_t(displayRating(e) * 1000);
                case SortField::LastLaunch: return e.lastLaunch;
                case SortField::FirstInstalled: return e.firstSeenInstalled;
                default: return 0;
            }
        };
        const auto xs = key(x), ys = key(y);
        if (s.sort != SortField::Name && xs != ys) {
            if (!xs || !ys) return xs != 0; // Unknown/unrated stays last in either direction.
            return s.descending ? xs > ys : xs < ys;
        }
        if (s.sort == SortField::Name && lower(x.name) != lower(y.name)) return s.descending ? lower(x.name) > lower(y.name) : lower(x.name) < lower(y.name);
        if (lower(x.name) != lower(y.name)) return lower(x.name) < lower(y.name);
        return x.titleId + x.url < y.titleId + y.url;
    });
    return out;
}
std::vector<std::pair<std::string, std::string>> entryDetails(const Entry& e) {
    auto unknown = [](const std::string& value) { return value.empty() ? "Unknown" : value; };
    std::vector<std::pair<std::string,std::string>> out{{"Name",e.name}};
    if (!e.info.category.empty()) out.emplace_back("Category",e.info.category);
    if (!e.igdb.genres.empty()) out.emplace_back("Genres",joinedNames(e.igdb.genres));
    if (!e.info.tags.empty()) out.emplace_back("Tags",joinedNames(e.info.tags));
    out.emplace_back("Game modes",unknown(joinedNames(e.igdb.gameModes)));
    out.emplace_back("Players",playerCountText(e));
    out.emplace_back("Split screen",splitScreenText(e));
    out.insert(out.end(), {{"Rating",ratingText(e)}, {"Size",formatSize(e.size)}, {"Version",unknown(e.version)},
        {"Publisher",unknown(joinedNames(e.igdb.publishers))},
        {"Release date",unknown(e.igdb.releaseDate.empty() ? e.release : e.igdb.releaseDate)},
        {"Region",unknown(e.region)}, {"Description",displayDescription(e).empty() ? "Not provided" : displayDescription(e)}});
    return out;
}
std::vector<std::pair<std::string, std::string>> entryAdditionalDetails(const Entry& e) {
    std::vector<std::pair<std::string,std::string>> out{
        {"Title ID",e.titleId.empty() ? "Unknown" : e.titleId}, {"Package type",packageKindName(e)},
        {"State",packageStatus(e)}, {"Installed version",e.installedVersion.empty() ? (e.installed ? e.version : "Unknown") : e.installedVersion},
        {"Installed size",formatSize(e.installedSize)}, {"Installation history",e.wasInstalled ? "Previously observed installed" : "Not recorded"},
        {"Launch requests",std::to_string(e.launches)}};
    if (!e.latestVersion.empty()) out.emplace_back("Latest catalog update",e.latestVersion);
    out.insert(out.end(),e.igdb.additional.begin(),e.igdb.additional.end());
    for (const auto& date : std::vector<std::pair<std::string, uint64_t>>{{"First recorded installed", e.firstSeenInstalled}, {"Last launch request", e.lastLaunch}}) {
        if (!date.second) continue;
        const std::time_t timestamp = date.second; const auto* utc = std::gmtime(&timestamp); char text[40];
        if (utc && std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M UTC", utc)) out.emplace_back(date.first, text);
    }
    return out;
}
std::vector<std::pair<std::string, std::string>> entryTechnicalDetails(const Entry& e) {
    std::vector<std::pair<std::string,std::string>> out;
    if (!e.url.empty()) out.emplace_back("Package URL",readableUrl(e.url));
    if (!e.contentId.empty()) out.emplace_back("Content ID",e.contentId);
    if (!e.minFirmware.empty()) out.emplace_back("Minimum firmware",e.minFirmware);
    if (!e.category.empty()) out.emplace_back("Catalog source",e.category);
    const std::set<std::string> known{"title_id","name","version","region","category","size","min_fw","release","cover_url","content_id","description","rating","was_installed","launches","first_seen_installed","last_launch","package_type","igdb","url"};
    for (const auto& field:e.metadata) if (!known.count(lower(field.first))) out.push_back(field);
    if (out.empty()) out.emplace_back("Technical metadata","Not provided");
    return out;
}
}
