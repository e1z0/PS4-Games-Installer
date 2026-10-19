#pragma once
#include "platform.hpp"

namespace psgi {
using DownloadProgress = std::function<void(uint64_t, uint64_t)>;
enum class InstallPhase { Checking, Downloading, Registering, RegisteringRemote };
// Queue downloads always stage locally; BGFT receives one complete file at a time.
uint64_t downloadPackage(const Entry& entry, const std::string& dataDirectory,
                         const std::atomic<bool>* cancelled, const DownloadProgress& progress,
                         const std::function<void(const std::string&)>& note = {},
                         const ArchiveSession& session = {});
int installDownloadedPackage(const Entry& entry, uint64_t size, int userId, const std::string& dataDirectory);
// Probe the actual package, use BGFT for range-capable servers, otherwise stream
// to private staging storage and submit the complete local package to BGFT.
int installPackage(const Entry& entry, int userId, const std::string& dataDirectory,
                   const std::atomic<bool>* cancelled, const DownloadProgress& progress,
                   const std::function<void(const std::string&)>& note = {},
                   const std::function<void(InstallPhase)>& phase = {},
                   const ArchiveSession& session = {});
std::string stagedPackagePath(const std::string& dataDirectory, const std::string& url);
void removeStagedPackage(const std::string& dataDirectory, const std::string& url);
}
