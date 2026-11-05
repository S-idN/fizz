#pragma once

#include <string>
#include <vector>
#include <functional>

constexpr float kBaseFontSize = 16.0f;

// Generates the ordered font candidate paths across major Linux distributions
// (Fedora, Debian/Ubuntu, Arch Linux) for standard fonts:
// Adwaita Sans, Cantarell, Google Sans, DejaVu Sans, and JetBrains Mono.
std::vector<std::string> getDefaultFontCandidates(const std::string& sysRoot = "");

// Resolves a readable system font file path matching the cascade.
// If customCandidates is non-empty, probes those instead of the default candidates.
// fileChecker allows dependency injection for tests (defaults to disk read check).
// Returns empty string if no candidate exists or is readable.
std::string resolveSystemFont(
    const std::string& sysRoot = "",
    const std::vector<std::string>& customCandidates = {},
    const std::function<bool(const std::string&)>& fileChecker = nullptr
);
