#pragma once
#include "artwork.hpp"
#include "ui.hpp"
#include "archive_auth.hpp"
#include <functional>
#include <memory>

namespace psgi {
class Video;
class App {
    struct Worker;
    std::unique_ptr<Worker> worker;
    std::unique_ptr<Worker> monitor;
    std::string directory;
    Settings saved;
    ArchiveSession archiveSession;
    std::optional<ViewState> resume;
    bool playerWritable = true;
    bool queueWritable = true;
    std::mutex logMutex;
    bool demo = false, initialScan = true, bootReady = false, startRemote = false;
    bool quitting = false, cleaningStaging = false;
    double lastLocalScan = 0, lastPoll = 0;
    std::map<std::string, int> tasks;
    std::map<int, int> reportedTaskErrors;
    std::set<std::string> pendingCancels;
    std::string activeDownloadUrl;
    void refresh(bool remote);
    void log(const std::string& message);
    void saveTasks();
    void savePosition();
    void savePlayer(const PlayerData& next);
    void persistQueue();
    void syncQueued();
    void startNextDownload(int userId);
    std::set<std::string> protectedStaging() const;
public:
    Ui ui;
    ArtworkCache artwork;
    App(const std::string& dataDirectory, bool preview = false);
    ~App();
    void tick(double elapsed, Video& video);
    bool running() const { return !quitting; }
};
std::vector<Entry> demoLibrary();
}
