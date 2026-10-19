#pragma once
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace psgi {

// Split the packaged CA bundle into complete PEM objects for sceHttpsLoadCert.
// Native SSL validates the certificates; this only checks bounded PEM framing.
inline std::vector<std::string> parseTrustedCertificates(const std::string& bundle) {
    if (bundle.empty() || bundle.size() >= 1024 * 1024 || bundle.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid HTTPS CA bundle size or encoding");
    std::string normalized;
    normalized.reserve(bundle.size());
    for (char c : bundle) if (c != '\r') normalized += c;
    std::vector<std::string> certificates;
    std::string certificate;
    bool payload = false;
    for (size_t position = 0; position < normalized.size();) {
        const auto end = normalized.find('\n', position);
        const auto line = normalized.substr(position, end == std::string::npos ? end : end - position);
        position = end == std::string::npos ? normalized.size() : end + 1;
        if (line == "-----BEGIN CERTIFICATE-----") {
            if (!certificate.empty() || certificates.size() == 256)
                throw std::runtime_error("Invalid HTTPS CA certificate boundary or count");
            certificate = line + "\r\n";
            payload = false;
        } else if (line == "-----END CERTIFICATE-----") {
            if (certificate.empty() || !payload)
                throw std::runtime_error("Empty or unexpected HTTPS CA certificate");
            certificate += line + "\r\n";
            certificates.push_back(std::move(certificate));
            certificate.clear();
        } else {
            if (line.find("-----BEGIN") != std::string::npos || line.find("-----END") != std::string::npos ||
                line.find("BEGIN CERTIFICATE") != std::string::npos || line.find("END CERTIFICATE") != std::string::npos)
                throw std::runtime_error("Malformed HTTPS CA certificate boundary");
            if (certificate.empty()) continue; // Mozilla comments and certificate names.
            for (char c : line) {
                if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                      (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '='))
                    throw std::runtime_error("Invalid HTTPS CA certificate encoding");
            }
            payload = payload || !line.empty();
            certificate += line + "\r\n";
        }
    }
    if (!certificate.empty() || certificates.empty())
        throw std::runtime_error("Incomplete or empty HTTPS CA bundle");
    return certificates;
}

} // namespace psgi
