#include "render.hpp"
#include "metadata.hpp"
#include "revision.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>

namespace psgi {
namespace {
constexpr uint32_t White = 0xf2f7ff, Muted = 0x9bbdd8, Accent = 0x7ae5e0, Blue = 0x267bb8, Panel = 0x102c49;
const char* Copyright = "Created by e1z0 (c) 2026 Vintage2000.org";
void scrollIndicator(Canvas& c, size_t total, size_t start, size_t count, int x, int top, int height) {
    if (!count || total <= count || height < 60) return;
    const size_t shown = std::min(count, total - std::min(start, total));
    const int trackTop = top + 18, trackHeight = height - 36;
    c.rounded(x + 3, trackTop, 8, trackHeight, 4, 0x34546c);
    const int thumbHeight = std::max(18, int(double(trackHeight) * shown / total));
    const int offset = std::min(trackHeight - thumbHeight, int(double(trackHeight) * start / total));
    c.rounded(x + 2, trackTop + offset, 10, thumbHeight, 5, Accent);
    const auto up = start ? Accent : Muted, down = start + shown < total ? Accent : Muted;
    for (int i = 0; i < 6; ++i) {
        c.rectangle(x + 6 - i, top + 3 + i, 2, 2, up);
        c.rectangle(x + 6 + i, top + 3 + i, 2, 2, up);
        c.rectangle(x + i + 1, top + height - 10 + i, 2, 2, down);
        c.rectangle(x + 11 - i, top + height - 10 + i, 2, 2, down);
    }
}

void buttonIcon(Canvas& c, Button button, int x, int baseline, int width) {
    const int top = baseline - 27;
    c.rounded(x, top, width, 36, 8, 0x46647e);
    c.rounded(x + 1, top + 1, width - 2, 34, 7, 0x16344f);
    auto line = [&](int ax, int ay, int bx, int by, uint32_t color) {
        const int steps = std::max(std::abs(bx - ax), std::abs(by - ay));
        for (int i = 0; i <= steps; ++i)
            c.rectangle(x + ax + (bx - ax) * i / steps, top + ay + (by - ay) * i / steps, 2, 2, color);
    };
    switch (button) {
        case Button::Cross:
            line(10, 10, 24, 24, 0x8cbfff); line(10, 24, 24, 10, 0x8cbfff); break;
        case Button::Square:
            line(9, 9, 25, 9, 0xf4a5d7); line(25, 9, 25, 25, 0xf4a5d7);
            line(25, 25, 9, 25, 0xf4a5d7); line(9, 25, 9, 9, 0xf4a5d7); break;
        case Button::Triangle:
            line(17, 8, 27, 25, 0x7ae5b5); line(27, 25, 7, 25, 0x7ae5b5);
            line(7, 25, 17, 8, 0x7ae5b5); break;
        case Button::Circle:
            for (int i = 0; i < 64; ++i) {
                double angle = i * 6.283185307 / 64;
                c.rectangle(x + 17 + int(std::round(9 * std::cos(angle))),
                            top + 17 + int(std::round(9 * std::sin(angle))), 2, 2, 0xff9b9f);
            }
            break;
        default: {
            const std::string label = button == Button::Options ? "OPTIONS" : button == Button::L2 ? "L2 / R2" : "L1 / R1";
            c.text(label, x + (width - c.textWidth(label, 13)) / 2, top + 23, 13, White);
            break;
        }
    }
}
void hints(Canvas& c, int x, int baseline,
           std::initializer_list<std::pair<Button, std::string>> items, int size = 23) {
    for (const auto& item : items) {
        const int width = item.first == Button::Options || item.first == Button::L1 || item.first == Button::L2 ? 80 : 36;
        buttonIcon(c, item.first, x, baseline, width);
        c.text(item.second, x + width + 10, baseline, size, White);
        x += width + 10 + c.textWidth(item.second, size) + 32;
    }
}
std::shared_ptr<Image> softened(const Image& input) {
    auto image = std::make_shared<Image>(input);
    std::vector<uint32_t> scratch(input.pixels.size());
    constexpr int radius = 7;
    for (int pass = 0; pass < 2; ++pass) {
        const int outer = pass == 0 ? input.height : input.width;
        const int inner = pass == 0 ? input.width : input.height;
        auto index = [&](int row, int col) { return pass == 0 ? row * input.width + col : col * input.width + row; };
        for (int row = 0; row < outer; ++row) {
            int red = 0, green = 0, blue = 0;
            auto add = [&](int col, int factor) {
                uint32_t color = image->pixels[index(row, std::clamp(col, 0, inner-1))];
                red += int((color >> 16) & 255) * factor;
                green += int((color >> 8) & 255) * factor;
                blue += int(color & 255) * factor;
            };
            for (int i = -radius; i <= radius; ++i) add(i, 1);
            for (int col = 0; col < inner; ++col) {
                scratch[index(row, col)] = 0xff000000 | uint32_t(red / 15) << 16 | uint32_t(green / 15) << 8 | uint32_t(blue / 15);
                add(col - radius, -1); add(col + radius + 1, 1);
            }
        }
        image->pixels.swap(scratch);
    }
    return image;
}
std::string state(const Entry& e) { const auto text = packageStatus(e); return text == "Available" ? "" : text; }
void stars(Canvas& c, unsigned rating, int x, int y, int size) {
    if (!rating) return;
    for (unsigned star = 0; star < 5; ++star) {
        std::array<std::pair<double, double>, 10> points;
        for (size_t i = 0; i < points.size(); ++i) {
            const double angle = -1.57079632679 + i * .628318530718;
            const double radius = size * (i % 2 ? .22 : .49);
            points[i] = {x + star * (size + 4) + size * .5 + std::cos(angle) * radius,
                         y + size * .5 + std::sin(angle) * radius};
        }
        for (int row = y; row < y + size; ++row) {
            std::array<double, 10> crossings{}; size_t count = 0;
            for (size_t i = 0; i < points.size(); ++i) {
                const auto a = points[i], b = points[(i + 1) % points.size()]; const double scan = row + .5;
                if ((a.second <= scan && b.second > scan) || (b.second <= scan && a.second > scan))
                    crossings[count++] = a.first + (scan - a.second) * (b.first - a.first) / (b.second - a.second);
            }
            std::sort(crossings.begin(), crossings.begin() + count);
            for (size_t i = 0; i + 1 < count; i += 2) {
                const int left = int(std::ceil(crossings[i])), right = int(std::floor(crossings[i + 1]));
                c.rectangle(left, row, right - left + 1, 1, star < rating ? 0xffd478 : 0x46647e);
            }
        }
    }
}
std::string shortSize(uint64_t bytes) {
    auto value = formatSize(bytes);
    size_t p = value.find(".0 ");
    if (p != std::string::npos) value.erase(p, 2);
    return value;
}
}
Renderer::Renderer(const std::string& assets) : placeholder(loadImage(assets + "/icon.png")) {}
void Renderer::panel(Canvas& c, int x, int y, int w, int h, const std::string& title) {
    c.rectangle(0, 0, Canvas::Width, Canvas::Height, 0x020b17, 170);
    c.rounded(x + 10, y + 16, w, h, 20, 0x00040d, 85);
    c.rounded(x, y, w, h, 20, Panel, 250);
    c.rectangle(x + 32, y, w - 64, 3, Accent);
    c.text(title, x + 40, y + 68, 36, White, w - 80);
}
void Renderer::menu(Canvas& c, const std::vector<std::string>& rows, size_t selected,
                    int x, int y, int w, int rowHeight, size_t count) {
    if (rows.empty() || !count) return;
    selected = std::min(selected, rows.size() - 1);
    const size_t start = count == SIZE_MAX ? 0 : selected / count * count;
    const bool overflow = rows.size() > count;
    const int contentWidth = w - (overflow ? 28 : 0);
    for (size_t i = start; i < rows.size() && i - start < count; ++i) {
        int top = y + int(i - start) * rowHeight;
        if (i == selected) c.rounded(x, top - 32, contentWidth, rowHeight - 4, 9, Blue, 180);
        c.text(rows[i], x + 20, top, 27, i == selected ? White : Muted, contentWidth - 40);
        if (i == selected) c.rectangle(x, top - 27, 3, rowHeight - 14, Accent);
    }
    if (overflow) scrollIndicator(c, rows.size(), start, count, x + w - 16, y - 32, int(count) * rowHeight - 4);
}
void Renderer::draw(Canvas& c, Ui& ui, ArtworkCache& artwork, double time) {
    c.background(time, ui.screen == Screen::Settings ? ui.draft.animations : ui.settings.animations);
    const Entry* selected = ui.current();
    const auto focus=selected ? selected->titleId+"|"+selected->url : "";
    if(focus!=textFocus){textFocus=focus;textFocusChanged=time;}
    const auto textTime=std::max(0.0,time-textFocusChanged);
    if (ui.screen == Screen::Splash) {
        c.image(*placeholder, 904, 330, 112, 112);
        c.centered("Loading PS4 Games Installer", 560, 48, White);
        for (int i = 0; i < 3; ++i) c.rounded(925 + i * 28, 608, 10, 10, 5, Accent,
            uint8_t(100 + 150 * (.5 + .5 * std::sin(time * 3 - i))));
        c.centered(Copyright, 1016, 23, Muted);
        return;
    }
    if (ui.settings.layout == Layout::Summary && selected) {
        std::string key = artworkKey(*selected);
        auto image = artwork.request(*selected, true);
        if (key != backgroundKey || (image && image != backgroundSource)) {
            previousBackground = currentBackground;
            backgroundSource = image;
            currentBackground = image ? softened(*image) : nullptr;
            backgroundKey = key;
            backgroundChanged = time;
        }
        unsigned alpha = ui.settings.animations ? std::min(255.0, std::max(0.0, (time - backgroundChanged) * 650)) : 255;
        if (previousBackground && alpha < 255) c.image(*previousBackground, 0, 0, Canvas::Width, Canvas::Height, 90 * (255 - alpha) / 255, true);
        if (currentBackground) c.image(*currentBackground, 0, 0, Canvas::Width, Canvas::Height, 90 * alpha / 255, true);
        c.rectangle(0, 0, Canvas::Width, Canvas::Height, 0x061a30, 130);
    }
    c.image(*placeholder, 54, 52, 58, 58);
    c.text("PS4 Games Installer", 132, 94, 39, White);
    c.text(PSGI_VERSION, 508, 127, 18, Muted);
    if (!ui.settings.activeProfile.empty()) c.text("Server: "+ui.settings.activeProfile,580,127,18,Accent,950);
    c.text(ui.freeBytes ? shortSize(ui.freeBytes) + " free" : "Your PS4 library", 1500, 94, 25, Muted, 350);
    c.rectangle(54, 151, 1812, 1, 0x638aad, 65);

    c.text("LIBRARY", 74, 216, 20, Muted);
    const auto sidebarRows=ui.sidebarRows();
    const auto active=ui.sidebar ? std::min(ui.sidebarIndex,sidebarRows.size()-1) : ui.activeSidebarIndex();
    const size_t sidebarStart=active/9*9;
    for(size_t i=sidebarStart;i<std::min(sidebarStart+9,sidebarRows.size());++i) {
        const bool focus=i==active, genre=!sidebarRows[i].genre.empty();
        const int y=268+int(i-sidebarStart)*57;
        if(focus) {
            c.rounded(56,y-35,262,50,10,ui.sidebar ? Blue : Panel,210);
            c.rectangle(56,y-25,3,31,Accent);
        }
        c.text(sidebarRows[i].name,genre ? 94 : 78,y,genre ? 22 : 27,focus ? White : Muted,genre ? 197 : 220);
    }
    scrollIndicator(c,sidebarRows.size(),sidebarStart,9,311,230,525);
    c.text("OPTIONS", 78, 840, 18, Muted);
    c.text("Settings & more", 78, 875, 23, White, 236);
    c.text("Vintage2000.org", 78, 935, 18, Muted);
    c.text(ui.library == Library::Installed ? "Installed games" : ui.updatesOnly ? "Updates / DLC" : "Available games", 368, 206, 32, White);
    c.text(ui.filterDescription(), 750, 205, 20, Accent, 850);
    c.text(std::to_string(ui.shownGames()) + (ui.shownGames() == 1 ? " title" : " titles"), 1640, 204, 23, Muted, 220);
    if (ui.bulkMode) c.text("SELECTING: " + std::to_string(ui.markedTitles.size()) + " games", 75, 665, 21, Accent, 250);

    if (ui.visible.empty()) {
        c.image(*placeholder, 1020, 365, 140, 140, 180);
        c.text(!ui.filterDescription().empty() || !ui.query.empty() ? "No games match this view" : ui.library == Library::Installed ? "No installed games found" : ui.updatesOnly ? "No updates or DLC found" : "Your remote library belongs here", 700, 585, 34, White, 1100);
        c.text(!ui.filterDescription().empty() ? "Options > Browse local categories or tags > All games clears local filters." : !ui.query.empty() ? "Options > Search lets you change or clear your search." : ui.library == Library::Installed ? "Refresh the library in Options, or browse Available." : ui.updatesOnly ? "Refresh the catalog to load updates and DLC. Install the matching base game first." : "Open Options > Settings and enter your Games JSON URL.", 610, 641, 26, Muted, 1220);
        if (ui.loading) c.text("Loading...", 1020, 702, 25, Accent);
    } else if (ui.settings.layout == Layout::Tiles) {
        const auto& rows = ui.tileRows();
        size_t rowIndex = ui.tileRowIndex();
        size_t begin = rowIndex / 2 * 2;
        for (size_t r = begin; r < std::min(begin + 2, rows.size()); ++r) {
            int y = 250 + (r - begin) * 348;
            if (!rows[r].group.empty()) c.text(rows[r].group, 370, y - 14, 20, Accent, 1200);
            for (size_t col = 0; col < rows[r].items.size(); ++col) {
                size_t position = rows[r].items[col];
                const auto& entry = ui.entries()[ui.visible[position].index];
                int x = 366 + col * 376;
                bool focus = position == ui.position() && !ui.sidebar;
                if (focus) c.rounded(x - 3, y - 3, 336, 325, 13, Accent, 220);
                c.rounded(x, y, 330, 319, 11, Panel, focus ? 245 : 190);
                auto image = artwork.request(entry, focus);
                c.image(*(image ? image : placeholder), x + 55, y + 12, 220, 204);
                if (!state(entry).empty()) {
                    c.rounded(x + 13, y + 16, ui.updatesOnly ? 304 : 148, 28, 6, 0x08283c, 235);
                    c.text(ui.updatesOnly ? packageKindName(entry) + " / v" + entry.version : state(entry), x + 22, y + 36, 15, Accent, ui.updatesOnly ? 286 : 132);
                }
                c.scrollingText(entry.name,x+17,y+245,24,White,297,textTime,focus&&ui.settings.animations);
                c.scrollingText(displayCategories(entry).empty() ? "Uncategorized" : joinedNames(displayCategories(entry)),x+17,y+270,17,Accent,297,textTime,focus&&ui.settings.animations);
                c.text(shortSize(ui.library == Library::Installed ? entry.installedSize : entry.size), x + 17, y + 299, 21, Muted, 288);
                stars(c, unsigned(std::round(displayRating(entry))), x + 190, y + 282, 18);
                if(!entry.rating&&entry.igdb.userRatingKnown)c.text("IGDB",x+149,y+299,12,Muted,39);
                if (ui.bulkMode && ui.markedTitles.count(entry.titleId)) {
                    c.rounded(x + 220, y + 16, 95, 30, 5, Blue); c.text("Selected", x + 228, y + 38, 17, White, 82);
                }
            }
        }
        c.text("Page " + std::to_string(begin / 2 + 1) + " / " + std::to_string((rows.size() + 1) / 2), 1670, 949, 18, Muted, 180);
    } else {
        // A compact list leaves a large selected-game preview on the right.
        size_t begin = ui.position() / 8 * 8;
        for (size_t i = begin; i < std::min(begin + 8, ui.visible.size()); ++i) {
            const auto& entry = ui.entries()[ui.visible[i].index];
            int y = 266 + (i - begin) * 82;
            bool focus = i == ui.position() && !ui.sidebar;
            if (focus) c.rounded(361, y - 24, 926, 76, 9, Blue, 190);
            auto image = artwork.request(entry, focus);
            c.image(*(image ? image : placeholder), 374, y - 17, 58, 58);
            c.text(entry.name, 450, y + 3, 25, White, 645);
            std::string meta = entry.titleId + (entry.region.empty() ? "" : "  " + entry.region) + (ui.visible[i].group.empty() ? "" : "  /  " + ui.visible[i].group);
            if (!state(entry).empty()) meta += "  /  " + (ui.updatesOnly ? packageKindName(entry) + " / v" + entry.version : state(entry));
            c.scrollingText(displayCategories(entry).empty() ? "Uncategorized" : joinedNames(displayCategories(entry)),451,y+26,17,Accent,640,textTime,focus&&ui.settings.animations);
            c.text(meta, 451, y + 46, 15, Muted, 640);
            c.text(shortSize(ui.library == Library::Installed ? entry.installedSize : entry.size), 1110, y + 13, 21, Muted, 165);
            stars(c, unsigned(std::round(displayRating(entry))), 1110, y + 25, 17);
            if(!entry.rating&&entry.igdb.userRatingKnown)c.text("IGDB",1230,y+41,12,Muted,45);
            if (ui.bulkMode && ui.markedTitles.count(entry.titleId)) c.text("[Selected]", 1008, y + 45, 15, Accent, 95);
        }
        if (selected) {
            auto image = artwork.request(*selected, true);
            c.rounded(1340, 252, 514, 622, 16, Panel, 180);
            c.image(*(image ? image : placeholder), 1366, 279, 462, 400);
            c.text(selected->name, 1372, 732, 30, White, 452);
            c.scrollingText(displayCategories(*selected).empty() ? "Uncategorized" : joinedNames(displayCategories(*selected)),1373,763,18,Accent,452,textTime,!ui.sidebar&&ui.settings.animations);
            c.text(shortSize(ui.library == Library::Installed ? selected->installedSize : selected->size), 1373, 800, 24, Accent, 450);
            c.text(ui.updatesOnly ? packageKindName(*selected) + " / v" + selected->version : state(*selected), 1373, 831, 20, Muted, 450);
            stars(c, unsigned(std::round(displayRating(*selected))), 1373, 845, 20);
            if(!selected->rating&&selected->igdb.userRatingKnown)c.text("IGDB users",1500,862,16,Muted,170);
        }
    }
    if (selected) {
        auto description = displayDescription(*selected);
        for (auto& c : description) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (!description.empty()) c.text(description, 368, 946, 21, Muted, 1250);
    }
    c.rectangle(54, 964, 1812, 1, 0x638aad, 65);
    c.text(ui.librarySummary(), 70, 1000, 22, White, 620);
    c.text(ui.status, 720, 1000, 21, ui.loading ? Accent : Muted, 1130);
    hints(c, 70, 1045, {{Button::Cross, ui.bulkMode ? "Mark game" : ui.library == Library::Installed ? "Launch" : "Install"},
        {Button::Square, ui.sidebar ? "Jump to letter" : "Details"}, {Button::Triangle, "Actions"}, {Button::Options, "Menu"},
        {Button::Circle, ui.bulkMode ? "End selection" : "Back / Exit"}, {Button::L1, "Library"}, {Button::L2, "Page"}}, 22);

    if (featureDialog(c, ui)) return;
    switch (ui.screen) {
        case Screen::Options:
            panel(c, 520, 168, 880, 728, "Options");
            menu(c, ui.options(), ui.menuIndex, 553, 310, 814, 58, 8);
            c.text("Up / Down for more options", 570, 812, 20, Muted, 760);
            hints(c, 570, 862, {{Button::Cross, "Select"}, {Button::Circle, "Back"}}); break;
        case Screen::Settings: {
            panel(c, 352, 115, 1216, 840, "Settings");
            menu(c, ui.settingRows(), ui.settingsIndex, 390, 251, 1140, 48, 10);
            c.text(ui.status, 412, 812, 20, Muted, 1090);
            std::string url = ui.draft.sources[0].url;
            c.text(url.empty() ? "Games URL is not configured" : url, 412, 844, 21, Muted, 1090);
            c.text(ui.settingsNotice.empty() ? "Select Save settings to apply edits. O cancels unsaved changes." : ui.settingsNotice,
                   412, 879, 20, Accent, 1090);
            hints(c, 412, 913, {{Button::Cross, "Change / Select"}, {Button::Circle, "Cancel"}});
            c.text("Left / Right Change", 1100, 913, 23, Muted); break;
        }
        case Screen::ArchiveAccount: {
            panel(c, 352, 165, 1216, 740, "Archive.org account");
            c.text(ui.archiveSignedIn ? "Signed in" : "Not signed in", 412, 300, 27, ui.archiveSignedIn ? Accent : Muted, 1090);
            c.text("Sign in to download files your account can access.", 412, 345, 24, Muted, 1090);
            menu(c, ui.archiveRows(), ui.archiveIndex, 390, 422, 1140, 66, 4);
            c.text(ui.archiveNotice, 412, 712, 23, Accent, 1090);
            c.text(ui.archiveBusy ? "You can go back while this finishes." : "After signing in, retry the failed download from Downloads.",
                   412, 770, 22, Muted, 1090);
            if (ui.archiveBusy) hints(c, 412, 861, {{Button::Circle, "Back"}});
            else hints(c, 412, 861, {{Button::Cross, "Edit / Select"}, {Button::Circle, "Back"}});
            break;
        }
        case Screen::Sources: {
            panel(c, 352, 140, 1216, 800, "Additional catalog sources");
            size_t start = ui.sourceIndex > 8 ? ui.sourceIndex - 7 : 1;
            for (size_t i = start; i <= ui.draft.sources.size() && i < start + 8; ++i) {
                int y = 280 + (i - start) * 68;
                if (i == ui.sourceIndex) c.rounded(390, y - 34, ui.draft.sources.size() > 8 ? 1112 : 1140, 62, 9, Blue, 190);
                c.text(i == ui.draft.sources.size() ? "+ Add catalog source" : ui.draft.sources[i].category, 414, y - 4, 27, White, ui.draft.sources.size() > 8 ? 1062 : 1090);
                if (i < ui.draft.sources.size()) c.text(ui.draft.sources[i].url.empty() ? "Disabled" : ui.draft.sources[i].url, 414, y + 22, 18, Muted, ui.draft.sources.size() > 8 ? 1062 : 1090);
            }
            scrollIndicator(c, ui.draft.sources.size(), start - 1, 8, 1514, 246, 540);
            hints(c, 412, 898, {{Button::Cross, "Edit / Add"}, {Button::Square, "Setup helper"}, {Button::Triangle, "Remove"}, {Button::Circle, "Back"}}, 21); break;
        }
        case Screen::Storage: {
            panel(c, 320, 170, 1280, 740, "Installation storage");
            c.text("Choose where new games will be installed", 370, 320, 30, White, 1180);
            auto space = [&](const char* name, const StorageSpace& drive, int y, size_t index) {
                if (ui.storageIndex == index) c.rounded(352, y - 36, 1210, 67, 9, Blue, 190);
                const bool active = ui.storage.destination == (index ? InstallDestination::External : InstallDestination::Internal);
                c.text(std::string(active ? "[Selected]  " : "") + name, 370, y, 27, White, 650);
                c.text(!ui.storageChecked ? "Checking..." : drive.available && !drive.sizeKnown ? "Space unavailable" : drive.available ?
                       (drive.freeBytes ? shortSize(drive.freeBytes) : "0 B") + " free" : "Unavailable",
                       1030, y, 26, drive.available ? Accent : Muted, 510);
            };
            space("Internal storage", ui.storage.internal, 425, 0);
            space("External USB (Extended Storage)", ui.storage.external, 510, 1);
            c.text("Press X to apply. This also changes the PS4's default install location.", 370, 610, 25, Accent, 1180);
            c.text("A USB drive must already be configured as PS4 Extended Storage.", 370, 671, 24, Muted, 1180);
            c.text(ui.storage.error.empty() ? "Existing games stay on their current drive." : ui.storage.error, 370, 729, 24, Muted, 1180);
            hints(c, 370, 863, {{Button::Cross, "Select destination"}, {Button::Triangle, "Check storage"}, {Button::Circle, "Back"}}); break;
        }
        case Screen::Details: {
            panel(c, 230, 120, 1460, 828, "Game details");
            auto image=artwork.request(ui.pending,true);
            c.image(*(image ? image : placeholder),270,240,260,260);
            c.text(ui.pending.name,572,264,34,White,1055);
            const auto rows=ui.detailRows();
            ui.detailScroll=std::min(ui.detailScroll,rows.size()>12 ? rows.size()-12 : size_t(0));
            scrollIndicator(c,rows.size(),ui.detailScroll,12,1650,297,516);
            for(size_t i=ui.detailScroll;i<std::min(rows.size(),ui.detailScroll+12);++i) {
                const int y=322+int(i-ui.detailScroll)*43;
                if(i==ui.detailIndex)c.rounded(564,y-29,1070,39,6,Blue,rows[i].action ? 220 : 90);
                const auto label=rows[i].key.empty() ? "" : rows[i].key+": ";
                const auto width=std::min(440,c.textWidth(label,23)+1);
                c.boldText(label,574,y,23,White,width);
                c.text(rows[i].value,574+width,y,23,rows[i].action ? Accent : Muted,1040-width);
            }
            c.text("Up / Down to browse",270,555,20,Muted,270);
            c.text("X expands a selected section",270,590,18,Muted,280);
            if(!ui.pending.igdb.screenshots.empty()&&!ui.pending.igdb.videos.empty())
                hints(c,270,908,{{Button::Cross,"View / expand"},{Button::Triangle,"Screenshots"},{Button::Square,"Videos"},{Button::Circle,"Back"}},21);
            else if(!ui.pending.igdb.screenshots.empty())
                hints(c,270,908,{{Button::Cross,"View / expand"},{Button::Triangle,"Screenshots"},{Button::Circle,"Back"}},21);
            else if(!ui.pending.igdb.videos.empty())
                hints(c,270,908,{{Button::Cross,"View / expand"},{Button::Square,"Videos"},{Button::Circle,"Back"}},21);
            else hints(c,270,908,{{Button::Cross,"Expand"},{Button::Circle,"Back"}},21);
            break;
        }
        case Screen::Screenshots: {
            panel(c,110,165,1700,775,"Screenshots / "+ui.pending.name);
            const auto screenshot=ui.screenshot();
            const auto image=artwork.request(screenshot,true,true);
            if(image)c.image(*image,160,270,1600,560);
            else {
                const auto error=artwork.error(screenshot,true);
                c.centered(error.empty() ? "Loading image..." : "Could not load screenshot",500,30,Muted);
                if(!error.empty())c.text(error,210,555,24,Muted,1500);
                c.centered("Triangle retries the image. L1 / R1 changes screenshots. O goes back.",620,24,Muted);
            }
            c.text(std::to_string(ui.mediaIndex+1)+" / "+std::to_string(ui.pending.igdb.screenshots.size()),1580,904,23,Accent,160);
            hints(c,160,904,{{Button::L1,"Previous / next"},{Button::Triangle,"Reload"},{Button::Circle,"Back"}},21);break;
        }
        case Screen::Videos: {
            panel(c,380,185,1160,720,"Videos / "+ui.pending.name);
            menu(c,ui.videoRows(),ui.mediaIndex,420,338,1080,60,7);
            c.text("Opens the video in the PS4 browser.",430,810,23,Muted,1050);
            hints(c,430,866,{{Button::Cross,"Open video"},{Button::Circle,"Back"}},22);break;
        }
        case Screen::Actions: {
            const auto rows = ui.actionRows();
            panel(c, 495, 210, 930, 650, ui.pending.name);
            menu(c, rows, ui.menuIndex, 532, 356, 856, 58, 7);
            c.text("Up / Down  |  Page " + std::to_string(ui.menuIndex / 7 + 1) + " / " + std::to_string((rows.size() + 6) / 7), 548, 778, 20, Muted, 830);
            hints(c, 548, 815, {{Button::Cross, "Select"}, {Button::Circle, "Back"}}); break;
        }
        case Screen::Rating: {
            panel(c, 560, 190, 800, 680, "Rate game");
            c.text(ui.pending.name, 600, 328, 28, White, 720);
            std::vector<std::string> rows;
            for (unsigned i = 1; i <= 5; ++i) rows.push_back(std::to_string(i) + (i == 1 ? " star" : " stars") + (ui.pending.rating == i ? " (current)" : ""));
            rows.push_back("Remove rating");
            menu(c, rows, ui.ratingIndex, 598, 400, 724, 56);
            for (unsigned i = 1; i <= 5; ++i) stars(c, i, 1110, 382 + (i - 1) * 56, 20);
            c.text("Saved locally by Title ID. Catalog refreshes keep your rating.", 600, 770, 20, Muted, 720);
            hints(c, 600, 829, {{Button::Cross, "Save rating"}, {Button::Circle, "Cancel"}}); break;
        }
        case Screen::LocalInfo:
            panel(c, 350, 170, 1220, 740, "Edit local game info");
            c.text(ui.pending.name + "  /  " + ui.pending.titleId, 395, 307, 28, White, 1130);
            menu(c, ui.localRows(), ui.localIndex, 390, 385, 1140, 72);
            c.text(ui.localNotice.empty() ? "New category/tag names are created when you save. Empty text clears a field." : ui.localNotice, 395, 785, 22, Accent, 1130);
            hints(c, 395, 861, {{Button::Cross, "Edit / Save"}, {Button::Circle, "Back"}}); break;
        case Screen::PickCategory:
        case Screen::PickTags: {
            const bool tags = ui.screen == Screen::PickTags;
            panel(c, 470, 160, 980, 760, ui.bulkEditing ? (tags ? (ui.bulkOperation == 1 ? "Add tags to selected games" : "Remove tags from selected games") : "Assign category to selected games") : ui.quickAssignment ? (tags ? "Assign tags" : "Assign category") : (tags ? "Choose local tags" : "Choose local category"));
            c.text(ui.pending.name + (ui.pending.titleId.empty() ? "" : "  /  " + ui.pending.titleId), 510, 281, 22, Accent, 900);
            std::vector<std::string> rows{tags ? (ui.bulkEditing ? "Clear tag selection" : "Clear all game tags") : "Uncategorized"};
            for (const auto& term : ui.termChoices(tags, true)) {
                const bool active = tags ? std::any_of(ui.localDraft.tags.begin(), ui.localDraft.tags.end(), [&](const std::string& tag) { return lower(tag) == lower(term); }) : lower(term) == lower(ui.localDraft.category);
                rows.push_back(std::string(active ? "[Selected]  " : "") + term);
            }
            menu(c, rows, ui.pickerIndex, 510, 320, 900, 56, 8);
            const auto help = ui.bulkEditing && tags ? "Select tags to add/remove, then Square saves the whole batch." : ui.quickAssignment ? (tags ? "Toggle tags, then press Square to save. O cancels your changes." : "Choose a category to save it. Triangle enters a new name.") : (tags ? "X toggles a tag. O returns to the draft." : "Choose a category, or press Triangle to create one.");
            c.text(ui.localNotice.empty() ? help : ui.localNotice, 510, 788, 21, Muted, 900);
            if (ui.quickAssignment && tags)
                hints(c, 510, 863, {{Button::Cross, "Toggle"}, {Button::Square, "Save"}, {Button::Triangle, "New"}, {Button::Circle, "Cancel"}}, 22);
            else hints(c, 510, 863, {{Button::Cross, ui.quickAssignment ? "Assign" : "Select"}, {Button::Triangle, "New"}, {Button::Circle, ui.quickAssignment ? "Cancel" : "Back"}});
            break;
        }
        case Screen::Collections: {
            panel(c, 380, 140, 1160, 790, ui.collectionTags ? "Browse local tags" : "Browse local categories");
            c.text(ui.library == Library::Installed ? "Installed library" : "Available library", 425, 276, 23, Accent, 1070);
            menu(c, ui.collectionRows(), ui.collectionIndex, 420, 340, 1080, 56, 7);
            c.text(ui.collectionNotice.empty() ? "L1 / R1 switches categories and tags. Up / Down scrolls names." : ui.collectionNotice, 425, 792, 21, Muted, 1070);
            hints(c, 425, 857, {{Button::Cross, "Browse"}, {Button::Triangle, "New"}, {Button::Square, "Rename"}, {Button::Options, "Delete"}}, 21);
            hints(c, 425, 900, {{Button::Circle, "Back"}}, 21); break;
        }
        case Screen::ConfirmDeleteCollection:
            panel(c, 390, 280, 1140, 530, ui.collectionTags ? "Delete local tag?" : "Delete local category?");
            c.text(ui.collectionOriginal, 435, 429, 32, White, 1050);
            c.text("This removes the name and its assignments from all local records.", 435, 515, 24, Muted, 1050);
            c.text("Games, ratings, descriptions and installation history are kept.", 435, 566, 24, Muted, 1050);
            hints(c, 435, 749, {{Button::Cross, "Delete local name"}, {Button::Circle, "Cancel"}}); break;
        case Screen::DiscardLocal:
            panel(c, 495, 334, 930, 380, "Discard local info edits?");
            c.text("Your saved local info will be kept.", 545, 502, 26, Muted, 830);
            hints(c, 545, 652, {{Button::Circle, "Discard"}, {Button::Cross, "Keep editing"}}, 26); break;
        case Screen::ConfirmUninstall:
            panel(c, 360, 266, 1200, 548, "Uninstall game?");
            c.text(ui.pending.name, 410, 413, 33, White, 1100);
            c.text(ui.pending.titleId, 410, 467, 26, Accent, 1100);
            c.text("This removes the game's installed files.", 410, 545, 26, Muted, 1100);
            c.text("Your local rating and installation history will be kept.", 410, 591, 24, Muted, 1100);
            hints(c, 410, 753, {{Button::Cross, "Uninstall"}, {Button::Circle, "Cancel"}}, 26); break;
        case Screen::ConfirmInstall: {
            const auto required = ui.requirements();
            panel(c, 300, 140, 1320, 800, required.blockers.empty() ? "Install package?" : "Installation requirements");
            c.text(ui.pending.name, 350, 285, 33, White, 1220);
            c.text(shortSize(ui.pending.size) + "  " + ui.pending.titleId + "  /  " + packageKindName(ui.pending), 350, 333, 25, Accent, 1220);
            std::vector<std::string> lines = required.blockers;
            lines.insert(lines.end(), required.notes.begin(), required.notes.end());
            if (lines.empty()) lines.push_back("Submitted tasks appear in PS4 Notifications > Downloads.");
            int y = 400;
            for (auto line : lines) {
                while (!line.empty() && y <= 714) {
                    size_t count = std::min(size_t(85),line.size());
                    if (count < line.size()) { auto space = line.rfind(' ',count); if (space != std::string::npos && space > 20) count = space; }
                    while (count && count < line.size() && (static_cast<unsigned char>(line[count]) & 0xc0) == 0x80) --count;
                    if (!count) count=1;
                    c.text(line.substr(0,count), 350, y, 24, required.blockers.empty() ? Muted : Accent, 1220);
                    line.erase(0,count); if (!line.empty() && line[0]==' ') line.erase(0,1); y += 35;
                }
            }
            c.text("Keep the app open during direct downloads. Download completion is not installation.", 350, 790, 22, Muted, 1220);
            hints(c, 350, 886, {{Button::Cross, required.blockers.empty() ? "Confirm install" : "Requirements unmet"}, {Button::Triangle, "Game packages"}, {Button::Circle, "Cancel"}}, 23); break;
        }
        case Screen::InstallError: {
            panel(c, 330, 200, 1260, 680, "Installation failed");
            c.text(ui.failedInstall.name, 380, 347, 33, White, 1160);
            c.text("See the error below for details.", 380, 410, 26, Accent, 1160);
            std::string remaining = ui.installError;
            int baseline = 473;
            while (!remaining.empty() && baseline <= 737) {
                size_t length = std::min(size_t(78), remaining.size());
                if (length < remaining.size()) {
                    size_t space = remaining.rfind(' ', length);
                    if (space != std::string::npos && space > 15) length = space;
                    while (length && (static_cast<unsigned char>(remaining[length]) & 0xc0) == 0x80) --length;
                    if (!length) length = 1;
                }
                c.text(remaining.substr(0, length), 380, baseline, 25, Muted, 1160);
                remaining.erase(0, length);
                if (!remaining.empty() && remaining.front() == ' ') remaining.erase(0, 1);
                baseline += 44;
            }
            hints(c, 380, 827, {{Button::Cross, "Retry"}, {Button::Circle, "Close"}}, 26); break;
        }
        case Screen::ConfirmExit:
            panel(c, 495, 334, 930, 380, "Exit PS4 Games Installer?");
            c.text("Submitted PS4 installation tasks will continue.", 545, 502, 25, Muted, 830);
            c.text("An unfinished direct download will be cancelled.", 545, 549, 25, Muted, 830);
            hints(c, 545, 652, {{Button::Circle, "Confirm exit"}, {Button::Cross, "Stay"}}, 28); break;
        case Screen::Discard:
            panel(c, 495, 334, 930, 380, "Discard unsaved settings?");
            c.text("Your saved configuration will be kept.", 545, 502, 26, Muted, 830);
            hints(c, 545, 652, {{Button::Circle, "Discard"}, {Button::Cross, "Keep editing"}}, 28); break;
        case Screen::About:
            panel(c, 290, 210, 1340, 660, "About PS4 Games Installer");
            c.image(*placeholder, 345, 334, 180, 180);
            c.text("PS4 Games Installer " PSGI_VERSION, 570, 365, 37, White, 1000);
            c.text(Copyright, 570, 420, 25, Accent, 1000);
            c.text("An OpenOrbis game library, launcher and package installer.", 570, 477, 24, Muted, 1000);
            c.text("Works with a companion catalog and package server.", 345, 580, 27, White, 1230);
            c.text("Browse its library, view game artwork and install packages.", 345, 625, 24, Muted, 1230);
            c.text("https://github.com/e1z0/PS4-Games-Installer", 345, 700, 24, Accent, 1230);
            c.text("Native PS4 dashboard built with the OpenOrbis toolchain.", 345, 745, 24, Muted, 1230);
            hints(c, 345, 827, {{Button::Circle, "Back"}}); break;
        case Screen::Keyboard:
            panel(c, 300, 330, 1320, 400, keyboardTitle(ui.editKind));
            c.rounded(340, 466, 1240, 88, 10, 0x061c32, 255);
            c.text(ui.keyboardDisplayText() + "|", 367, 522, 28, White, 1185);
#ifdef PSGI_HOST
            c.text("Type with your keyboard. Enter saves; Escape cancels.", 350, 671, 24, Accent);
#else
            c.text("X selects a character. Choose Done on the keyboard to accept.", 350, 610, 24, Muted);
            hints(c, 350, 671, {{Button::Cross, "Select key"}, {Button::Circle, "Cancel"}});
#endif
            break;
        default: break;
    }
}
bool Renderer::featureDialog(Canvas& c, Ui& ui) {
    auto notice = [&](int y = 807) { c.text(ui.featureNotice, 380, y, 21, Accent, 1160); };
    auto wrap = [](const std::vector<std::string>& source) {
        std::vector<std::string> lines;
        for (auto line : source) {
            for (auto& ch : line) if (ch == '\n' || ch == '\r' || ch == '\t') ch = ' ';
            while (line.size() > 86) {
                size_t cut = line.rfind(' ', 86); if (cut == std::string::npos || cut < 20) cut = 86;
                while (cut && (static_cast<unsigned char>(line[cut]) & 0xc0) == 0x80) --cut;
                if (!cut) cut = 86;
                lines.push_back(line.substr(0, cut)); line.erase(0, cut); if (!line.empty() && line[0] == ' ') line.erase(0, 1);
            }
            lines.push_back(line);
        }
        return lines;
    };
    switch (ui.screen) {
        case Screen::Search:
            panel(c,300,110,1320,850,"Search library");
            c.text("Current: "+(ui.query.empty() ? std::string("All games") : ui.query),350,248,24,Accent,1220);
            menu(c,ui.searchRows(),ui.searchIndex,340,320,1240,53,8);
            c.text("Recent searches are remembered on this PS4. Search keeps your other filters.",350,778,22,Muted,1220);
            c.text(ui.browseNotice,350,822,21,Accent,1220);
            hints(c,350,906,{{Button::Cross,"Select"},{Button::Triangle,"Type search"},{Button::Square,"Clear history"},{Button::Circle,"Back"}},21); return true;
        case Screen::ConfirmClearSearches:
            panel(c,350,260,1220,550,"Clear recent searches?");
            c.text("This removes the remembered search terms from this PS4.",400,424,27,White,1120);
            c.text("Your current search and saved collections stay.",400,489,24,Muted,1120);
            hints(c,400,748,{{Button::Cross,"Clear history"},{Button::Circle,"Cancel"}}); return true;
        case Screen::JumpLetter:
            panel(c,390,100,1140,880,"Jump to letter");
            c.text("Jump within the current filtered view; sorting and grouping stay.",440,245,23,Muted,1040);
            if (ui.letterRows().empty()) c.text("No titles in this view. Clear your search or filters first.",440,350,26,White,1040);
            else menu(c,ui.letterRows(),ui.jumpIndex,430,320,1060,54,9);
            c.text("# includes numbers, symbols and names outside A-Z.",440,846,22,Muted,1040);
            hints(c,440,923,{{Button::Cross,"Jump"},{Button::L2,"Page"},{Button::Circle,"Back"}},22); return true;
        case Screen::ServerProfiles:
            panel(c,270,100,1380,880,"Server profiles");
            c.text("Selected in draft: "+(ui.draft.activeProfile.empty() ? std::string("Custom sources") : ui.draft.activeProfile),320,243,26,Accent,1280);
            c.text("Each profile remembers all Games / Updates / DLC catalog URLs.",320,287,23,Muted,1280);
            if (ui.draft.profiles.empty()) c.text("Press Triangle to save your current URLs as a named profile.",320,385,27,White,1280);
            else menu(c,ui.profileRows(),ui.profileIndex,310,365,1300,54,6);
            c.text("Use selects a profile in Settings. Save settings applies your changes.",320,741,22,Muted,1280);
            c.text(ui.browseNotice,320,789,21,Accent,1280);
            hints(c,320,866,{{Button::Cross,"Use"},{Button::Triangle,"Save copy"},{Button::Square,"Rename"},{Button::Options,"Delete"}},22);
            hints(c,320,927,{{Button::Circle,"Back"}},22); return true;
        case Screen::ConfirmDeleteProfile:
            panel(c,350,250,1220,570,"Delete server profile?");
            if (ui.profileIndex<ui.draft.profiles.size()) c.text(ui.draft.profiles[ui.profileIndex].name,400,417,31,White,1120);
            c.text("Only the saved profile is removed. Current URLs and queued tasks stay.",400,486,24,Muted,1120);
            c.text("Return to Settings and Save settings to keep the change.",400,541,24,Muted,1120);
            hints(c,400,759,{{Button::Cross,"Delete profile"},{Button::Circle,"Cancel"}}); return true;
        case Screen::Cleanup: {
            panel(c, 300, 130, 1320, 820, "Storage cleanup");
            c.text("Artwork cache: " + shortSize(ui.usage.artwork), 350, 290, 30, White, 1220);
            c.text("Package staging: " + shortSize(ui.usage.staging), 350, 352, 30, White, 1220);
            c.text("Safe to remove: " + shortSize(ui.usage.reclaimable) + " / " + std::to_string(ui.usage.removableFiles) + (ui.usage.removableFiles == 1 ? " file" : " files"), 350, 435, 28, Accent, 1220);
            c.text("Protected: " + shortSize(ui.usage.protectedBytes) + " / " + std::to_string(ui.usage.protectedFiles) + (ui.usage.protectedFiles == 1 ? " file" : " files"), 350, 494, 28, White, 1220);
            c.text("Packages needed by PS4 and files with unknown status are kept.", 350, 581, 24, Muted, 1220);
            c.text("Artwork is downloaded again when needed. Your games and records stay.", 350, 625, 24, Muted, 1220);
            c.text(ui.usage.checked ? ui.featureNotice : "Press Triangle to check storage usage.", 350, 769, 21, Accent, 1220);
            hints(c, 350, 858, {{Button::Cross, "Clean staging"}, {Button::Square, "Clear artwork"}, {Button::Triangle, "Refresh"}}, 22);
            hints(c, 350, 906, {{Button::Circle, "Back"}}, 22); return true;
        }
        case Screen::ConfirmCleanup:
            panel(c, 350, 250, 1220, 570, ui.cleanupArtwork ? "Clear downloaded artwork?" : "Remove released package files?");
            c.text(ui.cleanupArtwork ? "Game icons will be fetched again when needed." : "Only released packages and inactive partial downloads will be removed.", 400, 424, 26, White, 1120);
            c.text("Active or uncertain staged packages and all personal records are kept.", 400, 491, 24, Muted, 1120);
            c.text("Usage and queue ownership are checked again before removing files.", 400, 544, 24, Muted, 1120);
            hints(c, 400, 760, {{Button::Cross, "Clean"}, {Button::Circle, "Cancel"}}); return true;
        case Screen::Diagnostics: {
            panel(c, 240, 130, 1440, 820, "Export diagnostics to USB");
            c.text("Includes app revision, library counts, storage, queue stages and selected error codes.", 290, 270, 24, White, 1340);
            c.text("Raw logs, source URLs, credentials and personal records are omitted.", 290, 317, 24, Accent, 1340);
            std::vector<std::string> rows; for (const auto& device : ui.backupDevices) rows.push_back(device.path);
            if (rows.empty()) c.text("Connect a writable FAT/exFAT USB drive and press Triangle.", 290, 425, 28, White, 1340);
            else menu(c, rows, ui.backupIndex, 280, 420, 1360, 56, 5);
            c.text("Creates a separate JSON report; existing USB backups are kept.", 290, 738, 23, Muted, 1340);
            c.text(ui.featureNotice, 290, 804, 21, Accent, 1340);
            hints(c, 290, 899, {{Button::Cross, "Export report"}, {Button::Triangle, "Refresh drives"}, {Button::Circle, "Back"}}, 22); return true;
        }
        case Screen::BulkMenu:
            panel(c, 330, 130, 1260, 800, "Organize selected games");
            c.text(std::to_string(ui.markedTitles.size()) + " unique Title IDs selected across both libraries", 380, 275, 25, Accent, 1160);
            menu(c, ui.bulkRows(), ui.bulkIndex, 370, 350, 1180, 58);
            notice(); hints(c, 380, 882, {{Button::Cross, "Select"}, {Button::Circle, "Back to selection"}}); return true;
        case Screen::ConfirmBulkClear:
            panel(c, 350, 260, 1220, 550, "Clear tags on selected games?");
            c.text(std::to_string(ui.markedTitles.size()) + " games will have all local tag assignments removed.", 400, 425, 28, White, 1120);
            c.text("Categories, notes, ratings and history will be kept.", 400, 493, 24, Muted, 1120);
            c.text(ui.featureNotice, 400, 620, 22, Accent, 1120);
            hints(c, 400, 752, {{Button::Cross, "Clear tags"}, {Button::Circle, "Cancel"}}); return true;
        case Screen::SavedCollections:
            panel(c, 330, 120, 1260, 820, "Saved collections");
            c.text("Rules update automatically as your library and local records change.", 380, 260, 23, Muted, 1160);
            menu(c, ui.savedRows(), ui.savedIndex, 370, 330, 1180, 58, 7);
            notice(); hints(c, 380, 862, {{Button::Cross, "Browse"}, {Button::Triangle, "New"}, {Button::Square, "Edit"}, {Button::Options, "Delete"}}, 21);
            hints(c, 380, 906, {{Button::Circle, "Back"}}, 21); return true;
        case Screen::EditSaved:
            panel(c, 300, 100, 1320, 880, ui.savedOriginal.empty() ? "Create saved collection" : "Edit saved collection");
            menu(c, ui.savedEditRows(), ui.savedEditIndex, 340, 247, 1240, 56, 8);
            c.text("Launch history only covers launches requested through this app.", 350, 765, 21, Muted, 1220);
            c.text("Left / Right changes rules. Up / Down scrolls to Save collection.", 350, 806, 21, Muted, 1220);
            c.text(ui.featureNotice, 350, 851, 21, Accent, 1220);
            hints(c, 350, 928, {{Button::Cross, "Change / Save"}, {Button::Circle, "Cancel changes"}}); return true;
        case Screen::ConfirmDeleteSaved:
            panel(c, 380, 270, 1160, 540, "Delete saved collection?");
            c.text(ui.savedOriginal, 430, 425, 31, White, 1060);
            c.text("Only these saved rules will be removed. Your games and records stay.", 430, 500, 24, Muted, 1060);
            c.text(ui.featureNotice, 430, 620, 21, Accent, 1060);
            hints(c, 430, 752, {{Button::Cross, "Delete collection"}, {Button::Circle, "Cancel"}}); return true;
        case Screen::Queue: {
            panel(c, 180, 120, 1560, 820, "Download queue");
            size_t active = 0; for (const auto& job : ui.downloads) if (queueActive(job.stage)) ++active;
            c.text(std::to_string(active) + " active / " + std::to_string(ui.downloads.size()) + " tracked packages", 230, 260, 24, Accent, 1460);
            if (ui.downloads.empty()) c.text("Install a package from Available to add it here.", 230, 350, 29, White, 1460);
            else menu(c, ui.queueRows(), ui.queueIndex, 220, 330, 1480, 61, 7);
            c.text(ui.featureNotice.empty() ? "Downloading, installing and verifying are separate stages. Open an item for details." : ui.featureNotice, 230, 810, 21, Muted, 1460);
            hints(c, 230, 884, {{Button::Cross, "Details"}, {Button::Triangle, "Refresh"}, {Button::Options, "Clear finished"}, {Button::Circle, "Back"}}, 21); return true;
        }
        case Screen::QueueDetails: {
            panel(c, 240, 120, 1440, 840, "Package progress and diagnostics");
            const auto lines = wrap(ui.queueDetails());
            ui.detailScroll = std::min(ui.detailScroll, lines.size() > 11 ? lines.size() - 11 : size_t(0));
            scrollIndicator(c, lines.size(), ui.detailScroll, 11, 1644, 240, 473);
            for (size_t i = ui.detailScroll; i < std::min(lines.size(), ui.detailScroll + 11); ++i)
                c.text(lines[i], 290, 265 + (i - ui.detailScroll) * 43, 24, i == 2 ? Accent : White, 1340);
            c.text(ui.featureNotice, 290, 807, 21, Accent, 1340);
            c.text("Up / Down scrolls diagnostics", 290, 849, 20, Muted, 1340);
            if (ui.downloads.empty()) hints(c, 290, 910, {{Button::Circle, "Back"}}, 22);
            else {
                const auto& job = ui.downloads[std::min(ui.queueIndex, ui.downloads.size() - 1)];
                const auto action = queueActive(job.stage) || job.task >= 0 ? "Cancel task" : "Remove entry";
                if (job.stage == QueueStage::Failed || job.stage == QueueStage::Cancelled)
                    hints(c, 290, 910, {{Button::Cross, "Retry failed"}, {Button::Triangle, action}, {Button::Circle, "Back"}}, 22);
                else hints(c, 290, 910, {{Button::Triangle, action}, {Button::Circle, "Back"}}, 22);
            }
            return true;
        }
        case Screen::ConfirmCancelQueue: {
            const auto* job = findJob(ui.downloads, ui.queueActionUrl);
            panel(c, 350, 250, 1220, 570, ui.removingQueueEntry ? "Remove this queue entry?" : "Cancel this package?");
            if (job) c.text(job->entry.name, 400, 420, 31, White, 1120);
            c.text(ui.removingQueueEntry ? "This removes only this inactive queue history entry." : "This stops this app's owned download/installation task.", 400, 493, 24, Muted, 1120);
            c.text(ui.removingQueueEntry ? "Installed games, personal records and saved packages are kept." : "Installed games and unrelated PS4 downloads are kept.", 400, 543, 24, Muted, 1120);
            hints(c, 400, 762, {{Button::Cross, ui.removingQueueEntry ? "Remove entry" : "Cancel package"}, {Button::Circle, "Keep entry"}}); return true;
        }
        case Screen::Backups:
        case Screen::BackupFiles: {
            const bool files = ui.screen == Screen::BackupFiles;
            panel(c, 240, 110, 1440, 850, files ? "Restore personal records from USB" : "USB backup / restore");
            c.text("Ratings, tags, categories, notes, history and saved collections. Game files stay on PS4.", 290, 250, 23, Muted, 1340);
            std::vector<std::string> rows;
            if (files && !ui.backupDevices.empty()) rows = ui.backupDevices[std::min(ui.backupIndex, ui.backupDevices.size() - 1)].files;
            else for (const auto& device : ui.backupDevices) rows.push_back(device.path + " / " + std::to_string(device.files.size()) + (device.files.size() == 1 ? " backup" : " backups"));
            if (rows.empty()) c.text(files ? "No personal-record backups found on this drive." : "Connect a FAT/exFAT USB drive and press Triangle.", 290, 350, 28, White, 1340);
            else menu(c, rows, files ? ui.backupFileIndex : ui.backupIndex, 280, 330, 1360, 58, 7);
            c.text(ui.featureNotice, 290, 810, 21, Accent, 1340);
            if (files) hints(c, 290, 904, {{Button::Cross, "Choose backup"}, {Button::Triangle, "Refresh"}, {Button::Circle, "Back"}}, 22);
            else hints(c, 290, 904, {{Button::Cross, "Export new backup"}, {Button::Square, "Restore"}, {Button::Triangle, "Refresh"}, {Button::Circle, "Back"}}, 22);
            return true;
        }
        case Screen::ConfirmRestore:
            panel(c, 270, 200, 1380, 700, "Replace personal records with this backup?");
            c.text(ui.restoreFilename, 320, 350, 26, White, 1280); c.text(ui.restoreDevice, 320, 401, 23, Accent, 1280);
            c.text("This replaces local ratings, organization, history and saved collections.", 320, 491, 25, White, 1280);
            c.text("The current profile is saved as a recovery copy before replacement.", 320, 548, 24, Muted, 1280);
            c.text("The backup must be valid. Games, sources and downloaded packages are kept.", 320, 601, 24, Muted, 1280);
            c.text(ui.featureNotice, 320, 730, 21, Accent, 1280);
            hints(c, 320, 843, {{Button::Cross, "Restore records"}, {Button::Circle, "Cancel"}}); return true;
        case Screen::SourceSetup: {
            panel(c, 250, 100, 1420, 880, "Connection setup helper");
            c.text("Source: " + ui.setupSource.category, 300, 235, 24, Accent, 1320);
            menu(c, {"URL: " + (ui.setupSource.url.empty() ? "Enter a games JSON URL" : ui.setupSource.url), ui.sourceTesting ? "Testing..." : "Test this source", "Use tested source in settings"}, ui.setupIndex, 290, 315, 1340, 64);
            std::vector<std::string> info;
            if (ui.sourceReport.success) info.push_back(std::to_string(ui.sourceReport.packages) + " packages: " + std::to_string(ui.sourceReport.games) + " games / " + std::to_string(ui.sourceReport.updates) + " updates / " + std::to_string(ui.sourceReport.dlc) + " DLC");
            info.push_back(ui.featureNotice.empty() ? ui.sourceReport.message : ui.featureNotice);
            info.push_back("Enter the full URL that returns a JSON catalog with a DATA object.");
            info.push_back("Testing checks the catalog format and entries; package URLs are checked when installing.");
            const auto lines = wrap(info);
            for (size_t i = 0; i < std::min(size_t(7), lines.size()); ++i) c.text(lines[i], 300, 565 + i * 36, 23, i == 0 ? Accent : Muted, 1320);
            hints(c, 300, 921, {{Button::Cross, "Select"}, {Button::Triangle, "Edit URL"}, {Button::Square, "Test"}, {Button::Circle, "Back"}}, 22); return true;
        }
        case Screen::Packages:
            panel(c, 180, 120, 1560, 820, "Base game, updates and DLC");
            c.text(ui.packageTitle + " / Installed and catalog versions are compared when available.", 230, 260, 23, Accent, 1460);
            if (ui.packageRows().empty()) c.text("No remote packages for this Title ID.", 230, 350, 28, White, 1460);
            else menu(c, ui.packageRows(), ui.packageIndex, 220, 335, 1480, 62, 7);
            c.text("DLC shares the base Title ID; base-game presence does not confirm individual DLC ownership.", 230, 812, 21, Muted, 1460);
            hints(c, 230, 884, {{Button::Cross, "Install / Queue"}, {Button::Square, "Details"}, {Button::Triangle, "Actions"}, {Button::Circle, "Back"}}, 22); return true;
        default: return false;
    }
}

}
