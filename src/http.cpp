#include "platform.hpp"
#include "http_redirect.hpp"
#include "https_trust.hpp"
#include "log.hpp"
#include <stdexcept>
#include <cstdio>
#include <memory>
#include <mutex>
#include <exception>

#ifndef PSGI_CA_BUNDLE_PATH
#ifdef PSGI_HOST
#define PSGI_CA_BUNDLE_PATH "assets/cacert.pem"
#else
#define PSGI_CA_BUNDLE_PATH "/app0/assets/cacert.pem"
#endif
#endif

namespace {
// Best-effort removal of temporary credentials and login replies from our buffers.
struct SensitiveText {
    std::string value;
    ~SensitiveText() {
        volatile char* data = value.empty() ? nullptr : &value[0];
        for (size_t i = 0; i < value.size(); ++i) data[i] = 0;
    }
};
void appendFormValue(std::string& output, const std::string& value) {
    constexpr char hex[] = "0123456789ABCDEF";
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') output += c;
        else { output += '%'; output += hex[c >> 4]; output += hex[c & 15]; }
    }
}
std::string statusFailure(const std::string& label, int status, bool authenticated) {
    if (authenticated && (status == 401 || status == 403))
        return "Internet Archive denied this download. Sign in again and confirm access to this file.";
    return label + " HTTP " + std::to_string(status) + "; check the URL on your server";
}

const std::vector<std::string>& trustedCertificates() {
    static std::mutex m;
    static std::unique_ptr<std::vector<std::string>> certificates;
    std::lock_guard<std::mutex> lock(m);

    if (!certificates) {
        const std::string bundlePath = PSGI_CA_BUNDLE_PATH;
        auto roots = psgi::parseTrustedCertificates(psgi::readFile(bundlePath, 1024 * 1024));
        const auto slash = bundlePath.find_last_of('/');
        const auto directory = slash == std::string::npos ? std::string() : bundlePath.substr(0, slash + 1);
        // This exact CA-published intermediate is verified/pinned by the asset tests.
        // Older firmware can reject GoDaddy's supplied cross-signed root path.
        auto goDaddyCert = psgi::parseTrustedCertificates(psgi::readFile(directory + "godaddy-secure-g2.pem", 16 * 1024));
        if (goDaddyCert.size() != 1) throw std::runtime_error("Expected one GoDaddy compatibility certificate");
        roots.insert(roots.begin(), std::move(goDaddyCert.front()));

        auto letsEncryptCert = psgi::parseTrustedCertificates(psgi::readFile(directory + "letsencrypt-gen-y.pem", 16 * 1024));
        if (letsEncryptCert.size() != 2) throw std::runtime_error("Expected two or more Let's Encrypt compatibility certificates");
        roots.insert(roots.begin(), std::move(letsEncryptCert.front()));

        certificates = std::make_unique<decltype(roots)>(roots);
    }
    return *certificates;
}
}
namespace psgi {
void prepareHttpTrust() { (void)trustedCertificates(); }
}

#ifdef PSGI_HOST
#include <curl/curl.h>
#include <chrono>
#include <cstdlib>
namespace {
struct Headers {
    std::string text;
    bool failed = false;
};
size_t receiveHeaders(char* data, size_t size, size_t count, void* userdata) {
    auto& headers = *static_cast<Headers*>(userdata);
    const size_t n = size * count;
    try {
        // Discard interim/proxy response blocks; Location belongs to the final one.
        if (n >= 5 && std::string(data, 5) == "HTTP/") headers.text.clear();
        if (n > psgi::MaxHttpHeaderBytes - headers.text.size()) { headers.failed = true; return 0; }
        headers.text.append(data, n);
        return n;
    } catch (...) { headers.failed = true; return 0; }
}
}
namespace psgi {
static HttpTransferInfo performTransfer(const std::string& url, const HttpSink& sink, bool probeRange,
                                       const std::atomic<bool>* cancelled, bool, bool document,
                                       const ArchiveSession* session = nullptr, const std::string* loginBody = nullptr) {
    static const CURLcode initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (initialized != CURLE_OK) throw std::runtime_error("curl initialization failed");
    const std::string label = loginBody ? "Internet Archive sign-in" : document ? "Catalog" : "Package";
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    std::string current = url;
    for (unsigned redirects = 0;; ++redirects) {
        if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
        std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
        if (!curl) throw std::runtime_error("curl allocation failed");
        Headers headers;
        struct Stream {
            CURL* curl; const HttpSink& sink; const std::atomic<bool>* cancelled;
            const std::string& label; bool range, authenticated = false;
            bool stopped = false; std::exception_ptr error;
        } stream{curl.get(), sink, cancelled, label, probeRange, false, false, {}};
        curl_easy_setopt(curl.get(), CURLOPT_URL, current.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "http,https");
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_LIMIT, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_TIME, 10L);
        curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "PS4-Games-Installer/1.0");
        SensitiveText cookie{session ? archiveCookieForUrl(current, *session) : std::string()};
        stream.authenticated = !cookie.value.empty();
        if (!cookie.value.empty()) curl_easy_setopt(curl.get(), CURLOPT_COOKIE, cookie.value.c_str());
        if (loginBody) {
            curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, loginBody->data());
            curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(loginBody->size()));
        }
        // Match curl's CLI trust override for the desktop checker/local TLS tests.
        // The console always uses its packaged, pinned trust bundle below.
        if (const char* bundle = std::getenv("CURL_CA_BUNDLE"))
            curl_easy_setopt(curl.get(), CURLOPT_CAINFO, bundle);
        if (document) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
            if (remaining <= 0) throw std::runtime_error("Catalog download timed out");
            curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT_MS, static_cast<long>(remaining));
        }
        if (probeRange) curl_easy_setopt(curl.get(), CURLOPT_RANGE, "0-3");
        curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, receiveHeaders);
        curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, &headers);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &stream);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, +[](char* data, size_t size, size_t count, void* ptr) -> size_t {
            auto& s = *static_cast<Stream*>(ptr);
            try {
                long status = 0; curl_off_t total = -1;
                curl_easy_getinfo(s.curl, CURLINFO_RESPONSE_CODE, &status);
                if (isHttpRedirect(static_cast<int>(status))) return 0;
                if (status != 200 && !(s.range && status == 206))
                    throw std::runtime_error(statusFailure(s.label, static_cast<int>(status), s.authenticated));
                curl_easy_getinfo(s.curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &total);
                if (s.cancelled && *s.cancelled) throw std::runtime_error("Download cancelled");
                if (!s.sink(data, size * count, total > 0 ? static_cast<uint64_t>(total) : 0)) { s.stopped = true; return 0; }
                return size * count;
            } catch (...) { s.error = std::current_exception(); return 0; }
        });
        curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &stream);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, +[](void* ptr, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
            auto& s = *static_cast<Stream*>(ptr); return s.cancelled && *s.cancelled ? 1 : 0;
        });
        const auto result = curl_easy_perform(curl.get());
        if (stream.error) std::rethrow_exception(stream.error);
        if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
        if (headers.failed) throw std::runtime_error("HTTP response headers exceed limit or cannot be read");
        long status = 0; curl_off_t length = -1;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
        curl_easy_getinfo(curl.get(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &length);
        const bool redirect = isHttpRedirect(static_cast<int>(status));
        if (result != CURLE_OK && !((stream.stopped || redirect) && result == CURLE_WRITE_ERROR))
            throw std::runtime_error(label + " download failed: " + curl_easy_strerror(result));
        if (redirect) {
            if (loginBody) throw std::runtime_error("Internet Archive sign-in redirects are not allowed");
            current = nextHttpUrl(current, redirectLocation(headers.text), redirects);
            continue;
        }
        if (status != 200 && !(probeRange && status == 206))
            throw std::runtime_error(statusFailure(label, static_cast<int>(status), stream.authenticated));
        return {static_cast<int>(status), length > 0 ? static_cast<uint64_t>(length) : 0, current};
    }
}
}
#else
#include "http_native.hpp"
#include <orbis/Http.h>
#include <orbis/Net.h>
#include <orbis/NetCtl.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <orbis/libkernel.h>

namespace {
std::string nativeHex(uint32_t code) {
    char value[11]; std::snprintf(value, sizeof(value), "0x%08X", code);
    return std::string(value);
}
void check(int rc, const char* operation) {
    if (rc < 0) throw std::runtime_error(std::string(operation) + " (" + nativeHex(rc) + ")");
}
struct HttpApi {
    decltype(&sceHttpInit) init = nullptr;
    decltype(&sceHttpTerm) term = nullptr;
    decltype(&sceHttpCreateTemplate) createTemplate = nullptr;
    decltype(&sceHttpDeleteTemplate) deleteTemplate = nullptr;
    decltype(&sceHttpCreateConnectionWithURL) createConnection = nullptr;
    decltype(&sceHttpDeleteConnection) deleteConnection = nullptr;
    decltype(&sceHttpCreateRequestWithURL) createRequest = nullptr;
    decltype(&sceHttpDeleteRequest) deleteRequest = nullptr;
    decltype(&psgiHttpSetAutoRedirect) setAutoRedirect = nullptr;
    decltype(&psgiHttpSetRecvTimeOut) setRecvTimeout = nullptr;
    decltype(&sceHttpSetConnectTimeOut) setConnectTimeout = nullptr;
    decltype(&sceHttpSetResolveTimeOut) setResolveTimeout = nullptr;
    decltype(&sceHttpSetSendTimeOut) setSendTimeout = nullptr;
    decltype(&sceHttpSendRequest) send = nullptr;
    decltype(&sceHttpGetStatusCode) getStatus = nullptr;
    decltype(&sceHttpReadData) read = nullptr;
    decltype(&sceHttpAbortRequest) abort = nullptr;
    decltype(&sceHttpAddRequestHeader) addHeader = nullptr;
    decltype(&sceHttpGetResponseContentLength) contentLength = nullptr;
    decltype(&sceHttpGetAllResponseHeaders) responseHeaders = nullptr;
    decltype(&sceHttpsLoadCert) loadCert = nullptr;
    decltype(&sceHttpsSetSslCallback) setSslCallback = nullptr;
    decltype(&psgiHttpsGetSslError) sslError = nullptr;
    decltype(&sceHttpGetLastErrno) lastErrno = nullptr;

    template<class T> void bind(int module, const char* name, T& function) {
        check(sceKernelDlsym(module, name, reinterpret_cast<void**>(&function)), name);
        if (!function) throw std::runtime_error(std::string("Missing HTTP function: ") + name);
    }
    void load() {
        // Resolve every operation from the same module instance as init/cleanup.
        static const int module = [] {
            int handle = sceKernelLoadStartModule("/system/common/lib/libSceHttp.sprx", 0, nullptr, 0, nullptr, nullptr);
            check(handle, "Load HTTP client");
            return handle;
        }();
        bind(module, "sceHttpInit", init);
        bind(module, "sceHttpTerm", term);
        bind(module, "sceHttpCreateTemplate", createTemplate);
        bind(module, "sceHttpDeleteTemplate", deleteTemplate);
        bind(module, "sceHttpCreateConnectionWithURL", createConnection);
        bind(module, "sceHttpDeleteConnection", deleteConnection);
        bind(module, "sceHttpCreateRequestWithURL", createRequest);
        bind(module, "sceHttpDeleteRequest", deleteRequest);
        bind(module, "sceHttpSetAutoRedirect", setAutoRedirect);
        bind(module, "sceHttpSetRecvTimeOut", setRecvTimeout);
        bind(module, "sceHttpSetConnectTimeOut", setConnectTimeout);
        bind(module, "sceHttpSetResolveTimeOut", setResolveTimeout);
        bind(module, "sceHttpSetSendTimeOut", setSendTimeout);
        bind(module, "sceHttpSendRequest", send);
        bind(module, "sceHttpGetStatusCode", getStatus);
        bind(module, "sceHttpReadData", read);
        bind(module, "sceHttpAbortRequest", abort);
        bind(module, "sceHttpAddRequestHeader", addHeader);
        bind(module, "sceHttpGetResponseContentLength", contentLength);
        bind(module, "sceHttpGetAllResponseHeaders", responseHeaders);
        bind(module, "sceHttpsLoadCert", loadCert);
        bind(module, "sceHttpsSetSslCallback", setSslCallback);
        bind(module, "sceHttpsGetSslError", sslError);
        bind(module, "sceHttpGetLastErrno", lastErrno);
    }
};
struct Network {
    HttpApi api;
    int pool = -1, ssl = -1, http = -1, caLoadResult = -1;
    std::vector<std::string> certificates;
    std::vector<SslPem> pem;
    std::vector<SslPem*> roots;
    void cleanup() {
        if (http >= 0) { api.term(http); http = -1; }
        if (ssl >= 0) { psgiSslTerm(ssl); ssl = -1; }
        if (pool >= 0) { sceNetPoolDestroy(pool); pool = -1; }
    }
    Network() {
        certificates = trustedCertificates();
        // libSceHttp takes an array of SslPem pointers. Keep all backing storage
        // stable until the HTTP context has been terminated.
        pem.reserve(certificates.size()); roots.reserve(certificates.size());
        for (auto& certificate : certificates) pem.push_back({certificate.data(), certificate.size()});
        for (auto& certificate : pem) roots.push_back(&certificate);
        try {
            check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET), "Load Net");
            check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NETCTL), "Load NetCtl");
            check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL), "Load SSL");
            check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP), "Load HTTP");
            api.load();
            check(sceNetInit(), "Net init");
            check(sceNetCtlInit(), "NetCtl init");
            pool = sceNetPoolCreate("psgi", 1024 * 1024, 0); check(pool, "Network pool");
            ssl = sceSslInit(1024 * 1024); check(ssl, "SSL init");
            http = api.init(pool, ssl, 1024 * 1024); check(http, "HTTP init");
            caLoadResult = api.loadCert(http, static_cast<int32_t>(roots.size()), roots.data(), nullptr, nullptr);
            check(caLoadResult, "Load HTTPS trust certificates");
        } catch (...) { cleanup(); throw; }
    }
    ~Network() { cleanup(); }
};
struct Request {
    HttpApi& api;
    int tmpl = -1, conn = -1, req = -1;
    ~Request() {
        if (req >= 0) api.deleteRequest(req);
        if (conn >= 0) api.deleteConnection(conn);
        if (tmpl >= 0) api.deleteTemplate(tmpl);
    }
};
Network& network() {
    static std::mutex m;
    static std::unique_ptr<Network> instance;
    std::lock_guard<std::mutex> lock(m);
    if (!instance)
        instance = std::make_unique<Network>();

    return *instance;
}
void setRequestPolicy(HttpApi& api, int id) {
    check(api.setAutoRedirect(id, 0), "Disable automatic redirects");
    check(api.setRecvTimeout(id, 10000000), "Set receive timeout");
    check(api.setConnectTimeout(id, 10000000), "Connect timeout");
    check(api.setResolveTimeout(id, 10000000), "DNS timeout");
    check(api.setSendTimeout(id, 10000000), "Send timeout");
}
std::string requestLocation(HttpApi& api, int request) {
    char* headers = nullptr; size_t length = 0;
    check(api.responseHeaders(request, &headers, &length), "Read HTTP redirect headers");
    if (!headers || length > psgi::MaxHttpHeaderBytes) throw std::runtime_error("Invalid HTTP redirect headers");
    // Firmware may include the string terminator in its reported buffer size.
    if (length && headers[length - 1] == '\0') --length;
    return psgi::redirectLocation(std::string(headers, length));
}
std::string nativeRequestDiagnostics(Network& net, int request, bool https) {
    std::string details;
    if (https) {
        int error = 0; uint32_t verify = 0;
        const int result = net.api.sslError(request, &error, &verify);
        details = result < 0 ? " ssl_query=" + nativeHex(result)
                            : " ssl=" + nativeHex(error) + " verify=" + nativeHex(verify);
    }
    int number = 0;
    const int result = net.api.lastErrno(request, &number);
    details += result < 0 ? " errno_query=" + nativeHex(result) : " errno=" + std::to_string(number);
    return details + " ca=" + std::to_string(net.roots.size()) +
        " ca_load=" + nativeHex(net.caLoadResult) + " keepalive=1";
}
int32_t publicImageCertificate(int32_t, uint32_t, void* const[], int32_t, void*) {
    // Preserve the existing compatibility exception for public IGDB artwork.
    // Catalogs and packages never install a verification-bypass callback.
    return 1;
}
}

namespace psgi {
static HttpTransferInfo performTransfer(const std::string& url, const HttpSink& sink, bool probeRange,
                                       const std::atomic<bool>* cancelled, bool artwork, bool document,
                                       const ArchiveSession* session = nullptr, const std::string* loginBody = nullptr) {
    auto& net = network(); auto& api = net.api;
    const std::string label = loginBody ? "Internet Archive sign-in" : document ? "Catalog" : "Package";
    const uint64_t deadline = sceKernelGetProcessTime() + 30000000;
    std::string current = url;
    for (unsigned redirects = 0;; ++redirects) {
        if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
        if (document && sceKernelGetProcessTime() > deadline) throw std::runtime_error("Catalog download timed out");
        Request r{api};
        r.tmpl = api.createTemplate(net.http, "PS4-Games-Installer/1.0", ORBIS_HTTP_VERSION_1_1, 0);
        check(r.tmpl, "HTTP template"); setRequestPolicy(api, r.tmpl);
        // Evaluate the artwork exception for each hop, so redirects cannot carry
        // this legacy exception to a different origin or path.
        constexpr char imageOrigin[] = "https://images.igdb.com/igdb/image/upload/";
        if (artwork && current.compare(0, sizeof(imageOrigin) - 1, imageOrigin) == 0)
            check(api.setSslCallback(r.tmpl, publicImageCertificate, nullptr), "Image HTTPS compatibility");
        // Use the PS4 keepalive path to retain custom HTTPS trust (see DEVELOPMENT.md).
        // Request still closes this connection after each transfer or redirect hop.
        r.conn = api.createConnection(r.tmpl, current.c_str(), true); check(r.conn, "HTTP connection");
        r.req = api.createRequest(r.conn, loginBody ? ORBIS_METHOD_POST : ORBIS_METHOD_GET,
                                  current.c_str(), loginBody ? loginBody->size() : 0); check(r.req, "HTTP request");
        try {
            SensitiveText cookie{session ? archiveCookieForUrl(current, *session) : std::string()};
            if (!cookie.value.empty()) check(api.addHeader(r.req, "Cookie", cookie.value.c_str(), 0), "Set account session");
            if (loginBody) check(api.addHeader(r.req, "Content-Type", "application/x-www-form-urlencoded", 0), "Set sign-in content type");
            if (probeRange) check(api.addHeader(r.req, "Range", "bytes=0-3", 0), "Set package range");

            const int sent = api.send(r.req, loginBody ? loginBody->data() : nullptr, loginBody ? loginBody->size() : 0);
            if (sent < 0) {
                const size_t begin = current.find("://") + 3;
                const size_t end = current.find_first_of("/?", begin);
                const std::string host = current.substr(begin, end - begin).substr(0, 128);

                // Only query SSL errors if the failure was specifically an SSL handshake failure
                const bool isSslError = (static_cast<uint32_t>(sent) == 0x8095F00Cu);

                const std::string action = std::string(isSslError
                    ? "HTTPS certificate not trusted for " : "Send request to ") + host +
                    " (redirect " + std::to_string(redirects) + ")";

                const auto details = nativeRequestDiagnostics(net, r.req, current.compare(0, 8, "https://") == 0 && isSslError);

                throw std::runtime_error(action + " (" + nativeHex(sent) + ");" + details);
            }

            HttpTransferInfo info;
            info.effectiveUrl = current;
            check(api.getStatus(r.req, &info.status), "HTTP status");
            if (isHttpRedirect(info.status)) {
                if (loginBody) throw std::runtime_error("Internet Archive sign-in redirects are not allowed");
                current = nextHttpUrl(current, requestLocation(api, r.req), redirects);
                api.abort(r.req);
                continue;
            }
            if (info.status != 200 && !(probeRange && info.status == 206))
                throw std::runtime_error(statusFailure(label, info.status, !cookie.value.empty()));
            if (!document) {
                int type = -1; size_t length = 0;
                check(api.contentLength(r.req, &type, &length), "HTTP content length");
                if (type == ORBIS_HTTP_CONTENTLEN_EXIST) info.length = length;
            }
            std::vector<char> block(64 * 1024);
            uint64_t nextWake = 0;
            for (;;) {
                if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
                const uint64_t now = sceKernelGetProcessTime();
                if (document && now > deadline) throw std::runtime_error("Catalog download timed out");
                if (!document && now >= nextWake) { sceSystemServicePowerTick(); nextWake = now + 10000000; }
                const int n = api.read(r.req, block.data(), block.size()); check(n, "Read HTTP response");
                if (!n) break;
                if (!sink(block.data(), n, info.length)) { api.abort(r.req); break; }
            }
            return info;
        } catch (...) { api.abort(r.req); throw; }
    }
}
}
#endif

namespace psgi {
HttpTransferInfo httpTransfer(const std::string& url, const HttpSink& sink, bool probeRange,
                             const std::atomic<bool>* cancelled) {
    if (!isHttpUrl(url)) throw std::runtime_error("Invalid package URL");
    if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
    return performTransfer(url, sink, probeRange, cancelled, false, false);
}
HttpTransferInfo httpTransferAuthenticated(const std::string& url, const HttpSink& sink, bool probeRange,
                                          const ArchiveSession& session, const std::atomic<bool>* cancelled) {
    if (!isHttpUrl(url)) throw std::runtime_error("Invalid package URL");
    if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
    return performTransfer(url, sink, probeRange, cancelled, false, false, &session);
}
ArchiveSession archiveLogin(const std::string& email, const std::string& password,
                            const std::atomic<bool>* cancelled) {
    if (email.empty() || password.empty() || email.size() > 512 || password.size() > 4096 ||
        email.find('\0') != std::string::npos || password.find('\0') != std::string::npos)
        throw std::runtime_error("Enter your Internet Archive email and password.");
    if (cancelled && *cancelled) throw std::runtime_error("Internet Archive sign-in cancelled");
    SensitiveText form, reply;
    form.value.reserve(16 + 3 * (email.size() + password.size()));
    form.value = "email="; appendFormValue(form.value, email);
    form.value += "&password="; appendFormValue(form.value, password);
    try {
        performTransfer("https://archive.org/services/xauthn/?op=login",
            [&](const void* data, size_t size, uint64_t) {
                if (size > 64 * 1024 - reply.value.size()) throw std::runtime_error("Sign-in reply exceeds limit");
                reply.value.append(static_cast<const char*>(data), size); return true;
            }, false, cancelled, false, true, nullptr, &form.value);
        return parseArchiveLogin(reply.value);
    } catch (const std::exception&) {
        if (cancelled && *cancelled) throw std::runtime_error("Internet Archive sign-in cancelled");
        // Server replies can contain credentials or account details. Never expose
        // their text through the UI, persisted queue messages, or application log.
        throw std::runtime_error("Internet Archive sign-in failed. Check your credentials, account access and connection.");
    }
}
static std::string getDocument(const std::string& url, size_t limit, const std::atomic<bool>* cancelled, bool artwork) {
    if (!isHttpUrl(url)) throw std::runtime_error("Invalid catalog URL");
    if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
    std::string out;
    performTransfer(url, [&](const void* data, size_t size, uint64_t) {
        if (size > limit - out.size()) throw std::runtime_error("Catalog download failed: Download exceeds limit");
        out.append(static_cast<const char*>(data), size);
        return true;
    }, false, cancelled, artwork, true);
    return out;
}
std::string httpGet(const std::string& url, size_t limit, const std::atomic<bool>* cancelled) {
    return getDocument(url, limit, cancelled, false);
}
std::string httpGetArtwork(const std::string& url, size_t limit, const std::atomic<bool>* cancelled) {
    return getDocument(url, limit, cancelled, true);
}
}
