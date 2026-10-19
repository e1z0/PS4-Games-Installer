#include "app.hpp"
#include "render.hpp"
#include "video.hpp"
#include "platform.hpp"
#include "settings.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <thread>

namespace {
using namespace psgi;
#ifdef PSGI_HOST
void capture(App& app, Renderer& renderer, Canvas& canvas) {
    const std::string directory = "build/screenshots";
    mkdir(directory.c_str(), 0755);
    auto& ui = app.ui;
    ui.settings.animations = true; // Captures demonstrate motion regardless of preview preferences.
    for (const auto& e : ui.available) app.artwork.request(e);
    for (int i = 0; i < 100; ++i) {
        bool ready = true;
        for (const auto& e : ui.available) if (!app.artwork.request(e)) ready = false;
        if (ready) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    double time = 12;
    auto shot = [&](const std::string& name) {
        renderer.draw(canvas, ui, app.artwork, time);
        time += 1;
        renderer.draw(canvas, ui, app.artwork, time);
        canvas.writePpm(directory + "/" + name + ".ppm");
        std::printf("Captured %s\n", name.c_str());
        time += 1;
    };
    ui.screen = Screen::Splash; shot("01-splash");
    ui.screen = Screen::Dashboard; ui.library = Library::Installed; ui.rebuild(); shot("02-installed-tiles");
    ui.library = Library::Available; ui.rebuild(); shot("03-available-tiles");
    shot("16-bubbles-tiles"); shot("17-bubbles-tiles-motion");
    ui.settings.layout = Layout::Summary; ui.rebuild(); shot("04-summary");
    shot("18-bubbles-summary"); shot("19-bubbles-summary-motion");
    ui.screen = Screen::Options; ui.menuIndex = 0; shot("05-options");
    ui.screen = Screen::Settings; ui.draft = ui.settings; ui.draft.sources[0].url = "http://192.168.1.10:3000/games"; shot("06-settings");
    ui.screen = Screen::Storage; shot("13-storage-available");
    ui.storage.external = {}; shot("14-storage-unavailable");
    ui.screen = Screen::Keyboard; ui.keyboardText = ui.draft.sources[0].url; shot("07-url-editor");
    ui.screen = Screen::ArchiveAccount; ui.archiveEmail = "player@example.com";
    ui.archivePassword = "preview-only-password"; ui.archiveIndex = 2; shot("93-archive-account");
    ui.screen = Screen::Keyboard; ui.editKind = EditKind::ArchivePassword;
    ui.keyboardText = ui.archivePassword; shot("94-archive-password");
    ui.archivePassword.clear(); ui.archiveEmail.clear(); ui.keyboardText.clear(); ui.editKind = EditKind::SourceUrl;
    ui.pending = ui.available[0]; ui.openDetails(Screen::Dashboard); shot("08-details");
    ui.pending = ui.available[1]; ui.openDetails(Screen::Dashboard); shot("85-details-igdb");
    ui.detailIndex=ui.detailRows().size()-2; ui.detailScroll=ui.detailIndex>10 ? ui.detailIndex-10 : 0; shot("86-details-collapsed");
    ui.detailsExpanded=true; shot("87-details-expanded");
    ui.technicalExpanded=true; ui.detailIndex=ui.detailRows().size()-1; ui.detailScroll=ui.detailIndex>10 ? ui.detailIndex-10 : 0; shot("88-details-technical");
    ui.screen=Screen::Videos;ui.mediaIndex=0;shot("89-videos");
    // Seed the normal disk cache with local preview art; the gallery follows
    // the same request/decode path as a previously cached remote screenshot.
    const auto media=ui.screenshot();
    app.artwork.refresh(media,true);
    // Refresh removes disk entries, so install the fixture after invalidation.
    atomicWrite("build/preview-data/artwork/"+artworkCacheName(artworkKey(media,true)),readFile("build/demo/1.png"));
    for(int i=0;i<100&&!app.artwork.request(media,true,true);++i)std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ui.screen=Screen::Screenshots;shot("90-screenshots-gallery");
    ui.screen=Screen::Dashboard;ui.library=Library::Available;ui.settings.layout=Layout::Tiles;
    ui.genresExpanded=true;ui.remoteGenre="Adventure";ui.rebuild();ui.sidebar=true;ui.sidebarIndex=ui.activeSidebarIndex();shot("91-genre-sidebar");
    ui.sidebarIndex=0;ui.handle(Button::Up);shot("92-genre-sidebar-bottom");
    ui.genresExpanded=false;ui.remoteGenre.clear();ui.sidebar=false;ui.updatesOnly=false;ui.rebuild();
    ui.library = Library::Installed; ui.rebuild();
    for (size_t i = 0; i < ui.visible.size(); ++i) if (ui.installed[ui.visible[i].index].titleId == ui.installed[0].titleId) ui.selected[0] = i;
    ui.pending = *ui.current(); ui.screen = Screen::Actions; ui.menuIndex = 0; shot("20-installed-actions");
    ui.screen = Screen::ConfirmUninstall; shot("21-uninstall-confirmation");
    ui.screen = Screen::Rating; ui.ratingIndex = 3; shot("22-rating");
    ui.localDraft = ui.pending.info; ui.localIndex = 2; ui.screen = Screen::LocalInfo; shot("24-local-info");
    ui.pickerIndex = 1; ui.screen = Screen::PickTags; shot("25-game-tags");
    ui.screen = Screen::Actions; ui.menuIndex = ui.actionRows().size() - 1; shot("30-actions-last-page");
    ui.quickAssignment = true; ui.screen = Screen::PickCategory; ui.pickerIndex = 1; shot("31-assign-category");
    ui.screen = Screen::PickTags; shot("32-assign-tags"); ui.quickAssignment = false;
    ui.library = Library::Available; ui.rebuild(); ui.pending = *ui.current();
    ui.screen = Screen::ConfirmInstall; shot("15-install-confirmation");
    ui.failedInstall = ui.pending;
    ui.installError = "PS4 refused installation (0x80990086). No matching task could be confirmed. Close the app, restart the PS4, enable GoldHEN and retry. Downloaded package kept.";
    ui.screen = Screen::InstallError; shot("23-installation-failed"); ui.installError.clear();
    ui.collectionTags = false; ui.collectionIndex = 2; ui.screen = Screen::Collections; shot("26-categories");
    ui.collectionTags = true; shot("27-tags");
    ui.screen = Screen::Dashboard; ui.settings.layout = Layout::Tiles; ui.settings.grouping = Grouping::LocalCategory; ui.selected[1] = 0; ui.rebuild(); shot("28-category-view");
    ui.settings.layout = Layout::Summary; ui.settings.grouping = Grouping::Tag; ui.rebuild(); shot("29-tag-view");
    ui.settings.grouping = Grouping::None; ui.rebuild();
    ui.bulkMode = true; ui.markedTitles = {ui.available[0].titleId, ui.available[1].titleId}; ui.screen = Screen::Dashboard; shot("33-bulk-selection");
    ui.screen = Screen::BulkMenu; ui.bulkIndex = 1; shot("34-bulk-actions");
    ui.quickAssignment = ui.bulkEditing = true; ui.bulkOperation = 1; ui.localDraft = {}; ui.localDraft.tags = {"Weekend"};
    ui.pending = {}; ui.pending.name = "2 selected games"; ui.pickerIndex = 3; ui.screen = Screen::PickTags; shot("35-bulk-tags");
    ui.quickAssignment = ui.bulkEditing = ui.bulkMode = false; ui.markedTitles.clear();
    ui.screen = Screen::SavedCollections; ui.savedIndex = 4; shot("36-saved-collections");
    ui.savedDraft = ui.player.collections.front(); ui.savedOriginal = ui.savedDraft.name; ui.savedEditIndex = 5; ui.screen = Screen::EditSaved; shot("37-collection-rules");
    ui.savedEditIndex = 9; shot("47-collection-save");
    ui.screen = Screen::Queue; ui.queueIndex = 1; shot("38-download-queue");
    ui.queueIndex = 3; ui.screen = Screen::QueueDetails; ui.detailScroll = 0; shot("39-queue-error-details");
    ui.queueIndex = 5; shot("48-queue-verification");
    ui.screen = Screen::Backups; ui.backupIndex = 0; shot("40-usb-backup");
    ui.restoreDevice = ui.backupDevices.front().path; ui.restoreFilename = ui.backupDevices.front().files.front(); ui.screen = Screen::ConfirmRestore; shot("41-restore-confirmation");
    ui.setupSource = {"games", "http://192.168.1.10:3000/games"}; ui.sourceReport = {}; ui.sourceReport.success = true; ui.sourceReport.source = ui.setupSource;
    ui.sourceReport.packages = 44; ui.sourceReport.games = 44; ui.featureNotice = "Valid catalog: 44 packages."; ui.screen = Screen::SourceSetup; ui.setupIndex = 2; shot("42-source-setup");
    ui.sourceReport.success = false; ui.featureNotice = "The server returned a web page. Enter the JSON catalog endpoint rather than its home page."; ui.setupIndex = 1; shot("43-source-error");
    ui.featureNotice.clear(); ui.packageTitle = ui.available.front().titleId; ui.packageIndex = 1; ui.screen = Screen::Packages; shot("44-game-packages");
    ui.settings.grouping = Grouping::Title; ui.settings.layout = Layout::Tiles; ui.screen = Screen::Dashboard;
    ui.rebuild();
    for (size_t i = 0; i < ui.visible.size(); ++i) if (ui.available[ui.visible[i].index].titleId == ui.packageTitle) { ui.selected[1] = i; break; }
    shot("45-package-groups");
    ui.screen = Screen::Options; ui.menuIndex = 12; shot("46-more-options");
    ui.settings.grouping = Grouping::None; ui.rebuild();
    ui.screen = Screen::Dashboard; ui.sidebar = true; ui.sidebarIndex = 2; ui.updatesOnly = true; ui.rebuild(); shot("49-sidebar-shortcuts");
    ui.sidebar = false; ui.updatesOnly = true; ui.library = Library::Available; ui.rebuild(); shot("50-newer-updates");
    ui.settings.layout = Layout::Summary; ui.rebuild(); shot("51-updates-summary");
    ui.updatesOnly = false; ui.settings.sort = SortField::Rating; ui.settings.descending = true; ui.rebuild(); shot("52-sort-ratings");
    ui.screen = Screen::Options; ui.menuIndex = 16; shot("53-sort-and-maintenance");
    ui.draft = ui.settings; ui.screen = Screen::Settings; ui.settingsIndex = 13; shot("54-settings-order");
    for (const auto& e : ui.available) if (packageKind(e) == 1) ui.pending = e;
    auto local = ui.installed; ui.installed.clear(); ui.screen = Screen::ConfirmInstall; shot("55-missing-base");
    QueueItem base; base.entry = local.front(); base.entry.url = "http://preview.invalid/queued-base.pkg"; base.stage = QueueStage::Waiting; ui.downloads.push_back(base);
    shot("56-wait-for-base"); ui.downloads.pop_back(); ui.installed = local; ui.rebuild();
    ui.screen = Screen::Cleanup; ui.featureNotice = "Storage checked. Active and uncertain packages kept."; shot("57-storage-cleanup");
    ui.screen = Screen::ConfirmCleanup; ui.cleanupArtwork = false; shot("58-confirm-cleanup");
    ui.screen = Screen::Diagnostics; ui.featureNotice.clear(); shot("59-usb-diagnostics");
    ui.featureNotice = "Diagnostics saved: PS4-Games-Installer-diagnostics-1792152000-0.json"; shot("60-diagnostics-exported");
    ui.featureNotice.clear(); ui.settings.sort = SortField::Name; ui.settings.descending = false; ui.rebuild();
    ui.screen=Screen::Search;ui.recentSearches={"DOOM","Weekend","CUSA10008","Adventure"};ui.searchIndex=2;shot("62-recent-searches");
    ui.screen=Screen::ConfirmClearSearches;shot("63-clear-searches");
    ui.screen=Screen::JumpLetter;ui.jumpIndex=3;shot("64-jump-letter");
    ui.jumpIndex=0;ui.handle(Button::R2);shot("69-jump-letter-next-page");
    ui.draft=ui.settings;ui.draft.sources={{"games","http://192.168.1.10:3000/games"},{"updates","http://192.168.1.10:3000/updates"},{"dlc","http://192.168.1.10:3000/dlc"}};
    addServerProfile(ui.draft,"Home NAS");ui.draft.sources={{"games","http://192.168.1.20:3000/games"}};addServerProfile(ui.draft,"Second server");
    ui.draft.sources={{"games","https://library.example.org/games"}};addServerProfile(ui.draft,"Remote library");useServerProfile(ui.draft,0);
    ui.screen=Screen::ServerProfiles;ui.profileIndex=0;ui.browseNotice="";shot("65-server-profiles");
    ui.screen=Screen::ConfirmDeleteProfile;ui.profileIndex=1;shot("66-delete-profile");
    ui.screen=Screen::Settings;ui.settingsIndex=16;shot("67-settings-profiles");
    ui.settings=ui.draft;ui.screen=Screen::Dashboard;ui.settings.layout=Layout::Tiles;ui.selected[1]=0;ui.rebuild();ui.pageJump(true);shot("68-page-jump");
    ui.screen = Screen::Options; ui.menuIndex = 0; ui.handle(Button::Up); shot("70-options-wrap-last");
    ui.handle(Button::Down); shot("71-options-wrap-first");
    ui.screen = Screen::Settings; ui.settingsIndex = 0; ui.handle(Button::Up); shot("72-settings-wrap-last");
    ui.screen = Screen::Dashboard; ui.settings.layout = Layout::Summary; ui.selected[1] = 0; ui.rebuild();
    ui.handle(Button::Up); shot("73-summary-wrap-last");
    ui.sidebar = true; ui.sidebarIndex = 0; ui.handle(Button::Up); shot("74-sidebar-wrap-last");
    ui.sidebar = false; ui.updatesOnly = false; ui.settings.layout = Layout::Tiles; ui.selected[1] = 0; ui.rebuild();
    ui.handle(Button::Up); shot("75-tiles-wrap-last");
    ui.screen = Screen::Options; ui.menuIndex = 7; ui.handle(Button::Down); shot("76-options-scroll-middle");
    auto savedSources = ui.draft;
    ui.draft = ui.settings;
    for (int i = 1; i <= 17; ++i) ui.draft.sources.push_back({"extra" + std::to_string(i), "http://192.168.1.10:3000/extra" + std::to_string(i)});
    ui.screen = Screen::Sources; ui.sourceIndex = 1; shot("77-sources-scroll-first");
    ui.handle(Button::Up); shot("78-sources-scroll-last"); ui.draft = savedSources;
    auto completedSample = ui.downloads[0];
    ui.downloads[0].stage = QueueStage::Verifying; ui.downloads[0].systemCompleted = true;
    verifyJob(ui.downloads[0], ui.installed, 1792182400);
    ui.screen = Screen::QueueDetails; ui.queueIndex = 0; ui.detailScroll = 0; shot("79-queue-native-complete");
    ui.downloads[0].stage = QueueStage::Verifying; ui.downloads[0].entry.version = "01.23";
    verifyJob(ui.downloads[0], ui.installed, 1792182400); shot("84-base-catalog-version-mismatch");
    ui.downloads[0] = completedSample;
    auto unknownSample = ui.downloads[5];
    ui.downloads[5].stage = QueueStage::Verifying; ui.downloads[5].systemCompleted = false; ui.downloads[5].verificationStarted = 1;
    verifyJob(ui.downloads[5], {}, 2); ui.queueIndex = 5; ui.detailScroll = 0; shot("80-queue-verification");
    for (int i = 0; i < 25; ++i) ui.handle(Button::Down);
    shot("81-queue-scroll-bottom"); ui.downloads[5] = unknownSample;
    ui.screen = Screen::QueueDetails; ui.queueIndex = 3; ui.detailScroll = 0;
    ui.handle(Button::Triangle); shot("82-remove-old-queue-entry"); ui.handle(Button::Circle);
    ui.queueIndex = 2; ui.handle(Button::Triangle); shot("83-cancel-active-task"); ui.handle(Button::Circle);
    ui.screen=Screen::Dashboard;ui.library=Library::Available;ui.sidebar=false;ui.updatesOnly=false;
    ui.remoteGenre.clear();ui.filter={};ui.query.clear();ui.settings.grouping=Grouping::None;ui.settings.layout=Layout::Tiles;
    ui.rebuild();
    for(size_t i=0;i<ui.visible.size();++i)if(ui.available[ui.visible[i].index].titleId=="CUSA10002")ui.selected[1]=i;
    shot("93-category-full-width");shot("94-category-scrolling");
    ui.pending=*ui.current();ui.openDetails(Screen::Dashboard);shot("95-modes-players-details");
    ui.pending.igdb.screenshots={{"Unavailable screenshot","http://127.0.0.1:1/screenshot.jpg"}};
    ui.mediaIndex=0;ui.screen=Screen::Screenshots;
    for(int i=0;i<200&&app.artwork.error(ui.screenshot(),true).empty();++i){
        app.artwork.request(ui.screenshot(),true,true);std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    shot("96-screenshot-unavailable");
    for(const auto& entry:ui.available)if(!entry.installed){ui.pending=entry;break;}
    ui.screen=Screen::Actions;ui.menuIndex=0;shot("97-available-screenshot-action");
    ui.screen = Screen::About; shot("09-about");
    ui.screen = Screen::ConfirmExit; shot("10-exit");
    ui.screen = Screen::Dashboard; ui.available.clear(); ui.updatesOnly = true; ui.rebuild(); shot("61-updates-empty");
    ui.updatesOnly = false; ui.rebuild(); shot("11-empty");
    ui.status = "Server unavailable. Showing the last valid library; Installed works offline.";
    ui.library = Library::Installed; ui.rebuild(); shot("12-offline");
}
#endif
}
static int runApplication(int argc, char** argv) {
    try {
#ifdef PSGI_HOST
        bool screenshots = argc > 1 && std::strcmp(argv[1], "--capture") == 0;
        const std::string assets = "assets", data = "build/preview-data";
#else
        (void)argc; (void)argv;
        const bool screenshots = false;
        const std::string assets = "/app0/assets";
#endif
        Canvas canvas(assets + "/font.ttf");
        Video video(screenshots);
        canvas.background(0, true);
        canvas.centered("Loading PS4 Games Installer", 560, 48, 0xf2f7ff);
        canvas.centered("Created by e1z0 (c) 2026 Vintage2000.org", 1016, 23, 0x9bbdd8);
        video.present(canvas);
        // Animation time starts at the first presented frame.
        const auto start = std::chrono::steady_clock::now();
        try {
        // libjbc changes the filesystem root. Read /app0 resources while the
        // application's sandbox paths are still accessible.
        Renderer renderer(assets);
        prepareHttpTrust();
        prepareConsole();
#ifdef PSGI_HOST
        App app(data, true);
#else
        App app(applicationDataDirectory());
#endif
#ifdef PSGI_HOST
        if (screenshots) { capture(app, renderer, canvas); return 0; }
#endif
        while (app.running()) {
            auto frameStart = std::chrono::steady_clock::now();
            double seconds = std::chrono::duration<double>(frameStart - start).count();
            video.pollKeyboard(app.ui);
            auto buttons = video.input(app.ui);
            if (app.ui.screen != Screen::Splash)
                for (Button button : buttons) app.ui.handle(button);
            app.tick(seconds, video);
            renderer.draw(canvas, app.ui, app.artwork, seconds);
            video.present(canvas);
            std::this_thread::sleep_until(frameStart + std::chrono::milliseconds(33));
        }
        } catch (const std::exception& e) {
#ifdef PSGI_HOST
            throw;
#else
            // Keep startup/runtime failures readable on a television until Circle closes them.
            Ui errorUi;
            bool stay = true;
            video.flushInput();
            while (stay) {
                canvas.background(0, false);
                canvas.centered("PS4 Games Installer could not continue", 430, 40, 0xf2f7ff);
                canvas.text(e.what(), 200, 520, 27, 0x9bbdd8, 1520);
                canvas.centered("Check GoldHEN and application storage, then try again.", 600, 26, 0x9bbdd8);
                canvas.centered("O Exit", 750, 28, 0x7ae5e0);
                video.present(canvas);
                for (Button button : video.input(errorUi)) if (button == Button::Circle) stay = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(33));
            }
            return 1;
#endif
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "PS4 Games Installer: %s\n", e.what());
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const int status = runApplication(argc, argv);
    // runApplication's scopes have closed the video/controller and joined all
    // workers before native credentials are restored and the shell takes over.
    return psgi::finishApplication(status);
}
