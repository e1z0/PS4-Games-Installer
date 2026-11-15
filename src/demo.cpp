#include "app.hpp"
#include "metadata.hpp"

namespace psgi {
std::vector<Entry> demoLibrary() {
#ifdef PSGI_HOST
    const char* names[] = {"DOOM", "Horizon Zero Dawn", "Journey", "Control", "Celeste", "No Man's Sky", "Death Stranding", "The Last Guardian", "ABZU", "Bloodborne", "God of War", "GRIS", "Hollow Knight", "Inside", "Ratchet & Clank", "Shadow of the Colossus"};
    std::vector<Entry> entries;
    for (int i = 0; i < 16; ++i) {
        Entry e;
        e.name = names[i]; e.titleId = "CUSA" + std::to_string(10000 + i);
        e.url = "http://preview.invalid/packages/" + std::to_string(i) + ".pkg";
        e.localIcon = "build/demo/" + std::to_string(i) + ".png";
        e.coverUrl = "http://preview.invalid/" + std::to_string(i) + ".png";
        e.region = i % 3 ? "EUR" : "USA";
        e.category = i % 5 ? "games" : "homebrew";
        e.serverFavorite = i == 0 || i == 2 || i == 9;
        e.size = i == 0 ? 32000000000ULL : uint64_t(2 + i * 3) * 1000000000;
        e.version = "1.00"; e.release = "2026"; e.minFirmware = "11.00";
        e.description = "Desktop preview fixture. Names, IDs and sizes are sample data; no packages are downloaded.";
        e.packageType="game";
        readIgdb(e,R"igdb({"schema_version":1,"publishers":["Example Publisher"],"developers":["Example Studio"],
          "genres":["Adventure","Shooter"],"themes":["Action","Science fiction"],
          "ps4_release_dates":[{"date":"2020-02-02","region":"worldwide"}],
          "ratings":{"users":{"score":84,"count":123}},
          "summary":"Explore a mysterious world and discover its story.",
          "storyline":"An explorer searches for a lost expedition, uncovering an ancient secret along the way.",
          "languages":{"interface":["English","French","German"],"voice":["English"]},
          "game_modes":["Single player","Co-operative"],
          "multiplayer":{"offline":{"supported":true,"max_players":2,"split_screen":true},"online":{"supported":false,"split_screen":false}},
          "media":{"screenshots":[{"url":"https://preview.invalid/screenshot.jpg"},{"url":"https://preview.invalid/screenshot2.jpg"}]},
          "videos":[{"name":"Official trailer (preview fixture)","video_id":"abcdefghijk"}]})igdb");
        if(i%3==1)e.igdb.genres={"Platform","Indie"};
        if(i%3==2)e.igdb.genres={"Role-playing (RPG)","Strategy","Puzzle","Racing","Sport","Simulator","Fighting"};
        e.metadata.emplace_back("APP_TYPE","1"); e.metadata.emplace_back("SYSTEM_VER","0x09000000");
        entries.push_back(e);
    }
    return entries;
#else
    return {};
#endif
}
}
