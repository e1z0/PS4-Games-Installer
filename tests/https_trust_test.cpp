#include "https_trust.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>

namespace {
const std::string certificate = "-----BEGIN CERTIFICATE-----\nQQ==\n-----END CERTIFICATE-----\n";
const std::string normalized = "-----BEGIN CERTIFICATE-----\r\nQQ==\r\n-----END CERTIFICATE-----\r\n";
void rejects(const std::string& input) {
    bool failed = false;
    try { psgi::parseTrustedCertificates(input); }
    catch (const std::runtime_error&) { failed = true; }
    assert(failed);
}
}

int main() {
    assert(psgi::parseTrustedCertificates(certificate) == std::vector<std::string>{normalized});
    assert(psgi::parseTrustedCertificates(normalized) == std::vector<std::string>{normalized});
    assert(psgi::parseTrustedCertificates(certificate.substr(0, certificate.size() - 1)) ==
           std::vector<std::string>{normalized});
    const auto pair = psgi::parseTrustedCertificates("# Mozilla CA bundle\nRoot CA\n=======\n" +
                                                    certificate + "\nAnother CA\n" + normalized);
    assert(pair == (std::vector<std::string>{normalized, normalized}));
    rejects("");
    rejects("# No roots\n");
    rejects(std::string(1024 * 1024, ' '));
    rejects(certificate + std::string("\0", 1));
    rejects("-----BEGIN CERTIFICATE-----\nQQ==\n");
    rejects("-----END CERTIFICATE-----\n");
    rejects("-----BEGIN CERTIFICATE-----\n-----END CERTIFICATE-----\n");
    rejects("-----BEGIN CERTIFICATE-----\n" + certificate);
    rejects("-----BEGIN CERTIFICATE-----\nQQ==\n-----END CERTIFICATE----\n");
    rejects("-----BEGIN CERTIFICATE----\nQQ==\n-----END CERTIFICATE-----\n");
    rejects("-----BEGIN PRIVATE KEY-----\nQQ==\n-----END PRIVATE KEY-----\n");
    rejects("-----BEGIN CERTIFICATE-----\ninvalid!\n-----END CERTIFICATE-----\n");
    rejects(certificate + "----BEGIN CERTIFICATE----\nQQ==\n----END CERTIFICATE----\n");
    rejects(certificate + "-----BEGIN CERTIFICATE-----\n");
    std::string many;
    for (size_t i = 0; i < 256; ++i) many += certificate;
    assert(psgi::parseTrustedCertificates(many).size() == 256);
    rejects(many + certificate);

    std::ifstream input("assets/cacert.pem", std::ios::binary);
    assert(input);
    const std::string bundle{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const auto roots = psgi::parseTrustedCertificates(bundle);
    assert(roots.size() == 121);
    for (const auto& root : roots) {
        assert(root.compare(0, 29, "-----BEGIN CERTIFICATE-----\r\n") == 0);
        assert(root.size() > 100 && root.substr(root.size() - 27) == "-----END CERTIFICATE-----\r\n");
        for (size_t i = 0; i < root.size(); ++i)
            if (root[i] == '\n') assert(i > 0 && root[i - 1] == '\r');
    }
    std::puts("HTTPS CA bundle framing, limits, normalization and packaged roots passed");
}
