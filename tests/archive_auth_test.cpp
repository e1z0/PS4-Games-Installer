#include "archive_auth.hpp"
#include "settings.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

using namespace psgi;
namespace {
const std::string Login = R"({"success":true,"values":{"cookies":{"logged-in-user":"fixture%40example.org","logged-in-sig":"fixture-secret=123"},"s3":{"access":"ignored-access","secret":"ignored-secret"},"screenname":"ignored-name"}})";
template<class F> void rejects(F operation) {
    try { operation(); }
    catch (const std::exception& error) {
        const std::string message = error.what();
        assert(message.find("fixture") == std::string::npos);
        assert(message.find("ignored-secret") == std::string::npos);
        assert(message.find("example.org") == std::string::npos);
        return;
    }
    assert(false && "Expected a safe failure");
}
void privateFile(const std::string& path) {
    struct stat st{};
    assert(stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode));
    assert((st.st_mode & 0777) == 0600);
}
}
int main() {
    const auto session = parseArchiveLogin(Login);
    assert(session.user == "fixture%40example.org" && session.sig == "fixture-secret=123");
    assert(!session.empty() && ArchiveSession{}.empty());
    assert((ArchiveSession{"user", ""}.empty()));
    for (const std::string response : {
        "", "[]", "{", "{}", "{} trailing", R"({"success":false,"error":"fixture-secret"})",
        R"({"success":1,"values":{"cookies":{"logged-in-user":"fixture","logged-in-sig":"fixture-secret"}}})",
        R"({"success":true,"success":false,"values":{"cookies":{"logged-in-user":"fixture","logged-in-sig":"fixture-secret"}}})",
        R"({"success":true,"values":{"cookies":{}}})",
        R"({"success":true,"values":{"cookies":{"logged-in-user":"fixture","logged-in-sig":null}}})",
        R"({"success":true,"values":{"cookies":{"logged-in-user":"fixture","logged-in-sig":""}}})",
        R"({"success":true,"values":{"cookies":{"logged-in-user":"fixture","logged-in-sig":"secret","logged-in-sig":"other"}}})",
        R"({"success":true,"values":{"cookies":{"logged-in-user":"fixture\u0000hidden","logged-in-sig":"secret"}}})",
        R"({"success":true,"values":{"cookies":{"logged-in-user":"fixture","logged-in-sig":"secret\r\nX-Leak: yes"}}})",
        R"({"success":true,"values":{"cookies":{"logged-in-user":"fixture; another=cookie","logged-in-sig":"secret"}}})",
        R"({"success":true,"values":{"cookies":{"logged-in-user":"fixture","logged-in-sig":"secret,other"}}})"
    }) rejects([&] { parseArchiveLogin(response); });
    rejects([&] { parseArchiveLogin(Login + std::string(1, '\0')); });
    rejects([&] { parseArchiveLogin(std::string(65537, 'x')); });

    const auto expected = "logged-in-user=" + session.user + "; logged-in-sig=" + session.sig;
    for (const std::string url : {
        "https://archive.org", "https://archive.org/download/file.pkg", "https://archive.org:443/file",
        "HTTPS://ARCHIVE.ORG/file", "https://ia800001.us.archive.org/download/file.pkg",
        "https://dn710101.ca.archive.org:443/cover.jpg", "https://archive.org/download/@file?x=1"
    }) assert(archiveCookieForUrl(url, session) == expected);
    for (const std::string url : {
        "", "archive.org", "http://archive.org/file", "ftp://archive.org/file", "https://example.org/",
        "https://evilarchive.org/file", "https://archive.org.example.org/file", "https://archive.org:444/file",
        "https://archive.org:80/file", "https://archive.org:/file", "https://archive.org:443:443/file",
        "https://fixture@archive.org/file", "https://archive.org@evil.example/file",
        "https://archive.org\\@evil.example/file", "https://archive.org./file", "https://.archive.org/file",
        "https://a..archive.org/file", "https://-a.archive.org/file", "https://a-.archive.org/file",
        "https://a_b.archive.org/file", "https://%61rchive.org/file", "https://archive.org%00.evil/file",
        "https://archive.org\r\nHost: evil.example/file", "https://[::1]/file"
    }) assert(archiveCookieForUrl(url, session).empty());
    assert(archiveCookieForUrl("https://archive.org/", {}).empty());
    rejects([&] { archiveCookieForUrl("https://archive.org/", {"fixture", ""}); });
    for (const std::string value : {"fixture secret", "fixture\tsecret", "fixture\r\nsecret",
                                    "fixture;secret", "fixture,secret", "fixture\"secret", "fixture\\secret",
                                    "fixture\x7fsecret", "fixture\xc3\xa9secret"})
        rejects([&] { archiveCookieForUrl("https://archive.org/", {"fixture", value}); });
    rejects([&] { archiveCookieForUrl("https://archive.org/", {"fixture", std::string(4097, 'x')}); });
    rejects([&] { archiveCookieForUrl("https://archive.org/", {"fixture", std::string("a\0b", 3)}); });

    char temporary[] = "/tmp/psgi-archive-auth-XXXXXX";
    assert(mkdtemp(temporary));
    const std::string directory = std::string(temporary) + "/private";
    const auto path = directory + "/archive-session.json";
    assert(loadArchiveSession(directory).empty());
    const auto previousMask = umask(0077);
    saveArchiveSession(directory, session);
    umask(previousMask);
    privateFile(path);
    const auto stored = loadArchiveSession(directory);
    assert(stored.user == session.user && stored.sig == session.sig);
    const auto encoded = readFile(path);
    assert(encoded.find("ignored-") == std::string::npos && encoded.find("password") == std::string::npos);
    assert(std::distance(std::filesystem::directory_iterator(directory), std::filesystem::directory_iterator{}) == 1);
    rejects([&] { saveArchiveSession(directory, {"fixture;injected", "fixture-secret"}); });
    assert(readFile(path) == encoded);
    // Replacing an existing file or stale temporary file keeps permissions private.
    assert(chmod(path.c_str(), 0644) == 0);
    atomicWrite(path + ".tmp", "stale fixture-secret");
    assert(chmod((path + ".tmp").c_str(), 0644) == 0);
    saveArchiveSession(directory, session);
    privateFile(path);
    assert(!std::filesystem::exists(path + ".tmp"));
    for (const std::string invalid : {"fixture-secret", "{}", R"({"schema":2,"logged-in-user":"fixture","logged-in-sig":"fixture-secret"})",
                                      R"({"schema":1,"logged-in-user":"fixture","logged-in-sig":"fixture-secret\u0000hidden"})"}) {
        atomicWrite(path, invalid);
        rejects([&] { loadArchiveSession(directory); });
    }
    atomicWrite(path, std::string(16385, 'x'));
    rejects([&] { loadArchiveSession(directory); });
    clearArchiveSession(directory);
    assert(loadArchiveSession(directory).empty());
    // Never read credentials through a substituted symbolic link.
    atomicWrite(directory + "/other.json", encoded);
    assert(symlink("other.json", path.c_str()) == 0);
    rejects([&] { loadArchiveSession(directory); });
    clearArchiveSession(directory);
    assert(readFile(directory + "/other.json") == encoded);
    saveArchiveSession(directory, session);
    atomicWrite(path + ".tmp", "stale fixture-secret");
    clearArchiveSession(directory);
    clearArchiveSession(directory);
    assert(loadArchiveSession(directory).empty() && !std::filesystem::exists(path + ".tmp"));
    rejects([&] { saveArchiveSession(directory + "/missing/child", session); });
    assert(mkdir(path.c_str(), 0700) == 0);
    rejects([&] { loadArchiveSession(directory); });
    rejects([&] { clearArchiveSession(directory); });
    std::filesystem::remove_all(temporary);
    std::puts("Archive.org session parsing, origin scoping and private persistence tests passed");
}
