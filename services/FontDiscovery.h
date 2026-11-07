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
