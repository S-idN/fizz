#pragma once

#include <string>
#include <vector>

constexpr float kBaseFontSize = 16.0f;

// Generates the ordered font candidate paths across major Linux distributions
// (Fedora, Debian/Ubuntu, Arch Linux) for standard fonts:
// Adwaita Sans, Cantarell, Google Sans, DejaVu Sans, and JetBrains Mono.
std::vector<std::string> getDefaultFontCandidates(const std::string& sysRoot = "");

// Resolves a readable system font file path matching the cascade.
// sysRoot allows inspecting alternative or sandboxed root filesystem directories.
// Returns empty string if no candidate exists or is readable.
std::string resolveSystemFont(const std::string& sysRoot = "");

// Represents a discovered system font choice
struct DiscoveredFont {
    std::string familyName; // e.g. "System Default (Auto-detect)", "Adwaita Sans", etc.
    std::string path;       // Resolved readable file path, or empty if default / unavailable
    bool available = false;
};

// Discovers standard system font families available on the platform
std::vector<DiscoveredFont> getDiscoveredSystemFonts(const std::string& sysRoot = "");
